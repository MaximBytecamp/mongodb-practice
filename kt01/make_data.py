"""Данные контрольной точки 01: учётные остатки и фактический пересчёт.

Учёт (warehouse.json) кладётся в sandbox.warehouse загрузчиком стенда.
Пересчёт (stocktake.json) студент заливает сам — это часть задания.

Зерно фиксировано: расхождения у всех одинаковые, числа сверяются на приёмке.
"""

import json
import random

random.seed(4242)

HERE = __file__.rsplit("/", 1)[0]
PRODUCTS = json.load(open(f"{HERE}/../stend/seed/shop.products.json", encoding="utf-8"))

warehouse, stocktake = [], []

for number, product in enumerate(PRODUCTS, start=1):
    accounted = random.choice([0, 3, 5, 8, 12, 20, 25, 40])
    warehouse.append({
        "_id": f"w-{number:03d}",
        "sku": product["sku"],
        "title": product["title"],
        "category": product["category"],
        "qty_accounted": accounted,       # сколько числится по учёту
        "shelf": f"{random.choice('ABCD')}-{random.randint(1, 24):02d}",
    })

    # Расхождение есть примерно у трети позиций: недостача чаще излишка.
    delta = 0
    roll = random.random()
    if roll < 0.24:
        delta = -random.choice([1, 1, 2, 3])
    elif roll < 0.40:
        delta = random.choice([1, 2])
    stocktake.append({
        "sku": product["sku"],
        "counted": max(0, accounted + delta),
        "counted_by": random.choice(["Волков", "Синицына", "Ким"]),
    })

json.dump(warehouse, open(f"{HERE}/../stend/seed/sandbox.warehouse.json", "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)
json.dump(stocktake, open(f"{HERE}/stocktake.json", "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)

matches = sum(1 for w, s in zip(warehouse, stocktake) if w["qty_accounted"] == s["counted"])
short = sum(1 for w, s in zip(warehouse, stocktake) if s["counted"] < w["qty_accounted"])
over = sum(1 for w, s in zip(warehouse, stocktake) if s["counted"] > w["qty_accounted"])
zero = sum(1 for s in stocktake if s["counted"] == 0)

print(f"позиций: {len(warehouse)}")
print(f"сошлось: {matches}, недостача: {short}, излишек: {over}, нулевой остаток: {zero}")
