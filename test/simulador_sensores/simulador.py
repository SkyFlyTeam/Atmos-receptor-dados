import paho.mqtt.client as mqtt
import time
import json
import random
import uuid
from dotenv import load_dotenv
from sensores import *
import os
load_dotenv()

config = {
    "mqtt_host": os.getenv("MQTT_HOST"),
    "mqtt_port": int(os.getenv("MQTT_PORT")),
    "mqtt_topic": os.getenv("MQTT_TOPIC"),
    "total_virtual_sensors": int(os.getenv("TOTAL_VIRTUAL_SENSORS")),
    "messages_per_burst": int(os.getenv("MESSAGES_PER_BURST")),
    "delay_between_bursts": int(os.getenv("DELAY_BETWEEN_BURSTS")),
    "delay_in_burst": float(os.getenv("DELAY_IN_BURST"))
}

print("Iniciando simulador de sensores")

# 1. Criar sensores virtuais
virtual_sensors = [create_sensor(i) for i in range(config['total_virtual_sensors'])]
print(f"{len(virtual_sensors)} sensores virtuais criados.")

# 2. Conectar ao MQTT Broker
client = mqtt.Client(client_id=f"simulador-burst-{uuid.uuid4()}")
try:
    client.connect(config['mqtt_host'], config['mqtt_port'], 60)
    client.loop_start() # Usa loop em background para gerenciar a conexão
except Exception as e:
    print(f"Erro ao conectar ao MQTT: {e}")
    exit()

print(f"Conectado a {config['mqtt_host']} e pronto para enviar dados.")

try:
    while True:
        print(f"\n--- Preparando para enviar {config['messages_per_burst']} mensagens ---")
        
        # 3. Loop para enviar mensagens
        for i in range(config['messages_per_burst']):
            # Escolhe um sensor aleatório
            random_sensor = random.choice(virtual_sensors)
            
            # Gera um novo conjunto de dados
            payload = generate_payload(random_sensor)
            
            # Converte para JSON e publica
            msg = json.dumps(payload)
            client.publish(config['mqtt_topic'], msg)
                        
            # Espera um tempo mínimo entre as mensagens
            time.sleep(config['delay_in_burst'])
            
        print(f"--- Mensagens enviadas. ---")
        
        # 4. Espera para a próxima 
        time.sleep(config['delay_between_bursts'])

except KeyboardInterrupt:
    print("\nSimulador interrompido pelo usuário.")
finally:
    client.loop_stop()
    client.disconnect()
    print("Desconectado do MQTT Broker.")