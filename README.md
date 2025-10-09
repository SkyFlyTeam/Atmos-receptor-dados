# Atmos Receptor de Dados

Sistema receptor de dados de sensores via MQTT que armazena informações em MongoDB. Este aplicativo recebe dados JSON de sensores através de um broker MQTT e os armazena em lotes no MongoDB para otimizar a performance.

## 📋 Pré-requisitos

- Python 3.9 ou superior
- MongoDB Atlas (nuvem) ou outro MongoDB em nuvem
- Broker MQTT em nuvem (ex: HiveMQ Cloud, AWS IoT, etc.)

## 🚀 Instalação e Configuração

### ⚡ Início Rápido (Docker)

```bash
# Clone o repositório
git clone <url-do-repositorio>
cd Atmos-receptor-dados

# Configure suas credenciais de nuvem
cp env.example .env
# Edite o arquivo .env com suas credenciais

# Execute a aplicação
docker-compose up -d

# Pronto! A aplicação está rodando
```

### Opção 1: Execução com Docker (Recomendado)

#### 1. Clone o repositório
```bash
git clone <url-do-repositorio>
cd Atmos-receptor-dados
```

#### 2. Execute com Docker Compose
```bash
# Inicia todos os serviços (MongoDB, MQTT e aplicação)
docker-compose up -d

# Para ver os logs em tempo real
docker-compose logs -f atmos-app

# Para parar todos os serviços
docker-compose down
```

#### 3. Verificar se está funcionando
```bash
# Verificar status dos containers
docker-compose ps

# Ver logs da aplicação
docker-compose logs atmos-app
```

### Opção 2: Execução Manual (Desenvolvimento)

#### 1. Clone o repositório
```bash
git clone <url-do-repositorio>
cd Atmos-receptor-dados
```

#### 2. Crie um ambiente virtual
```bash
# Windows
python -m venv venv
venv\Scripts\activate

# Linux/Mac
python -m venv venv
source venv/bin/activate
```

#### 3. Instale as dependências
```bash
pip install -r requirements.txt
```

#### 4. Configure as variáveis de ambiente

**OBRIGATÓRIO**: Configure suas credenciais de nuvem:

```bash
copy env.example .env
```

Edite o arquivo `.env` com suas credenciais:

```env
# Configurações do MongoDB (Nuvem)
MONGO_URI=mongodb+srv://usuario:senha@cluster.mongodb.net/atmos_database
MONGO_DATABASE=atmos_database
MONGO_COLLECTION=sensor_data

# Configurações do MQTT (Nuvem)
MQTT_HOST=broker.hivemq.com
MQTT_TOPIC=sensors/data
```

#### Variáveis de ambiente obrigatórias:

- **MONGO_URI**: URI de conexão com MongoDB em nuvem
  - MongoDB Atlas: `mongodb+srv://usuario:senha@cluster.mongodb.net/database`
  - MongoDB Cloud: `mongodb://usuario:senha@host:porta/database`
  
- **MONGO_DATABASE**: Nome do banco de dados na nuvem
  
- **MONGO_COLLECTION**: Nome da coleção onde os dados serão inseridos
  
- **MQTT_HOST**: Endereço do broker MQTT em nuvem
  - HiveMQ Cloud: `broker.hivemq.com`
  - AWS IoT: `seu-endpoint.iot.regiao.amazonaws.com`
  - Outros: `seu-broker.com`
  
- **MQTT_TOPIC**: Tópico MQTT para receber os dados dos sensores

> **⚠️ Importante**: Você DEVE configurar o arquivo `.env` com suas credenciais de nuvem antes de executar.

## 🏃‍♂️ Como Executar

### Com Docker (Recomendado)

```bash
# Configure suas credenciais primeiro
cp env.example .env
# Edite o .env com suas credenciais de nuvem

# Inicia a aplicação
docker-compose up -d

# Para desenvolvimento (com logs em tempo real)
docker-compose up

# Para parar
docker-compose down

# Para rebuild da aplicação após mudanças
docker-compose up --build
```

### Execução Manual

#### 1. Configure suas credenciais de nuvem
```bash
cp env.example .env
# Edite o .env com suas credenciais
```

#### 2. Execute a aplicação
```bash
python src/main.py
```

> **Nota**: Para execução manual, você ainda precisa de MongoDB e MQTT em nuvem configurados.

## 📊 Funcionamento

A aplicação funciona da seguinte forma:

1. **Conexão MQTT**: Conecta ao broker MQTT especificado
2. **Inscrição no tópico**: Se inscreve no tópico configurado para receber mensagens
3. **Processamento de mensagens**: 
   - Recebe mensagens JSON dos sensores
   - Armazena temporariamente em um buffer em memória
4. **Inserção em lote**: A cada 3 segundos, insere todos os dados do buffer no MongoDB
5. **Logs**: Registra todas as operações para monitoramento

## 🔧 Configurações Avançadas

### MongoDB Atlas
```env
MONGO_URI=mongodb+srv://usuario:senha@cluster.mongodb.net/atmos_database
```

### MongoDB Cloud (Outros provedores)
```env
MONGO_URI=mongodb://usuario:senha@host:porta/atmos_database
```

### HiveMQ Cloud
```env
MQTT_HOST=broker.hivemq.com
```

### AWS IoT Core
```env
MQTT_HOST=seu-endpoint.iot.regiao.amazonaws.com
```

### MQTT com Autenticação
Para usar autenticação MQTT, você precisará modificar o arquivo `src/mqtt_client.py`:
```python
client.username_pw_set("usuario", "senha")
```

## 📝 Estrutura do Projeto

```
Atmos-receptor-dados/
├── src/
│   ├── main.py          # Ponto de entrada da aplicação
│   ├── config.py        # Carregamento das configurações
│   ├── mqtt_client.py   # Cliente MQTT e processamento de mensagens
│   └── database.py      # Conexão com MongoDB
├── requirements.txt     # Dependências Python
├── env.example         # Exemplo de variáveis de ambiente
├── Dockerfile          # Configuração do container da aplicação
├── docker-compose.yml  # Orquestração da aplicação
├── .dockerignore       # Arquivos ignorados no build Docker
└── README.md           # Este arquivo
```

## 🐳 Comandos Docker Úteis

### Gerenciamento de Containers
```bash
# Ver status dos containers
docker-compose ps

# Reiniciar apenas a aplicação
docker-compose restart atmos-app

# Rebuild e restart da aplicação
docker-compose up --build atmos-app

# Parar e remover volumes (CUIDADO: apaga dados)
docker-compose down -v
```

### Logs e Debugging
```bash
# Ver logs de todos os serviços
docker-compose logs

# Ver logs apenas da aplicação
docker-compose logs atmos-app

# Seguir logs em tempo real
docker-compose logs -f atmos-app

# Entrar no container da aplicação
docker-compose exec atmos-app bash
```

### Testando a Aplicação
```bash
# Ver logs da aplicação
docker-compose logs -f atmos-app

# Verificar se está conectado aos serviços de nuvem
docker-compose logs atmos-app | grep -i "conectado\|connected"
```

### Limpeza
```bash
# Parar todos os serviços
docker-compose down

# Remover containers e volumes
docker-compose down -v

# Remover imagens não utilizadas
docker system prune -a
```

Para visualizar os logs em tempo real, execute:
```bash
python src/main.py 2>&1 | tee logs.txt
```
