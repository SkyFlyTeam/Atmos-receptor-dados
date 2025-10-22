import json
import logging
import threading
import time
import paho.mqtt.client as mqtt

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)  

console_handler = logging.StreamHandler()
console_handler.setLevel(logging.DEBUG)  

formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
console_handler.setFormatter(formatter)

logger.addHandler(console_handler)

message_buffer = [] 
buffer_lock = threading.Lock() 

def _bulk_insert_worker(collection, interval_seconds=3):
    logger.info(f"Worker de inserção em massa iniciado. Verificando a cada {interval_seconds}s.")
    while True:
        time.sleep(interval_seconds)

        with buffer_lock:
            if not message_buffer:
                continue 
            
            data_to_insert = message_buffer[:] 
            message_buffer.clear()

        try:
            collection.insert_many(data_to_insert)
            logger.info(f"SUCESSO: Inseridos {len(data_to_insert)} novos registros no banco.")
        except Exception as e:
            logger.error(f"ERRO ao inserir dados em massa: {e}")

def start_mqtt(config, collection):
    worker_thread = threading.Thread(
        target=_bulk_insert_worker, 
        args=(collection, 3), 
        daemon=True
    )
    worker_thread.start()

    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            logger.info("Conectado ao Broker MQTT!")
            client.subscribe(config["mqtt_topic"])
            logger.info(f"Inscrito no tópico: {config['mqtt_topic']}")
        else:
            logger.error(f"Falha na conexão com MQTT, código: {rc}")

    def on_message(client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode("utf-8"))
            with buffer_lock:
                message_buffer.append(data)
        except json.JSONDecodeError:
            logger.warning(f"Mensagem recebida não é um JSON válido: {msg.payload}")
        except Exception as e:
            logger.error(f"Erro ao processar mensagem: {e}")

    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(config["mqtt_host"], 1883, 60)
        client.loop_forever() 
    except Exception as e:
        logger.critical(f"Não foi possível conectar ao broker MQTT: {e}")