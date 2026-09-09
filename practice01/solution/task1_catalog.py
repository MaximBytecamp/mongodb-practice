"""ПР-01, задача 1. Витрина каталога: страница категории.

Прикладная постановка: на сайте магазина открывается раздел категории.
Нужно отдать фронтенду ровно то, что он покажет, — и ничего лишнего:
название, цену, признак наличия и номер страницы.

Запуск:
    python3 solution/task1_catalog.py
    python3 solution/task1_catalog.py --category смартфоны --page 1

Используются только средства модуля 1: find, проекция, sort, limit, skip,
count_documents. Наличие считаем в Python по массиву stock — операторы
для массивов будут в модуле 2.
"""

import argparse
import os

from pymongo import MongoClient

PER_PAGE = 5


def in_stock(product):
    """Товар считается в наличии, если хотя бы на одном складе есть остаток."""
    return any(item.get("qty", 0) > 0 for item in product.get("stock", []))


def catalog_page(products, category, page=1, per_page=PER_PAGE):
    """Одна страница каталога: документы и данные для пагинации."""
    query = {"category": category}

    total = products.count_documents(query)
    cursor = (
        products.find(query, {"_id": 0, "title": 1, "price": 1, "stock": 1, "discount": 1})
        .sort([("price", -1), ("title", 1)])   # второй ключ — чтобы порядок был однозначным
        .skip((page - 1) * per_page)
        .limit(per_page)
    )

    items = []
    for product in cursor:
        items.append({
            "title": product["title"],
            "price": product["price"],
            "discount": product.get("discount"),      # поля может не быть — это законно
            "available": in_stock(product),
        })

    pages = (total + per_page - 1) // per_page
    return {"category": category, "page": page, "pages": pages, "total": total, "items": items}


def render(page_data):
    """Печать витрины так, как её увидел бы человек."""
    print(f"Категория «{page_data['category']}» — товаров: {page_data['total']}, "
          f"страница {page_data['page']} из {page_data['pages']}")
    print("-" * 62)
    for item in page_data["items"]:
        mark = "в наличии" if item["available"] else "под заказ"
        discount = f"  −{item['discount']}%" if item["discount"] else ""
        print(f"{item['price']:>8} ₽  {item['title']:<34} {mark}{discount}")
    if not page_data["items"]:
        print("  на этой странице пусто")


def main():
    parser = argparse.ArgumentParser(description="Витрина каталога магазина")
    parser.add_argument("--category", default="ноутбуки")
    parser.add_argument("--page", type=int, default=1)
    parser.add_argument("--uri", default=os.environ.get("MONGO_URI", "mongodb://localhost:27017/"))
    args = parser.parse_args()

    client = MongoClient(args.uri)
    try:
        products = client["shop"]["products"]
        render(catalog_page(products, args.category, args.page))

        print()
        print("Сводка по всем категориям:")
        for category in sorted(products.distinct("category")):
            count = products.count_documents({"category": category})
            print(f"  {category:<16} {count:>2} товаров")
    finally:
        client.close()


if __name__ == "__main__":
    main()
