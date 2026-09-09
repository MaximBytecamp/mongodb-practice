"""Генератор учебных данных для стенда курса.

Данные детерминированные: зерно фиксировано, поэтому числа в справочнике
и в ответах к практикам совпадают при любом пересоздании базы.

Запуск:  python3 generate.py
Результат: JSON-файлы рядом со скриптом, по файлу на коллекцию.
"""

import json
import random
from datetime import datetime, timedelta, timezone

random.seed(20260401)

HERE = __file__.rsplit("/", 1)[0]
UTC = timezone.utc


def iso(dt):
    """Расширенный JSON: дата, а не строка. Так mongoimport положит ISODate."""
    return {"$date": dt.astimezone(UTC).strftime("%Y-%m-%dT%H:%M:%S.000Z")}


def dump(name, docs):
    path = f"{HERE}/{name}.json"
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(docs, handle, ensure_ascii=False, indent=1)
    print(f"{name:24} {len(docs):>6} документов")


# ---------------------------------------------------------------- shop
CATEGORIES = {
    "ноутбуки": [
        ("Ноутбук Lenovo IdeaPad 3", 42990, ["15.6\"", "8 ГБ", "SSD 512 ГБ"]),
        ("Ноутбук ASUS VivoBook 15", 51490, ["15.6\"", "16 ГБ", "SSD 512 ГБ"]),
        ("Ноутбук HP Pavilion 14", 67900, ["14\"", "16 ГБ", "SSD 1 ТБ"]),
        ("Ноутбук Apple MacBook Air 13", 114990, ["13.6\"", "16 ГБ", "SSD 512 ГБ"]),
        ("Ноутбук Acer Nitro V15", 89990, ["15.6\"", "16 ГБ", "RTX 4050"]),
    ],
    "смартфоны": [
        ("Смартфон Xiaomi Redmi Note 13", 18990, ["6.67\"", "128 ГБ"]),
        ("Смартфон Samsung Galaxy A55", 32990, ["6.6\"", "256 ГБ"]),
        ("Смартфон Apple iPhone 15", 79990, ["6.1\"", "128 ГБ"]),
        ("Смартфон realme 12 Pro", 27490, ["6.7\"", "256 ГБ"]),
    ],
    "периферия": [
        ("Клавиатура Keychron K2", 8990, ["механическая", "Bluetooth"]),
        ("Мышь Logitech MX Master 3S", 9490, ["беспроводная", "8000 dpi"]),
        ("Монитор Dell P2422H", 21990, ["24\"", "IPS", "75 Гц"]),
        ("Наушники Sony WH-1000XM5", 34990, ["накладные", "шумоподавление"]),
        ("Веб-камера Logitech C920", 6790, ["1080p", "USB"]),
    ],
    "комплектующие": [
        ("SSD Samsung 980 1 ТБ", 7890, ["NVMe", "1 ТБ"]),
        ("Оперативная память Kingston 16 ГБ", 4590, ["DDR4", "3200 МГц"]),
        ("Видеокарта NVIDIA RTX 4060", 38990, ["8 ГБ", "PCIe 4.0"]),
        ("Блок питания be quiet! 650 Вт", 8490, ["650 Вт", "80+ Gold"]),
    ],
    "аксессуары": [
        ("Рюкзак для ноутбука Xiaomi", 2990, ["15.6\"", "водоотталкивающий"]),
        ("Кабель USB-C 2 м", 790, ["USB-C", "2 м"]),
        ("Powerbank Anker 20000 мА·ч", 4290, ["20000 мА·ч", "65 Вт"]),
    ],
}

WAREHOUSES = ["Москва-1", "Москва-2", "Санкт-Петербург", "Екатеринбург"]
# Латинский код категории для артикула: артикул читают и люди, и внешние системы.
CATEGORY_CODE = {
    "ноутбуки": "NB", "смартфоны": "PH", "периферия": "PR",
    "комплектующие": "CP", "аксессуары": "AC",
}
BRAND_BY_WORD = {
    "Lenovo": "Lenovo", "ASUS": "ASUS", "HP": "HP", "Apple": "Apple", "Acer": "Acer",
    "Xiaomi": "Xiaomi", "Samsung": "Samsung", "realme": "realme", "Keychron": "Keychron",
    "Logitech": "Logitech", "Dell": "Dell", "Sony": "Sony", "Kingston": "Kingston",
    "NVIDIA": "NVIDIA", "quiet!": "be quiet!", "Anker": "Anker",
}

products = []
counter = 0
for category, items in CATEGORIES.items():
    for title, price, specs in items:
        counter += 1
        brand = next((b for word, b in BRAND_BY_WORD.items() if word in title), "OEM")
        stock = [
            {"warehouse": w, "qty": random.choice([0, 0, 3, 5, 8, 12, 20, 40])}
            for w in random.sample(WAREHOUSES, random.choice([1, 2, 2, 3]))
        ]
        product = {
            "_id": f"p-{counter:03d}",
            "sku": f"SKU-{CATEGORY_CODE[category]}-{counter:03d}",
            "title": title,
            "brand": brand,
            "category": category,
            "price": price,
            "specs": specs,
            "stock": stock,
            "rating": round(random.uniform(3.6, 4.9), 1),
            "reviews": random.randint(0, 240),
            "added": iso(datetime(2026, 1, 1, tzinfo=UTC) + timedelta(days=random.randint(0, 200))),
        }
        # У части товаров нет скидки — поле просто отсутствует. Это нужно для $exists.
        if random.random() < 0.45:
            product["discount"] = random.choice([5, 10, 15, 20, 30])
        products.append(product)

CITIES = ["Москва", "Санкт-Петербург", "Ярославль", "Екатеринбург", "Казань", "Новосибирск"]
NAMES = [
    "Анна Белова", "Игорь Титов", "Мария Дроздова", "Пётр Ким", "Ольга Синицына",
    "Дмитрий Волков", "Елена Круглова", "Артём Носов", "Светлана Гаева", "Роман Ильин",
    "Ксения Лаврова", "Никита Жуков", "Вера Панина", "Максим Седов", "Юлия Ефимова",
    "Андрей Барсов", "Инна Королёва", "Тимур Ахметов", "Лидия Чернова", "Егор Мельник",
]

customers = []
for number, name in enumerate(NAMES, start=1):
    login = f"user{number:02d}"
    customers.append({
        "_id": f"c-{number:03d}",
        "name": name,
        "email": f"{login}@example.com",
        "city": random.choice(CITIES),
        "registered": iso(datetime(2025, 6, 1, tzinfo=UTC) + timedelta(days=random.randint(0, 400))),
        "segment": random.choice(["новый", "постоянный", "постоянный", "vip"]),
        "phone": f"+7 9{random.randint(10, 99)} {random.randint(100, 999)}-{random.randint(10, 99)}-{random.randint(10, 99)}",
    })

STATUSES = ["доставлен", "доставлен", "доставлен", "в пути", "собирается", "отменён"]
PAYMENTS = ["карта", "карта", "картой при получении", "СБП"]

orders = []
for number in range(1, 121):
    customer = random.choice(customers)
    created = datetime(2026, 3, 1, tzinfo=UTC) + timedelta(
        days=random.randint(0, 180), hours=random.randint(8, 22), minutes=random.choice([0, 15, 30, 45])
    )
    picked = random.sample(products, random.choice([1, 1, 2, 2, 3, 4]))
    items = []
    for product in picked:
        qty = random.choice([1, 1, 1, 2, 3])
        items.append({
            "sku": product["sku"],
            "title": product["title"],
            "category": product["category"],
            "qty": qty,
            "price": product["price"],
        })
    total = sum(item["qty"] * item["price"] for item in items)
    status = random.choice(STATUSES)
    order = {
        "_id": f"o-{number:04d}",
        "number": f"2026-{number:04d}",
        "customer_id": customer["_id"],
        "created": iso(created),
        "status": status,
        "items": items,
        "total": total,
        "payment": {"method": random.choice(PAYMENTS), "paid": status != "отменён"},
        "delivery": {
            "city": customer["city"],
            "type": random.choice(["курьер", "пункт выдачи", "пункт выдачи", "почта"]),
            "days": random.randint(1, 9),
        },
    }
    if random.random() < 0.3:
        order["promo"] = random.choice(["SPRING10", "FIRST5", "LOYAL15"])
    orders.append(order)

dump("shop.products", products)
# Песочница: та же коллекция товаров, но её не жалко менять и удалять.
# Эталонные базы из-за упражнений не должны разъезжаться с книгой.
dump("sandbox.products", products)
dump("shop.customers", customers)
dump("shop.orders", orders)

# ---------------------------------------------------------------- logs
SERVICES = ["api-gateway", "orders", "payments", "search", "notifier"]
LEVELS = ["info", "info", "info", "info", "warn", "error"]
ROUTES = ["/api/orders", "/api/orders/{id}", "/api/products", "/api/search", "/api/login", "/api/pay"]

events = []
moment = datetime(2026, 9, 1, 0, 0, tzinfo=UTC)
for number in range(1, 1201):
    moment += timedelta(seconds=random.randint(20, 240))
    level = random.choice(LEVELS)
    service = random.choice(SERVICES)
    status = 200
    if level == "warn":
        status = random.choice([301, 400, 404, 429])
    if level == "error":
        status = random.choice([500, 502, 503])
    event = {
        "_id": f"e-{number:05d}",
        "ts": iso(moment),
        "service": service,
        "level": level,
        "route": random.choice(ROUTES),
        "status": status,
        "duration_ms": random.randint(4, 90) if level == "info" else random.randint(120, 4200),
        "user_id": random.choice([None, f"c-{random.randint(1, 20):03d}"]),
        "trace": f"tr-{random.randint(100000, 999999)}",
    }
    if level == "error":
        event["error"] = {
            "type": random.choice(["TimeoutError", "ConnectionError", "ValidationError"]),
            "message": random.choice([
                "upstream did not respond in 3000 ms",
                "connection refused by payments:8080",
                "field 'total' must be a positive number",
            ]),
        }
    events.append(event)

dump("logs.events", events)

# ---------------------------------------------------------------- org
STAFF = [
    ("s-001", "Ирина Соколова", "директор по продукту", None, "продукт", 420000),
    ("s-002", "Павел Демин", "руководитель разработки", "s-001", "разработка", 380000),
    ("s-003", "Наталья Гурьева", "руководитель аналитики", "s-001", "аналитика", 330000),
    ("s-004", "Алексей Ким", "тимлид backend", "s-002", "разработка", 300000),
    ("s-005", "Марина Власова", "тимлид frontend", "s-002", "разработка", 290000),
    ("s-006", "Сергей Носов", "backend-разработчик", "s-004", "разработка", 240000),
    ("s-007", "Дарья Белова", "backend-разработчик", "s-004", "разработка", 215000),
    ("s-008", "Кирилл Юдин", "junior backend-разработчик", "s-004", "разработка", 120000),
    ("s-009", "Ольга Рябова", "frontend-разработчик", "s-005", "разработка", 230000),
    ("s-010", "Тимур Ахметов", "frontend-разработчик", "s-005", "разработка", 205000),
    ("s-011", "Егор Мельник", "аналитик данных", "s-003", "аналитика", 210000),
    ("s-012", "Вера Панина", "аналитик данных", "s-003", "аналитика", 195000),
    ("s-013", "Роман Ильин", "junior аналитик", "s-011", "аналитика", 110000),
    ("s-014", "Юлия Ефимова", "QA-инженер", "s-002", "разработка", 180000),
    ("s-015", "Максим Седов", "DevOps-инженер", "s-002", "инфраструктура", 310000),
]
employees = [
    {
        "_id": staff_id,
        "name": name,
        "position": position,
        "manager_id": manager,
        "department": department,
        "salary": salary,
        "hired": iso(datetime(2023, 1, 10, tzinfo=UTC) + timedelta(days=random.randint(0, 900))),
        "skills": random.sample(
            ["Python", "MongoDB", "PostgreSQL", "Docker", "React", "SQL", "Kafka", "Airflow"],
            random.randint(2, 4),
        ),
    }
    for staff_id, name, position, manager, department, salary in STAFF
]
dump("org.employees", employees)

CATALOG = [
    ("cat-tech", "Техника", None),
    ("cat-comp", "Компьютеры", "cat-tech"),
    ("cat-note", "Ноутбуки", "cat-comp"),
    ("cat-parts", "Комплектующие", "cat-comp"),
    ("cat-phone", "Телефоны и гаджеты", "cat-tech"),
    ("cat-smart", "Смартфоны", "cat-phone"),
    ("cat-acc", "Аксессуары", "cat-phone"),
    ("cat-periph", "Периферия", "cat-comp"),
    ("cat-audio", "Аудио", "cat-periph"),
    ("cat-input", "Клавиатуры и мыши", "cat-periph"),
]
dump("org.categories", [
    {"_id": cid, "title": title, "parent_id": parent}
    for cid, title, parent in CATALOG
])

print("\nГотово. Дальше: bash load.sh")
