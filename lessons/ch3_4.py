"""Глава 3.4 · Текстовый поиск: $text и $search — заготовка для примеров.

Перед главой верните базы в исходное состояние (глава 1.0, §8).
Пример из главы вставьте под чертой вместо проверочных строк и запустите:

    cd lessons
    python ch3_4.py          # Windows
    python3 ch3_4.py         # macOS и Linux

Следующий пример вставляйте вместо предыдущего: каждый пример запускается
один раз и по порядку — именно так получены выводы в справочнике.
"""

import os
from pymongo import MongoClient

client = MongoClient(os.environ.get("MONGO_URI", "mongodb://localhost:27017/"))
db = client["shop"]
products = db["products"]
orders = db["orders"]
events = client["logs"]["events"]
sandbox = client["sandbox"]
box = sandbox["products"]        # песочница: здесь можно менять

showcase = sandbox["showcase"]    # копия товаров магазина: на ней строится текстовый индекс

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.admin.command("ping")
print("Заготовка главы 3.4 подключилась к серверу. Замените проверочные строки примером из главы.")
