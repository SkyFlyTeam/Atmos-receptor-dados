# ⚙️ Guia de Configuração e Segredos (Secrets)

Este projeto de firmware para ESP32 utiliza um método de configuração baseado em arquivos de cabeçalho C++ para gerenciar **credenciais sensíveis**, como senhas de Wi-Fi e endereços de servidores MQTT.

O objetivo é separar as informações sensíveis do código-fonte principal (`.ino`), tornando o projeto mais **seguro** e fácil de implantar em diferentes ambientes.


## 🛠️ Como Configurar

Para iniciar o projeto em seu ambiente:

1.  **Crie o arquivo de segredos real:**
    * Copie o arquivo `secrets.example.h`.
    * Renomeie a cópia para **`secrets.h`**.

2.  **Edite o `secrets.h`:**
    * Abra o novo arquivo `secrets.h`.
    * Substitua todos os valores de exemplo (`"SEU_WIFI_AQUI"`, etc.) pelas suas **credenciais e configurações reais**.

### Exemplo de `secrets.h` (Estrutura)

```cpp
#ifndef SECRETS_H
#define SECRETS_H

const char* SECRET_SSID = "SUA_REDE_WIFI_AQUI"; 
const char* SECRET_PWD = "SUA_SENHA_AQUI";   
const char* SECRET_MQTT_SERVER = "BROKER_SERVER";
const int SECRET_MQTT_PORT = 'PORT'; // [INT] porta do broker
const char* SRECET_MQTT_TOPIC = "BROKER_TOPIC";
const char* SECRET_NTP_SERVER = "br.pool.ntp.org";


#endif // SECRETS_H