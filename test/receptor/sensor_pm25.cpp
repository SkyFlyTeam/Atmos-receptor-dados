#include <WiFi.h>
#include <PubSubClient.h> // Biblioteca para MQTT
#include "time.h"
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// NOVO BLOCO (Requisito: método genérico replicável + PMSX003 datasheet)
// ---------------------------------------------------------------------------
static const uint32_t PMSX003_BAUD = 9600;              // Taxa indicada no datasheet
static const uint8_t PMSX003_FRAME_LENGTH = 32;         // Pacote completo = 32 bytes
static const uint16_t PMSX003_HEADER = 0x424D;          // Palavra de início "BM"
static const uint32_t PMSX003_WARMUP_MS = 5000;         // Tempo para estabilizar o laser
static const uint32_t PMSX003_SAMPLE_PERIOD_MS = 1000;  // Amostragem 1 Hz (tempo real)
static const uint8_t MAX_SENSOR_FRAME_BYTES = 64;       // Permite reaproveitar com outros sensores
static const int PMSX003_RX_PIN = 16;                   // Ajuste conforme hardware
static const int PMSX003_TX_PIN = 17;                   // Ajuste conforme hardware

typedef struct {
  float factor;
  float offset;
} SensorCalibration_t;

typedef struct {
  uint32_t timestampMs;
  uint16_t massConcentration[3]; // PM1.0 / PM2.5 / PM10
  uint16_t particleCount[6];     // 0.3–10 µm
  float temperature;
  float humidity;
  bool valid;
} SensorFrame_t;

typedef struct {
  float massConcentration[3];
  float particleCount[6];
  uint32_t timestampMs;
  bool valid;
} SensorSample_t;

typedef bool (*SensorParser_t)(const uint8_t* buffer, uint16_t len, SensorFrame_t& frame);

typedef struct {
  const char* sensorId;
  HardwareSerial* serial;
  SensorParser_t parser;
  SensorCalibration_t massCal[3];
  SensorCalibration_t countCal[6];
  uint16_t frameLength;
  uint32_t samplePeriodMs;
} SensorDescriptor_t;

// Prototipos das rotinas novas
bool parsePmsx003Frame(const uint8_t* buffer, uint16_t len, SensorFrame_t& frame);
bool receiveSensorFrame(const SensorDescriptor_t& descriptor, SensorFrame_t& frame);
SensorSample_t convertWithCalibration(const SensorDescriptor_t& descriptor, const SensorFrame_t& frame);
void tCapturaPM(void* pvParameters);

// --- Variaveis Globais para FreeRTOS, Dados e Configuracao ---
TaskHandle_t taskColeta;
TaskHandle_t taskMonWiFi;
TaskHandle_t taskSensorPM; // NOVO

// Semaforo Mutex para proteger o acesso a estrutura 'med'
SemaphoreHandle_t mutex;
SemaphoreHandle_t sensorMutex; // NOVO - protege dados do sensor PM

typedef struct {
  float temp;
  float umi;
  float dirvento;
  float velvento;
  float pressao;
} Medidas_t;

Medidas_t med;
SensorSample_t pmSample; // NOVO - último valor convertido do sensor PM

HardwareSerial PMSerial(2); // NOVO - Serial exclusiva para o PMSX003-N

SensorDescriptor_t pmsx003Descriptor = { // NOVO - descritor genérico do sensor
  "PMSX003-N",
  &PMSerial,
  parsePmsx003Frame,
  { {1.0f, 0.0f}, {1.02f, -1.5f}, {0.98f, 0.5f} }, // fatores/offsets de massa
  { {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f},
    {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f} },    // fatores/offsets contagem
  PMSX003_FRAME_LENGTH,
  PMSX003_SAMPLE_PERIOD_MS
};

// Buffer JSON para o POST/MQTT
DynamicJsonDocument post(1024);
// ID do Cliente MQTT (DEVE SER UNICO)
String mqttClientId = "ESP32_Estacao_" + String(random(0xFFFF), HEX); 
String uid = "ESTACAO_001_A0B1C2D3"; // UID simulado

// Configs do Wi-Fi
const char* ssid = "A25 de Guilherme";
const char* pwd = "19guilherme96!";

// Configs do Servidor MQTT
const char* mqtt_server = "fogueira-magica.com";
const int mqtt_port = 1883;
const char* mqtt_topic = "/fatec/dados/teste"; // Topico de publicacao

// Configs do servidor NTP
const char* ntpServer = "br.pool.ntp.org";
long gmtOffset_sec = -3 * 3600; // -3 horas em segundos (GMT-3)
int daylight_sec = 0;

time_t now;
struct tm timeinfo;

// Variaveis de controle para o loop
const long INTERVALO_ENVIO_MS = 5000; 
unsigned long proxEnvio = 0; // Proximo tempo de envio em milissegundos

// Cliente Wi-Fi e Cliente MQTT
WiFiClient wclient;
PubSubClient mqttClient(wclient); 

// ----------------------------------------------------------------------------------

/**
 * @brief Task de Coleta de Dados (Simulada)
 * Roda no Core 0. Coleta dados a cada 1 segundo.
 */
void tColeta(void *pvParameters)
{
  Serial.println(">>> Task de Coleta de Dados Iniciada (Core 0)");
  while(true){
    // Regiao critica: Acesso e modificacao da estrutura 'med'
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      // Simulacao da leitura dos sensores
      med.temp     = random(100, 400) / 10.0;     // 10.0 a 40.0 C
      med.umi      = random(20, 99);             // 20 a 99 %
      med.dirvento = random(0, 359);             // 0 a 359 graus
      med.velvento = random(100, 1000) / 10.0;   // 10.0 a 100.0 km/h
      med.pressao  = random(100000, 101325) / 100.0; // 1000.00 a 1013.25 hPa
      xSemaphoreGive(mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Espera 1 segundo
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
  // Aumentamos o tempo limite para 30s (60 * 500ms) para redes mais lentas
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
 * Roda no Core 1. Verifica a conexao Wi-Fi a cada 30 segundos.
 */
void tMonWiFi(void *pvParameters)
{
  Serial.println(">>> Task Monitor de Internet Iniciada (Core 1)");
  while(true){
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[FAIL] Wi-Fi desconectado. Tentando reconectar...");
      connectWiFi(); // Tenta reconectar
    }
    // A cada 30 segundos, verifica se a conexao Wi-Fi esta ativa
    vTaskDelay(pdMS_TO_TICKS(30000)); 
  }
}

// ---------------------------------------------------------------------------
// NOVO - Funções genéricas para captura/parse/calibração do PMSX003-N
// ---------------------------------------------------------------------------
static uint16_t readWord(const uint8_t* buffer, uint8_t index) {
  return static_cast<uint16_t>(buffer[index] << 8 | buffer[index + 1]);
}

bool parsePmsx003Frame(const uint8_t* buffer, uint16_t len, SensorFrame_t& frame)
{
  if (len != PMSX003_FRAME_LENGTH) {
    return false;
  }

  const uint16_t header = readWord(buffer, 0);
  if (header != PMSX003_HEADER) {
    return false;
  }

  const uint16_t frameLen = readWord(buffer, 2);
  if (frameLen != 28) { // valor documentado na datasheet
    return false;
  }

  frame.massConcentration[0] = readWord(buffer, 4);
  frame.massConcentration[1] = readWord(buffer, 6);
  frame.massConcentration[2] = readWord(buffer, 8);

  // Bytes 16..27 armazenam contagem acumulada por diâmetro
  frame.particleCount[0] = readWord(buffer, 16);
  frame.particleCount[1] = readWord(buffer, 18);
  frame.particleCount[2] = readWord(buffer, 20);
  frame.particleCount[3] = readWord(buffer, 22);
  frame.particleCount[4] = readWord(buffer, 24);
  frame.particleCount[5] = readWord(buffer, 26);

  frame.temperature = readWord(buffer, 28) / 10.0f;
  frame.humidity = readWord(buffer, 30) / 10.0f;

  uint16_t checksum = 0;
  for (uint8_t i = 0; i < PMSX003_FRAME_LENGTH - 2; ++i) {
    checksum += buffer[i];
  }

  frame.valid = (checksum == readWord(buffer, PMSX003_FRAME_LENGTH - 2));
  return frame.valid;
}

bool receiveSensorFrame(const SensorDescriptor_t& descriptor, SensorFrame_t& frame)
{
  if (!descriptor.serial) {
    return false;
  }

  if (descriptor.frameLength > MAX_SENSOR_FRAME_BYTES) {
    return false;
  }

  uint8_t buffer[MAX_SENSOR_FRAME_BYTES] = {0};
  const size_t bytesRead = descriptor.serial->readBytes(
    buffer,
    descriptor.frameLength
  );

  if (bytesRead != descriptor.frameLength) {
    return false;
  }

  frame.timestampMs = millis();
  return descriptor.parser(buffer, descriptor.frameLength, frame);
}

SensorSample_t convertWithCalibration(const SensorDescriptor_t& descriptor, const SensorFrame_t& frame)
{
  SensorSample_t sample = {};
  sample.valid = frame.valid;
  sample.timestampMs = frame.timestampMs;
  if (!frame.valid) {
    return sample;
  }

  for (uint8_t i = 0; i < 3; ++i) {
    sample.massConcentration[i] = frame.massConcentration[i] * descriptor.massCal[i].factor +
                                  descriptor.massCal[i].offset;
  }

  for (uint8_t i = 0; i < 6; ++i) {
    sample.particleCount[i] = frame.particleCount[i] * descriptor.countCal[i].factor +
                              descriptor.countCal[i].offset;
  }

  return sample;
}

void tCapturaPM(void* pvParameters)
{
  SensorDescriptor_t* descriptor = static_cast<SensorDescriptor_t*>(pvParameters);
  Serial.println(">>> Task Captura PMSX003-N Iniciada (Core 0)");

  // Aguarda estabilização do laser antes de coletar leituras
  vTaskDelay(pdMS_TO_TICKS(PMSX003_WARMUP_MS));

  while (true) {
    SensorFrame_t frame = {};
    if (receiveSensorFrame(*descriptor, frame)) {
      SensorSample_t sample = convertWithCalibration(*descriptor, frame);
      if (sensorMutex != NULL && xSemaphoreTake(sensorMutex, portMAX_DELAY) == pdTRUE) {
        pmSample = sample;
        xSemaphoreGive(sensorMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(descriptor->samplePeriodMs));
  }
}

/**
 * @brief Sincroniza a hora usando o servidor NTP.
 */
void sincronizaTempo(void)
{
  Serial.print("Sincronizando tempo via NTP...");
  configTime(gmtOffset_sec, daylight_sec, ntpServer);
  
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
  // Verifica se ha conexao Wi-Fi
  if (WiFi.status() != WL_CONNECTED) return;
  
  // Tenta conectar se nao estiver conectado
  if (!mqttClient.connected()) {
    Serial.print("Tentando conectar ao Broker MQTT...");
    
    // O Cliente ID e gerado no setup (deve ser unico)
    if (mqttClient.connect(mqttClientId.c_str())) {
      Serial.println("[OK] Conectado ao MQTT!");
      
      // Assinatura (MANTEMOS O TOPICO DO SEU EXEMPLO PARA DEMONSTRAR O CALLBACK)
      // Se voce nao for receber comandos, pode remover esta linha
      mqttClient.subscribe("fatec/labs/407/luz/"); 
    } else {
      Serial.print("[FAIL] Falha na conexao MQTT. Estado: ");
      Serial.print(mqttClient.state()); // Imprime o codigo de erro
      Serial.println(". Tentando novamente em 5s.");
      // Espera um pouco antes de tentar de novo
      vTaskDelay(pdMS_TO_TICKS(5000)); 
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
  
  // Exemplo de controle (pode ser ajustado ou removido se for so para envio)
  // if (msg.indexOf("on") >= 0) { 
  //   digitalWrite(LED, HIGH);
  // } else if (msg.indexOf("off") >= 0) {
  //   digitalWrite(LED, LOW);
  // }
}

// ----------------------------------------------------------------------------------

void setup() {
  // pinMode(LED, OUTPUT); // Se estiver usando LED
  // digitalWrite(LED, LOW); // Se estiver usando LED
  randomSeed(analogRead(0)); // Inicializa o gerador de numeros aleatorios
  Serial.begin(115200);
  Serial.println("\n--- Inicializacao do Sistema ESP32 MQTT ---");

  // 0. Configura o cliente MQTT ANTES de iniciar o Wi-Fi
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback); 
  
  // 1. Criacao do Mutex
  mutex = xSemaphoreCreateMutex();
  if (mutex == NULL) {
    Serial.println("Fatal Error: Erro ao criar o mutex");
    return;
  }
  sensorMutex = xSemaphoreCreateMutex(); // NOVO
  if (sensorMutex == NULL) {
    Serial.println("Fatal Error: Erro ao criar o mutex do sensor PM");
    return;
  }
  pmSample.valid = false; // NOVO - estado inicial

  // 2. Criacao da Task de Coleta (Core 0)
  xTaskCreatePinnedToCore(
    tColeta,        // Funcao da task
    "TaskColeta",   // Nome da task
    2048,           // Tamanho da stack 
    NULL,           // Parametros task
    2,              // Prioridade da task (Alta)
    &taskColeta,    // Task handle
    0               // Core (Processamento de dados)
  );

  // 2.1 Inicializacao da serial do sensor e task dedicada (NOVO)
  PMSerial.begin(PMSX003_BAUD, SERIAL_8N1, PMSX003_RX_PIN, PMSX003_TX_PIN);
  PMSerial.setTimeout(1000); // evita travas aguardando bytes
  xTaskCreatePinnedToCore(
    tCapturaPM,
    "TaskPMSX003",
    4096,
    (void*)&pmsx003Descriptor,
    2,
    &taskSensorPM,
    0
  );

  // 3. Inicializacao do Wi-Fi e Criacao da Task Monitor (Core 1)
  WiFi.begin(ssid, pwd); 

  xTaskCreatePinnedToCore(
    tMonWiFi,       // Funcao da task
    "MonitoraWiFi", // Nome da task
    4096,           // Stack AUMENTADA
    NULL,           // Parametros task
    1,              // Prioridade da task (Media)
    &taskMonWiFi,   // Task handle
    1               // Core (Comunicacao)
  );
  
  // 4. Conexao inicial e Sincronizacao do tempo
  connectWiFi(); 
  sincronizaTempo();
  
  // Define o primeiro envio para 5s apos o setup
  proxEnvio = millis() + 5000; 

  Serial.println("--- Setup Concluido. Iniciando Loop ---");
}

void loop() {
  // 1. Garante que o cliente MQTT esteja conectado
  if (!mqttClient.connected()){
    connectMqtt();
  }
  
  // 2. Processa trafego MQTT (obrigatorio para manter a conexao e receber mensagens)
  mqttClient.loop();

  // 3. Controle de tempo robusto para envios a cada INTERVALO_ENVIO_MS
  if (millis() >= proxEnvio) {
    
    Serial.println("\n-------------------------------------------");
    Serial.println("--- Inicio do Ciclo de Transmissao MQTT ---");
    
    // Atualiza a hora
    sincronizaTempo();

    // 4. Coleta dos dados (Regiao critica)
    String json_payload;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
      
      char timeBuffer[20]; 
      strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
      
      // Popula o objeto JSON
      post.clear();
      post["uid"] = uid;
      post["unixtime"] = time(&now);
      post["timestamp"] = timeBuffer;
      post["temp"] = med.temp;
      post["umi"] = med.umi;
      post["dirvento"] = med.dirvento;
      post["velvento"] = med.velvento;
      post["pressao"] = med.pressao;
      // --- NOVO: injeta dados calibrados do PMSX003 na mesma mensagem MQTT
      if (sensorMutex != NULL && xSemaphoreTake(sensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (pmSample.valid) {
          JsonObject pmObj = post.createNestedObject("pm_sensor");
          pmObj["id"] = pmsx003Descriptor.sensorId;
          pmObj["timestamp_ms"] = pmSample.timestampMs;
          pmObj["pm1_0"] = pmSample.massConcentration[0];
          pmObj["pm2_5"] = pmSample.massConcentration[1];
          pmObj["pm10"] = pmSample.massConcentration[2];
          pmObj["count_0_3"] = pmSample.particleCount[0];
          pmObj["count_0_5"] = pmSample.particleCount[1];
          pmObj["count_1_0"] = pmSample.particleCount[2];
          pmObj["count_2_5"] = pmSample.particleCount[3];
          pmObj["count_5_0"] = pmSample.particleCount[4];
          pmObj["count_10"] = pmSample.particleCount[5];
        }
        xSemaphoreGive(sensorMutex);
      }
      
      // Converte JSON para String
      serializeJson(post, json_payload);
      
      xSemaphoreGive(mutex);
    } else {
      // Linha 181: Mensagem simplificada ASCII
      Serial.println("Mutex error. Skip send."); 
      proxEnvio = millis() + 5000; 
      return; 
    }

    Serial.print("JSON a ser enviado: ");
    Serial.println(json_payload);
    
    // 5. Transmissao MQTT
    if (mqttClient.connected()) {
      Serial.print("Publicando no topico: ");
      Serial.println(mqtt_topic);
      
      // Publica a mensagem JSON
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
