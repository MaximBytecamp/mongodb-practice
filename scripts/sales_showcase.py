"""Витрина продаж за квартал: конвейеры агрегации из модуля 4.

Скрипт строит отчёт по заказам магазина за второй квартал 2026 года
(апрель — июнь): статусы, выручка по месяцам и городам, категории и товары,
чеки по суммам, самый дорогой заказ каждого города, оплата и доставка.
Базы только читаются.

    cd scripts
    python3 sales_showcase.py

    docker compose run --rm python scripts/sales_showcase.py    # в контейнере
"""

import os
from datetime import datetime

from pymongo import MongoClient

client = MongoClient(os.environ.get("MONGO_URI", "mongodb://localhost:27017/"))
orders = client["shop"]["orders"]

QUARTER = {"created": {"$gte": datetime(2026, 4, 1), "$lt": datetime(2026, 7, 1)}}
DELIVERED = {"$match": {**QUARTER, "status": "доставлен"}}


def title(text):
    print("\n" + text)
    print("─" * len(text))


# ── 1. Заказы квартала ────────────────────────────────────────────────────
title("1. Заказы квартала")

for doc in orders.aggregate([{"$match": QUARTER}, {"$sortByCount": "$status"}]):
    print(f"  {doc['_id']:12} {doc['count']:4}")

total = next(orders.aggregate([
    DELIVERED,
    {"$group": {"_id": None, "orders": {"$sum": 1}, "revenue": {"$sum": "$total"}, "avg": {"$avg": "$total"}}},
    {"$project": {"orders": 1, "revenue": 1, "avg": {"$round": ["$avg", 0]}}},
]))
print(f"  доставлено: {total['orders']}, выручка {total['revenue']}, средний чек {int(total['avg'])}")

# ── 2. Выручка по месяцам ─────────────────────────────────────────────────
title("2. Выручка по месяцам")

months = orders.aggregate([
    DELIVERED,
    {"$bucket": {"groupBy": "$created",
                 "boundaries": [datetime(2026, 4, 1), datetime(2026, 5, 1), datetime(2026, 6, 1), datetime(2026, 7, 1)],
                 "output": {"orders": {"$sum": 1}, "revenue": {"$sum": "$total"}}}},
])
for name, doc in zip(["апрель", "май", "июнь"], months):
    print(f"  {name:12} {doc['orders']:4} {doc['revenue']:10}")

# ── 3. Города ─────────────────────────────────────────────────────────────
title("3. Города")

print(f"  {'город':16} {'заказов':>7} {'выручка':>10} {'средний чек':>12}")
for doc in orders.aggregate([
    DELIVERED,
    {"$group": {"_id": "$delivery.city", "orders": {"$sum": 1},
                "revenue": {"$sum": "$total"}, "avg": {"$avg": "$total"}}},
    {"$project": {"orders": 1, "revenue": 1, "avg": {"$round": ["$avg", 0]}}},
    {"$sort": {"revenue": -1}},
]):
    print(f"  {doc['_id']:16} {doc['orders']:7} {doc['revenue']:10} {int(doc['avg']):12}")

# ── 4. Категории ──────────────────────────────────────────────────────────
title("4. Категории")

print(f"  {'категория':16} {'штук':>5} {'выручка':>10}")
for doc in orders.aggregate([
    DELIVERED,
    {"$unwind": "$items"},
    {"$group": {"_id": "$items.category", "qty": {"$sum": "$items.qty"},
                "revenue": {"$sum": {"$multiply": ["$items.qty", "$items.price"]}}}},
    {"$sort": {"revenue": -1}},
]):
    print(f"  {doc['_id']:16} {doc['qty']:5} {doc['revenue']:10}")

# ── 5. Пять самых продаваемых товаров ─────────────────────────────────────
title("5. Пять самых продаваемых товаров")

for place, doc in enumerate(orders.aggregate([
    DELIVERED,
    {"$unwind": "$items"},
    {"$group": {"_id": "$items.title", "qty": {"$sum": "$items.qty"}}},
    {"$sort": {"qty": -1, "_id": 1}},
    {"$limit": 5},
]), 1):
    print(f"  {place}. {doc['_id']:32} {doc['qty']:3} шт.")

# ── 6. Чеки по суммам ─────────────────────────────────────────────────────
title("6. Чеки по суммам")

LABELS = {0: "до 10 000", 10000: "10 000 — 50 000", 50000: "50 000 — 200 000", 200000: "от 200 000"}
for doc in orders.aggregate([
    DELIVERED,
    {"$bucket": {"groupBy": "$total", "boundaries": [0, 10000, 50000, 200000, 500000],
                 "output": {"orders": {"$sum": 1}}}},
]):
    print(f"  {LABELS[doc['_id']]:18} {doc['orders']:4}")

# ── 7. Самый дорогой заказ города ─────────────────────────────────────────
title("7. Самый дорогой заказ города")

for doc in orders.aggregate([
    DELIVERED,
    {"$sort": {"total": -1}},
    {"$group": {"_id": "$delivery.city", "number": {"$first": "$number"}, "total": {"$first": "$total"}}},
    {"$sort": {"total": -1}},
]):
    print(f"  {doc['_id']:16} {doc['number']}  {doc['total']:8}")

# ── 8. Оплата и доставка ──────────────────────────────────────────────────
title("8. Оплата и доставка")

facets = next(orders.aggregate([
    DELIVERED,
    {"$facet": {
        "payment": [{"$sortByCount": "$payment.method"}],
        "delivery": [{"$sortByCount": "$delivery.type"}],
        "promo": [{"$match": {"promo": {"$exists": True}}}, {"$count": "orders"}],
    }},
]))
print("  оплата:   " + ", ".join(f"{d['_id']} {d['count']}" for d in facets["payment"]))
print("  доставка: " + ", ".join(f"{d['_id']} {d['count']}" for d in facets["delivery"]))
print(f"  с кодом скидки: {facets['promo'][0]['orders'] if facets['promo'] else 0}")

client.close()
