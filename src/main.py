from mqtt_client import start_mqtt
from database import get_collection
from config import load_config

def main():
    config = load_config()
    collection = get_collection(config)
    start_mqtt(config, collection)

if __name__ == "__main__":
    main()
