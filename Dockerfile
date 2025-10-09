# Use Python 3.11 slim como base
FROM python:3.11-slim

# Define o diretório de trabalho
WORKDIR /app

# Copia o arquivo de dependências
COPY requirements.txt .

# Instala as dependências Python
RUN pip install --no-cache-dir -r requirements.txt

# Copia o código da aplicação
COPY src/ ./src/

# Cria um usuário não-root para segurança
RUN useradd --create-home --shell /bin/bash app && chown -R app:app /app
USER app

# Expõe a porta (opcional, para logs ou health checks)
EXPOSE 8000

# Define o comando padrão
CMD ["python", "src/main.py"]

