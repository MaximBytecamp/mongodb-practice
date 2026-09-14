"""Глава 1.6 · Порядок и порции: sort, limit, skip — заготовка для примеров.

Глава только читает данные — сбрасывать базы не нужно.
Пример из главы вставьте под чертой вместо проверочных строк и запустите:

    cd lessons
    python ch1_6.py          # Windows
    python3 ch1_6.py         # macOS и Linux

Следующий пример вставляйте вместо предыдущего: каждый пример запускается
один раз и по порядку — именно так получены выводы в справочнике.
"""

import os
from pymongo import MongoClient

client = MongoClient(os.environ.get("MONGO_URI", "mongodb://localhost:27017/"))
db = client["shop"]
products = db["products"]
orders = db["orders"]
box = client["sandbox"]["products"]        # песочница: здесь можно менять

# Коллекция на 20 000 документов для замера в §6. Создаётся один раз.
col = client["sandbox"]["bench"]
if col.count_documents({}) == 0:
    col.insert_many({"_id": n} for n in range(20000))
last_seen_id = 19989                        # последний _id предыдущей страницы

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.admin.command("ping")
print("Заготовка главы 1.6 подключилась к серверу. Замените проверочные строки примером из главы.")
