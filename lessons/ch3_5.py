"""Глава 3.5 · География: 2dsphere, $near, $geoWithin, $geoIntersects — заготовка для примеров.

Перед главой верните базы в исходное состояние (глава 1.0, §8).
Пример из главы вставьте под чертой вместо проверочных строк и запустите:

    cd lessons
    python ch3_5.py          # Windows
    python3 ch3_5.py         # macOS и Linux

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

pickup = sandbox["pickup"]        # пункты выдачи: точки на карте
zones = sandbox["zones"]          # зоны доставки: многоугольники

# Покупатель в центре Ярославля: долгота, затем широта.
here = {"type": "Point", "coordinates": [39.8875, 57.6300]}

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.admin.command("ping")
print("Заготовка главы 3.5 подключилась к серверу. Замените проверочные строки примером из главы.")
