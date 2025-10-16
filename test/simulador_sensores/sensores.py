import random
import time

def create_sensor(sensor_id):
    """Cria a configuração para um único sensor virtual."""
    sensor_type = random.choice(['pluviometro', 'qualidade_ar', 'solo'])
    return {"id": sensor_id, "type": sensor_type}

def generate_payload(sensor):
    """Gera uma leitura de dados aleatória baseada no tipo do sensor."""
    payload = {
        "UUID": f"{sensor['type'].upper()}-{sensor['id']}",
        "unixtime": int(time.time())
    }
    
    if sensor['type'] == 'pluviometro':
        payload.update({
            "plu": random.randint(0, 5),      # nível de chuva (mm)
            "umi": random.randint(60, 98),    # umidade (%)
            "tem": round(random.uniform(18.0, 35.0), 2) # temperatura (°C)
        })
    elif sensor['type'] == 'qualidade_ar':
        payload.update({
            "co2": random.randint(300, 1000), # concentração de CO₂ (ppm)
            "voc": random.randint(0, 500),    # compostos orgânicos voláteis (ppb)
            "pm25": round(random.uniform(0, 50), 2) # partículas finas PM2.5 (µg/m³)
        })
    elif sensor['type'] == 'solo':
        payload.update({
            "hum": random.randint(10, 90),         # umidade do solo (%)
            "ph": round(random.uniform(5.5, 7.5), 2), # pH do solo
            "tmp": round(random.uniform(15.0, 35.0), 2) # temperatura do solo (°C)
        })
        
    return payload