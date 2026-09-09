"""ПР-01, задача 2. Ревизия каталога: приёмка партии и снятие с продажи.

Прикладная постановка: пришла новая партия товаров, у одной позиции
поменялась цена, категорию «аксессуары» решили снять с витрины, а черновые
карточки из базы убрать совсем. Всё это — рабочий день контент-менеджера,
и всё делается четырьмя операциями модуля 1.

Скрипт устроен так, чтобы его можно было запускать повторно: он не
задваивает данные и печатает отчёт о каждом шаге.

Запуск:
    python3 solution/task2_revision.py            # обычный прогон
    python3 solution/task2_revision.py --dry-run  # только показать, что изменится
"""

import argparse
import json
import os
import pathlib

from pymongo import MongoClient
from pymongo.errors import BulkWriteError

HERE = pathlib.Path(__file__).resolve().parent.parent
DELIVERY = HERE / "data" / "delivery.json"


def step(title):
    print()
    print(title)
    print("-" * len(title))


def accept_delivery(box, dry_run):
    """Шаг 1. Приёмка партии: новые карточки товаров из файла поставки."""
    step("Шаг 1 · приёмка партии")
    with open(DELIVERY, encoding="utf-8") as handle:
        batch = json.load(handle)

    known = box.count_documents({"_id": {"$in": [item["_id"] for item in batch]}})
    print(f"в файле позиций: {len(batch)}, из них уже в базе: {known}")
    if dry_run:
        return 0

    # ordered=False: одна уже существующая позиция не должна останавливать приёмку.
    try:
        result = box.insert_many(batch, ordered=False)
        inserted = len(result.inserted_ids)
    except BulkWriteError as error:
        failed = error.details["writeErrors"]
        inserted = len(batch) - len(failed)
        print(f"пропущено дубликатов: {len(failed)} (повторный запуск — это нормально)")
    print(f"добавлено новых карточек: {inserted}")
    return inserted


def change_price(box, sku, price, dry_run):
    """Шаг 2. Новая цена по артикулу — с проверкой, что товар вообще нашёлся."""
    step("Шаг 2 · новая цена")
    query = {"sku": sku}
    current = box.find_one(query, {"_id": 0, "title": 1, "price": 1})
    if current is None:
        print(f"товар {sku} не найден — цена не менялась")
        return 0
    print(f"{current['title']}: было {current['price']} ₽, станет {price} ₽")
    if dry_run:
        return 0

    result = box.update_one(query, {"$set": {"price": price}})
    # Проверяем matched, а не modified: цена могла уже быть такой.
    print(f"нашлось: {result.matched_count}, изменено: {result.modified_count}")
    return result.modified_count


def hide_category(box, category, dry_run):
    """Шаг 3. Снятие с витрины — мягко: отметкой, а не удалением."""
    step("Шаг 3 · снятие категории с витрины")
    query = {"category": category}
    count = box.count_documents(query)
    print(f"под фильтр {query} попадает документов: {count}")
    for product in box.find(query, {"_id": 1, "title": 1}).limit(10):
        print(f"  {product['_id']}  {product['title']}")
    if dry_run:
        return 0

    result = box.update_many(query, {"$set": {"discontinued": True}})
    print(f"помечено снятыми: {result.modified_count} (найдено {result.matched_count})")
    return result.modified_count


def drop_drafts(box, dry_run):
    """Шаг 4. Черновики удаляем по-настоящему: истории по ним не ведём."""
    step("Шаг 4 · удаление черновиков")
    query = {"status": "черновик"}
    count = box.count_documents(query)
    print(f"черновиков в базе: {count}")
    if dry_run or count == 0:
        return 0

    result = box.delete_many(query)
    print(f"удалено: {result.deleted_count}")
    if result.deleted_count != count:
        print("ВНИМАНИЕ: удалено не столько, сколько нашлось, — данные меняет кто-то ещё")
    return result.deleted_count


def main():
    parser = argparse.ArgumentParser(description="Ревизия каталога магазина")
    parser.add_argument("--dry-run", action="store_true", help="ничего не менять, только показать")
    parser.add_argument("--uri", default=os.environ.get("MONGO_URI", "mongodb://localhost:27017/"))
    args = parser.parse_args()

    client = MongoClient(args.uri)
    try:
        box = client["sandbox"]["products"]      # правим только песочницу
        print(f"документов в sandbox.products до ревизии: {box.count_documents({})}")

        accept_delivery(box, args.dry_run)
        change_price(box, "SKU-NB-001", 39990, args.dry_run)
        hide_category(box, "аксессуары", args.dry_run)
        drop_drafts(box, args.dry_run)

        print()
        print(f"документов в sandbox.products после ревизии: {box.count_documents({})}")
        print(f"снято с витрины: {box.count_documents({'discontinued': True})}")
    finally:
        client.close()


if __name__ == "__main__":
    main()
