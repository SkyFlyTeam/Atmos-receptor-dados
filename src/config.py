import os
from dotenv import load_dotenv

def load_config():
    load_dotenv()
    config = {
        "mongo_uri": os.getenv("MONGO_URI"),
        "mongo_database": os.getenv("MONGO_DATABASE"),
        "mongo_collection": os.getenv("MONGO_COLLECTION"),
        "mqtt_topic": os.getenv("MQTT_TOPIC"),
        "mqtt_host": os.getenv("MQTT_HOST")
    }
    missing = [k for k, v in config.items() if not v]
    if missing:
        raise ValueError(f"Missing environment variables: {missing}")
    return config
