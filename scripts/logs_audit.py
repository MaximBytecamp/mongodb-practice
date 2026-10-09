"""Аудит журнала сервиса: что умеют правила, индексы и explain из модуля 3.

Скрипт работает с копией журнала logs.events в песочнице: проверяет события
правилом $jsonSchema и включает его как валидатор, снимает планы трёх
запросов дежурного, строит под них индексы по правилу ESR и сравнивает
планы до и после. База logs остаётся без изменений.

    cd scripts
    python3 logs_audit.py

    docker compose run --rm python scripts/logs_audit.py    # в контейнере
"""

import os
from datetime import datetime

from pymongo import MongoClient
from pymongo.errors import WriteError

client = MongoClient(os.environ.get("MONGO_URI", "mongodb://localhost:27017/"))
events = client["logs"]["events"]
sandbox = client["sandbox"]
audit = sandbox["audit"]


def title(text):
    print("\n" + text)
    print("─" * len(text))


def summary(find):
    """План и счётчики explain: стадии сверху вниз, вернул, ключей, документов."""
    info = sandbox.command("explain", {"find": "audit", **find}, verbosity="executionStats")
    stage = info["queryPlanner"]["winningPlan"]
    stage = stage.get("queryPlan", stage)
    steps = []
    while stage:
        steps.append(stage["stage"] + (" " + stage["indexName"] if "indexName" in stage else ""))
        stage = stage.get("inputStage")
    stats = info["executionStats"]
    return " → ".join(steps), stats["nReturned"], stats["totalKeysExamined"], stats["totalDocsExamined"]


# ── 1. Копия журнала ──────────────────────────────────────────────────────
title("1. Копия журнала в песочнице")

audit.drop()
audit.insert_many(events.find())
print(f"  событий в копии: {audit.count_documents({})}")
print(f"  индексов: {len(list(audit.list_indexes()))}")

# ── 2. Правило для событий ────────────────────────────────────────────────
title("2. Правило для событий")

RULE = {
    "bsonType": "object",
    "required": ["ts", "service", "level", "route", "status", "duration_ms"],
    "properties": {
        "ts": {"bsonType": "date"},
        "service": {"enum": ["api-gateway", "notifier", "orders", "payments", "search"]},
        "level": {"enum": ["info", "warn", "error"]},
        "route": {"bsonType": "string"},
        "status": {"bsonType": "int", "minimum": 100, "maximum": 599},
        "duration_ms": {"bsonType": "number", "minimum": 0},
    },
}
print(f"  соответствуют правилу:    {audit.count_documents({'$jsonSchema': RULE})}")
print(f"  не соответствуют правилу: {audit.count_documents({'$nor': [{'$jsonSchema': RULE}]})}")

sandbox.command("collMod", "audit", validator={"$jsonSchema": RULE},
                validationLevel="strict", validationAction="error")
print("  правило включено: strict, error")

# ── 3. Проверка правила на записи ─────────────────────────────────────────
title("3. Неверные события")

good = {"ts": datetime(2026, 9, 2, 18, 30), "service": "payments", "level": "error",
        "route": "/api/pay", "status": 500, "duration_ms": 300}
wrong = [
    ("статус 700", {**good, "status": 700}),
    ("уровень fatal", {**good, "level": "fatal"}),
    ("без времени", {key: value for key, value in good.items() if key != "ts"}),
]
for label, doc in wrong:
    try:
        audit.insert_one(doc)
        print(f"  {label:14} записано")
    except WriteError as e:
        reasons = []
        for rule in e.details["errInfo"]["details"]["schemaRulesNotSatisfied"]:
            if rule["operatorName"] == "required":
                reasons += [f"нет {name}" for name in rule["missingProperties"]]
            if rule["operatorName"] == "properties":
                for prop in rule["propertiesNotSatisfied"]:
                    reasons += [f"{prop['propertyName']}: {d['operatorName']}" for d in prop["details"]]
        print(f"  {label:14} отклонено, код {e.code}: {', '.join(reasons)}")
print(f"  событий в копии: {audit.count_documents({})}")

# ── 4. Запросы дежурного без индексов ─────────────────────────────────────
QUERIES = [
    ("ошибки платежей", {"filter": {"service": "payments", "status": {"$gte": 500}},
                         "sort": {"ts": -1}, "limit": 5}),
    ("медленный поиск", {"filter": {"service": "search", "duration_ms": {"$gte": 1000}},
                         "sort": {"duration_ms": -1}, "limit": 5}),
    ("события покупателя", {"filter": {"user_id": "c-015"}, "sort": {"ts": -1}}),
]


def report():
    print(f"  {'запрос':20} {'вернул':>6} {'ключей':>7} {'документов':>11}")
    for name, find in QUERIES:
        plan, returned, keys, docs = summary(find)
        print(f"  {name:20} {returned:6} {keys:7} {docs:11}")
        print(f"      {plan}")


title("4. Запросы дежурного без индексов")
report()

# ── 5. Индексы по правилу ESR ─────────────────────────────────────────────
title("5. Индексы: равенство, сортировка, диапазон")

for keys in ([("service", 1), ("ts", -1), ("status", 1)],
             [("service", 1), ("duration_ms", -1)],
             [("user_id", 1), ("ts", -1)]):
    print(f"  создан {audit.create_index(keys)}")

# ── 6. Те же запросы с индексами ──────────────────────────────────────────
title("6. Запросы дежурного с индексами")
report()

# ── 7. Пять последних ошибок платежей ─────────────────────────────────────
title("7. Пять последних ошибок платежей")

name, find = QUERIES[0]
for doc in audit.find(find["filter"]).sort("ts", -1).limit(find["limit"]):
    print(f"  {doc['ts']:%d.%m %H:%M:%S}  {doc['route']:18} {doc['status']}  {doc['duration_ms']:5} мс")

client.close()
