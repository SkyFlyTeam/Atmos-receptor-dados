# Simulador de Sensores MQTT

Este diretório contém o simulador de sensores para envio de dados via MQTT.

## Configuração

Antes de executar o simulador, configure as seguintes variáveis de ambiente:

### Descrição das Variáveis

- `MQTT_HOST`: Endereço do broker MQTT
- `MQTT_PORT`: Porta do broker MQTT
- `MQTT_TOPIC`: Tópico base para publicação das mensagens
- `TOTAL_VIRTUAL_SENSORS`: Número total de sensores virtuais a serem simulados
- `MESSAGES_PER_BURST`: Quantidade de mensagens enviadas em cada lote
- `DELAY_BETWEEN_BURSTS`: Atraso (em segundos) entre os lotes de mensagens
- `DELAY_IN_BURST`: Atraso (em segundos) entre mensagens dentro de um mesmo lote

## Como Executar

1. Certifique-se de ter Python 3.7+ instalado
2. Instale as dependências:
   ```bash
   pip install paho-mqtt
   ```
3. Execute o simulador:
   ```bash
   python simulador.py
   ```

## Dicas

- Ajuste as variáveis de ambiente conforme necessário para seu caso de uso
- Para simular mais sensores, aumente o valor de `TOTAL_VIRTUAL_SENSORS`
- Para aumentar/diminuir a taxa de mensagens, ajuste `DELAY_BETWEEN_BURSTS` e `DELAY_IN_BURST`
