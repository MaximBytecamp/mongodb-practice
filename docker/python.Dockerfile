FROM python:3.12-slim

COPY requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt

# sed снимает CRLF, если репозиторий склонирован на Windows с autocrlf
COPY docker/run.sh /usr/local/bin/run
RUN sed -i 's/\r$//' /usr/local/bin/run && chmod +x /usr/local/bin/run
