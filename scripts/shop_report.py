"""Отчёт по магазину: что умеет MongoDB из модуля 1.

Скрипт проходит все операции модуля 1 на базе shop и в песочнице sandbox
и печатает результат так, как его показывают в консоли: счётчики, таблицы,
карточку документа, страницы и итоги записи.

    cd scripts
    python3 shop_report.py

    docker compose run --rm python scripts/shop_report.py    # в контейнере
"""

import os

from pymongo import MongoClient

client = MongoClient(os.environ.get("MONGO_URI", "mongodb://localhost:27017/"))
shop = client["shop"]
products = shop["products"]
orders = shop["orders"]
box = client["sandbox"]["products"]


def title(text):
    """Заголовок раздела: отчёт читают глазами, разделы видно сразу."""
    print("\n" + text)
    print("─" * len(text))


# ── 1. Что лежит на сервере ───────────────────────────────────────────────
title("1. Связь и база shop")

client.admin.command("ping")                 # первое обращение к серверу
print("базы на сервере:", ", ".join(sorted(client.list_database_names())))
print("коллекции shop:", ", ".join(sorted(shop.list_collection_names())))
print(f"товаров {products.count_documents({})}, "
      f"заказов {orders.count_documents({})}, "
      f"покупателей {shop['customers'].count_documents({})}")

# ── 2. Карточка одного документа ──────────────────────────────────────────
title("2. Один товар целиком")

tovar = products.find_one({"_id": "p-001"})
for field, value in tovar.items():
    print(f"  {field:10} {value}")

# ── 3. Таблица: пять самых дорогих товаров ────────────────────────────────
title("3. Пять самых дорогих товаров")

print(f"  {'название':32} {'категория':14} {'цена':>8}")
cursor = (products.find({}, {"_id": 0, "title": 1, "category": 1, "price": 1})
                  .sort("price", -1)
                  .limit(5))
for doc in cursor:
    print(f"  {doc['title']:32} {doc['category']:14} {doc['price']:>8}")

# ── 4. Сортировка по двум полям ───────────────────────────────────────────
title("4. Каталог: категории по алфавиту, внутри — от дорогих к дешёвым")

katalog = (products.find({}, {"_id": 0, "category": 1, "title": 1, "price": 1})
                   .sort([("category", 1), ("price", -1)])
                   .limit(8))
for doc in katalog:
    print(f"  {doc['category']:14} {doc['title']:32} {doc['price']:>8}")

# ── 5. Счётчики по категориям ─────────────────────────────────────────────
title("5. Товары по категориям")

for category in sorted(products.distinct("category")):
    print(f"  {category:14} {products.count_documents({'category': category}):2} шт")

# ── 6. Страницы заказов ───────────────────────────────────────────────────
title("6. Заказы страницами по 5")

for page in (1, 2):
    print(f"  страница {page}")
    stranica = (orders.find({"status": "доставлен"}, {"_id": 0, "number": 1, "total": 1})
                      .sort("number", 1)
                      .skip((page - 1) * 5)
                      .limit(5))
    for doc in stranica:
        print(f"    {doc['number']}  {doc['total']:>7}")

# ── 7. Вложенные поля и массивы в выводе ──────────────────────────────────
title("7. Заказ с доставкой и позициями")

zakaz = orders.find_one({"_id": "o-0001"})
print(f"  номер {zakaz['number']}, статус {zakaz['status']}, сумма {zakaz['total']}")
print(f"  доставка: {zakaz['delivery']['city']}, {zakaz['delivery']['type']}, "
      f"{zakaz['delivery']['days']} дн.")
print(f"  оплата: {zakaz['payment']['method']}, проведена: {zakaz['payment']['paid']}")
for position in zakaz["items"]:
    print(f"    {position['qty']} × {position['title']:34} {position['price']:>7}")

# ── 8. Запись: вставка, обновление, удаление ──────────────────────────────
title("8. Песочница: вставка, обновление, удаление")

for key in ("p-301", "p-302"):        # чтобы скрипт можно было запускать повторно
    box.delete_one({"_id": key})

added = box.insert_many([
    {"_id": "p-301", "title": "Док-станция USB-C", "category": "аксессуары", "price": 6490, "reviews": 0},
    {"_id": "p-302", "title": "Кабель HDMI 2 м", "category": "аксессуары", "price": 890, "reviews": 0},
])
print("  вставлено:", len(added.inserted_ids), "ключи:", ", ".join(added.inserted_ids))

changed = box.update_one({"_id": "p-301"}, {"$set": {"price": 5990}, "$inc": {"reviews": 1}})
print(f"  обновление одного: найдено {changed.matched_count}, изменено {changed.modified_count}")

many = box.update_many({"category": "аксессуары"}, {"$set": {"sale": True}})
print(f"  пометка распродажи: найдено {many.matched_count}, изменено {many.modified_count}")

after = box.find_one({"_id": "p-301"}, {"_id": 0, "title": 1, "price": 1, "reviews": 1, "sale": 1})
print("  что стало:", after)

gone = box.delete_one({"_id": "p-302"})
print("  удалено:", gone.deleted_count, "· товаров в песочнице:", box.count_documents({}))

# ── 9. Итог ───────────────────────────────────────────────────────────────
title("9. Итог")

print(f"  прочитано из shop: {products.count_documents({})} товаров и "
      f"{orders.count_documents({})} заказов")
print("  изменено в песочнице: 1 вставка пачкой, 2 обновления, 1 удаление")
client.close()
