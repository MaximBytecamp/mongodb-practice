"""Глава 1.7 · Обновление: $set, $inc, $unset — заготовка для примеров.

Перед главой верните базы в исходное состояние (глава 1.0, §8).
Пример из главы вставьте под чертой вместо проверочных строк и запустите:

    cd lessons
    python ch1_7.py          # Windows
    python3 ch1_7.py         # macOS и Linux

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

# Товары, которые добавлялись в песочницу в главе 1.3. Если песочницу
# сбрасывали, заготовка добавит их снова — обновлять будет что.
if box.count_documents({"_id": "p-101"}) == 0:
    box.insert_many([
        {"_id": "p-101", "title": "Подставка для ноутбука", "category": "аксессуары", "price": 2490},
        {"_id": "p-102", "title": "Чехол для планшета", "category": "аксессуары", "price": 1890},
        {"sku": "SKU-AC-022", "title": "Коврик для мыши XL", "brand": "OEM",
         "category": "аксессуары", "price": 1290},
    ])

orders_box = client["sandbox"]["orders"]      # копия заказов: их можно менять
if orders_box.count_documents({}) == 0:
    orders_box.insert_many(orders.find())

stats = client["sandbox"]["product_stats"]    # счётчики просмотров для §6
stats.drop()                                  # каждый запуск заготовки считает с нуля
from datetime import date
today = date.today().isoformat()

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.admin.command("ping")
print("Заготовка главы 1.7 подключилась к серверу. Замените проверочные строки примером из главы.")
