"""Глава 1.2 · Документ и BSON — заготовка для примеров.

Перед главой верните базы в исходное состояние (глава 1.0, §8).
Пример из главы вставьте в конец файла, под чертой, и запустите:

    cd lessons
    python ch1_2.py          # Windows
    python3 ch1_2.py         # macOS и Linux

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
