"""Глава 1.4 · Чтение: find, find_one и курсор — заготовка для примеров.

Глава только читает данные — сбрасывать базы не нужно.
Пример из главы вставьте под чертой вместо проверочных строк и запустите:

    cd lessons
    python ch1_4.py          # Windows
    python3 ch1_4.py         # macOS и Linux

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

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.admin.command("ping")
print("Заготовка главы 1.4 подключилась к серверу. Замените проверочные строки примером из главы.")
