"""Проверка подключения: что лежит на сервере.

    docker compose run --rm python hello/hello.py    # из контейнера
    python hello/hello.py                            # с компьютера, если стоит pymongo
"""

import os
from pymongo import MongoClient

# В контейнере сервер называется mongo, на компьютере — localhost
URI = os.environ.get("MONGO_URI", "mongodb://localhost:27017/")
client = MongoClient(URI, serverSelectionTimeoutMS=5000)

print("Адрес:", URI)
print("Сервер:", client.server_info()["version"])
print("Базы данных:", client.list_database_names())

db = client["shop"]
print("Коллекции shop:", sorted(db.list_collection_names()))
print("Товаров:", db["products"].count_documents({}))
print("Заказов:", db["orders"].count_documents({}))

client.close()
