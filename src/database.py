from pymongo import MongoClient

def get_collection(config):
    client = MongoClient(config["mongo_uri"])
    db = client[config["mongo_database"]]
    return db[config["mongo_collection"]]
