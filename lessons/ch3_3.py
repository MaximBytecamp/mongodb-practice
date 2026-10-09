"""Глава 3.3 · Как читать explain: COLLSCAN против IXSCAN — заготовка для примеров.

Перед главой верните базы в исходное состояние (глава 1.0, §8).
Пример из главы вставьте под чертой вместо проверочных строк и запустите:

    cd lessons
    python ch3_3.py          # Windows
    python3 ch3_3.py         # macOS и Linux

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

journal = sandbox["events"]       # копия журнала: на ней строятся индексы главы


def summary(coll, **find):
    """План и счётчики explain одной строкой: стадии | вернул | ключей | документов."""
    info = coll.database.command("explain", {"find": coll.name, **find}, verbosity="executionStats")
    stage = info["queryPlanner"]["winningPlan"]
    stage = stage.get("queryPlan", stage)     # MongoDB 7 кладёт план во вложенное поле
    steps = []
    while stage:
        steps.append(stage["stage"] + (" " + stage["indexName"] if "indexName" in stage else ""))
        stage = stage.get("inputStage")
    stats = info["executionStats"]
    return (" → ".join(steps) + f" | вернул {stats['nReturned']}"
            + f" | ключей {stats['totalKeysExamined']} | документов {stats['totalDocsExamined']}")

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.admin.command("ping")
print("Заготовка главы 3.3 подключилась к серверу. Замените проверочные строки примером из главы.")
