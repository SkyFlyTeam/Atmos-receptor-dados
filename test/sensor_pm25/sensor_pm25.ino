#include <WiFi.h>
#include <PubSubClient.h> 
#include "time.h"
#include <ArduinoJson.h>
#include <PMS.h> 
#include "secrets.h" /

// ----------------------------------------------------------------------------------
// --- DEFINIÇÃO DE ESTRUTURAS ---
// ----------------------------------------------------------------------------------

/**
 * @brief Estrutura para armazenar as medições do sensor (já calibradas).
 */
typedef struct {
  float pm25; // Concentração de PM2.5 em µg/m³
} Medidas_t;

/**
 * @brief Estrutura para armazenar as regras de calibração.
 */
struct Regra {
  float fator;
  float offset;
};

// ----------------------------------------------------------------------------------
// --- Regras de Conversão (Fator e Offset) ---
// ----------------------------------------------------------------------------------

// Variáveis constantes para calibração/ajuste dos dados
const Regra REGRA_PM25 = { .fator = 1, .offset = 0 }; 

/**
 * @brief Aplica o fator e o offset para corrigir/calibrar o valor lido.
 */
float converterValor(float valor_lido, const Regra& regra) {
  return (valor_lido * regra.fator) + regra.offset;
}

// ----------------------------------------------------------------------------------
// --- Variaveis Globais e Configuração de Hardware ---
// ----------------------------------------------------------------------------------
TaskHandle_t taskColeta;
TaskHandle_t taskMonWiFi;
SemaphoreHandle_t mutex;

Medidas_t med;
DynamicJsonDocument post(1024); 

String mqttClientId; 
String uid; // Armazena o MAC Address

// Configs do Wi-Fi (USANDO SECRETS.H)
const char* ssid = SECRET_SSID; 
const char* pwd = SECRET_PWD;

// Configs do Servidor MQTT (USANDO SECRETS.H)
const char* mqtt_server = SECRET_MQTT_SERVER;
const int mqtt_port = SECRET_MQTT_PORT;
const char* mqtt_topic = SRECET_MQTT_TOPIC; // Tópico é público, pode ficar aqui

// Configs do servidor NTP (USANDO SECRETS.H)
const char* ntpServer = SECRET_NTP_SERVER;
long gmtOffset_sec = -3 * 3600; 
int daylight_sec = 0;

time_t now;
struct tm timeinfo;

const long INTERVALO_ENVIO_MS = 5000; 
unsigned long proxEnvio = 0; 

WiFiClient wclient;
PubSubClient mqttClient(wclient); 

// --- Configuração dos Pinos do Sensor PMSX003 ---
#define PMS_RX_PIN 23 // RX do ESP32 
#define PMS_TX_PIN 19 // TX do ESP32 

PMS pms(Serial2);
PMS::DATA pmsData; 

/**
 * @brief Task de Coleta de Dados (Leitura Real do Sensor)
 * Roda no Core 0. Coleta dados, aplica conversão e armazena.
 */
void tColeta(void *pvParameters)
{
  Serial.println(">>> Task de Coleta de Dados Iniciada (Core 0)");
  
  while(true){
    
    if (pms.readUntil(pmsData, 100)) { 
      
      if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        float raw_pm25 = pmsData.PM_AE_UG_2_5; 
        med.pm25  = converterValor(raw_pm25, REGRA_PM25); 
        
        xSemaphoreGive(mutex);
      }
    } else {
      Serial.println("[WARN] Falha ao ler o sensor PMS (pms.readUntil() falhou).");
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // Espera 1 segundo para a próxima leitura
  }
}

/**
 * @brief Tenta conectar o ESP32 ao Wi-Fi com tempo limite e diagnostico.
 */
void connectWiFi()
{
  Serial.print("Conectando o WiFI ");
  WiFi.begin(ssid, pwd);
  
  int cont = 0;
  while(WiFi.status() != WL_CONNECTED && cont < 60)
  {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    cont++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("[OK] Conectado com sucesso. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.print("[FAIL] Falha na conexao apos tempo limite. Status do Wi-Fi: ");
    Serial.println(WiFi.status()); 
  }
}

/**
 * @brief Task Monitor de Internet
 */
void tMonWiFi(void *pvParameters)
{
  Serial.println(">>> Task Monitor de Internet Iniciada (Core 1)");
  while(true){
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[FAIL] Wi-Fi desconectado. Tentando reconectar...");
      connectWiFi(); // Tenta reconectar
    }
    vTaskDelay(pdMS_TO_TICKS(30000)); 
  }
}

/**
 * @brief Sincroniza a hora usando o servidor NTP.
 */
void sincronizaTempo(void)
{
  Serial.print("Sincronizando tempo via NTP...");
  configTime(gmtOffset_sec, daylight_sec, ntpServer);
  
  // Tenta obter a hora com timeout de 10 segundos
  if (!getLocalTime(&timeinfo, 10000)) 
  {
    Serial.println("[FAIL] Erro ao acessar o servidor NTP."); 
  }
  else
  {
    Serial.print("[OK] Data/Hora Configurada: ");
    Serial.println(&timeinfo, "%A, %d de %B de %Y %H:%M:%S");
  }
}

/**
 * @brief Tenta conectar ou reconectar ao Broker MQTT.
 */
void connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  if (!mqttClient.connected()) {
    Serial.print("Tentando conectar ao Broker MQTT...");
    
    if (mqttClient.connect(mqttClientId.c_str())) {
      Serial.println("[OK] Conectado ao MQTT!");
      
      mqttClient.subscribe(mqtt_topic); 
    } else {
      Serial.print("[FAIL] Falha na conexao MQTT. Estado: ");
      Serial.print(mqttClient.state()); 
      Serial.println(". Tentando novamente em 5s.");
    }
  }
}

/**
 * @brief Funcao Callback para mensagens MQTT recebidas (Assinatura)
 */
void callback(char* topic, byte* message, unsigned int length){
  String msg;
  Serial.print("Mensagem recebida no topico: ");
  Serial.print(topic);
  Serial.print(" -> ");
  
  for (int i = 0; i<length; i++){
    msg += (char)message[i];
  }
  Serial.println(msg); 
}

/**
 * @brief Monta e serializa o JSON payload com base nas medidas e informações da estação.
 */
String montarJsonPayload(const Medidas_t& dados, const struct tm& timeinfo, const String& station_uid) {
    post.clear();
    
    char timeBuffer[20]; 
    strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // Popula o objeto JSON
    post["uid"] = station_uid;
    post["unixtime"] = time(&now);
    post["timestamp"] = timeBuffer;
    post["pm25"] = dados.pm25;
    
    String json_payload;
    serializeJson(post, json_payload);
    
    return json_payload;
}

void setup() {
  randomSeed(analogRead(0)); 
  Serial.begin(115200);
  Serial.println("\n--- Inicializacao do Sistema ESP32 MQTT PMS ---");

  // Inicia a comunicação Serial2 com o sensor PMS
  Serial2.begin(9600, SERIAL_8N1, PMS_RX_PIN, PMS_TX_PIN);
  Serial.println("[OK] Serial2 para PMS inicializada nos pinos RX=23 e TX=19.");

  // 0. Configura o cliente MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback); 
  
  // 1. Criacao do Mutex
  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    Serial.println("Fatal Error: Erro ao criar o mutex");
    return;
  }

  // 2. Criacao da Task de Coleta (Core 0 - Leitura de Hardware)
  xTaskCreatePinnedToCore(
    tColeta,        
    "TaskColeta",   
    4096,           
    NULL,           
    2,              
    &taskColeta,    
    0               
  );

  // 3. Inicializacao do Wi-Fi e Criacao da Task Monitor (Core 1 - Comunicação)
  WiFi.begin(ssid, pwd); 

  xTaskCreatePinnedToCore(
    tMonWiFi,       
    "MonitoraWiFi", 
    4096,           
    NULL,           
    1,              
    &taskMonWiFi,   
    1               
  );
  
  // 4. Conexao inicial, Obtenção do MAC e Sincronizacao do tempo
  connectWiFi(); 
  
  // BLOCO PARA OBTER O MAC ADDRESS (usado como UID)
  if (WiFi.status() == WL_CONNECTED) {
    uid = WiFi.macAddress(); 
    uid.replace(":", ""); // Remove os dois pontos do MAC
    mqttClientId = "PM25_" + uid;
    
    Serial.print("[INFO] UID da Estacao (MAC): ");
    Serial.println(uid);
    Serial.print("[INFO] Client ID MQTT: ");
    Serial.println(mqttClientId);
  } else {
    Serial.println("[WARN] Falha ao obter MAC/UID. Usando valor padrao...");
    uid = "MAC_NOT_FOUND";
    mqttClientId = "PM25_Default";
  }
  
  sincronizaTempo();
  
  proxEnvio = millis() + 5000; // Começa o ciclo de envio 5s após o setup

  Serial.println("--- Setup Concluido. Iniciando Loop ---");
}

void loop() {
  // 1. Garante que o cliente MQTT esteja conectado
  if (!mqttClient.connected()){
    connectMqtt();
  }
  
  // 2. Processa trafego MQTT (mantém a conexão viva e recebe mensagens)
  mqttClient.loop();

  // 3. Controle de tempo robusto para envios a cada INTERVALO_ENVIO_MS
  if (millis() >= proxEnvio) {
    
    Serial.println("\n-------------------------------------------");
    Serial.println("--- Inicio do Ciclo de Transmissao MQTT ---");
    
    // Sincroniza o tempo antes de cada envio para garantir o timestamp correto
    sincronizaTempo();

    // 4. Montagem do JSON 
    String json_payload;
    
    // Trava o Mutex para leitura segura
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(100)) == pdTRUE) { // Espera max 100ms
      
      // CHAMADA DA FUNÇÃO QUE MONTA O JSON
      json_payload = montarJsonPayload(med, timeinfo, uid);
      
      xSemaphoreGive(mutex);
    } else {
      Serial.println("Mutex error. Skip send. A tarefa de coleta pode estar travada."); 
      proxEnvio = millis() + 5000; 
      return; 
    }

    Serial.print("JSON a ser enviado: ");
    Serial.println(json_payload);
    
    // 5. Transmissao MQTT
    if (mqttClient.connected()) {
      Serial.print("Publicando no topico: ");
      Serial.println(mqtt_topic);
      
      if (mqttClient.publish(mqtt_topic, json_payload.c_str())) {
        Serial.println("[OK] Publicacao MQTT SUCESSO.");
      } else {
        Serial.print("[FAIL] Publicacao MQTT FALHOU. Codigo: ");
        Serial.println(mqttClient.state());
      }
    } else {
      Serial.println("[FAIL] Nao e possivel publicar: Cliente MQTT desconectado.");
    }

    // Atualiza o tempo para o proximo envio
    proxEnvio = millis() + INTERVALO_ENVIO_MS;
    Serial.println("--- Fim do Ciclo de Transmissao MQTT ---");
    Serial.println("-------------------------------------------");
  }
  
  delay(10); 
}