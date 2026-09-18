"""Снимает план выполнения запроса практикума и кладёт его в файл.

Нужен там, где длинная команда mongosh не переживает копирования: в cmd.exe
одинарные кавычки не экранируют, при вставке в несколько строк оболочка
выполняет каждую строку отдельно. Здесь кавычек в командной строке нет совсем.

    docker compose run --rm python praktikum-explain/plan.py 1 plan-1.json
    docker compose run --rm python praktikum-explain/plan.py 2 plan-4.json

Первый аргумент — номер запроса практикума (1 или 2), второй — имя файла.
Файл ложится в work/praktikum-explain/ вашего репозитория; папку можно
задать третьим аргументом (путь считается от корня mongodb-practice).

Индексы скрипт не трогает: он снимает план того состояния коллекции, которое
вы сделали в оболочке. Поэтому порядок работы прежний — создали индекс в
mongosh, сняли план этим скриптом, разобрали его на сайте.
"""

import json
import sys
from pathlib import Path

from bson.json_util import RELAXED_JSON_OPTIONS, dumps
from pymongo import ASCENDING, DESCENDING, MongoClient

# Запросы практикума. Ключ — номер, который студент указывает первым аргументом.
QUERIES = {
    "1": {
        "opisanie": "ошибки payments дольше 500 мс, свежие сверху",
        "filtr": {"service": "payments", "level": "error", "duration_ms": {"$gt": 500}},
        "sort": [("ts", DESCENDING)],
        "limit": 0,
    },
    "2": {
        "opisanie": "десять самых долгих ошибок",
        "filtr": {"level": "error"},
        "sort": [("duration_ms", DESCENDING)],
        "limit": 10,
    },
}

PAPKA_PO_UMOLCHANIYU = "work/praktikum-explain"


def podskazka(tekst: str) -> None:
    print(tekst, file=sys.stderr)
    sys.exit(2)


def main() -> None:
    if len(sys.argv) < 3:
        podskazka(
            "Укажите номер запроса и имя файла, например:\n"
            "  docker compose run --rm python praktikum-explain/plan.py 1 plan-1.json"
        )

    nomer, imya = sys.argv[1], sys.argv[2]
    papka = sys.argv[3] if len(sys.argv) > 3 else PAPKA_PO_UMOLCHANIYU

    if nomer not in QUERIES:
        podskazka("Запрос бывает 1 или 2, а не %r. Смотрите README." % nomer)
    if not imya.endswith(".json"):
        podskazka("Имя файла должно оканчиваться на .json, например plan-1.json")

    zapros = QUERIES[nomer]

    # Раннер подключён к сети сервера, поэтому адрес тот же, что и на компьютере.
    client = MongoClient("mongodb://localhost:27017", serverSelectionTimeoutMS=5000)
    try:
        client.admin.command("ping")
    except Exception:
        podskazka(
            "Сервер не отвечает. Поднимите стенд: docker compose up -d\n"
            "и залейте данные: docker compose run --rm reset"
        )

    events = client["logs"]["events"]
    if events.count_documents({}) == 0:
        podskazka("Коллекция logs.events пуста. Залейте данные: docker compose run --rm reset")

    kursor = events.find(zapros["filtr"]).sort(zapros["sort"])
    if zapros["limit"]:
        kursor = kursor.limit(zapros["limit"])
    plan = kursor.explain()

    # Корень репозитория примонтирован в /work, вне контейнера это текущая папка.
    koren = Path("/work") if Path("/work").is_dir() else Path.cwd()
    cel = koren / papka
    cel.mkdir(parents=True, exist_ok=True)
    fayl = cel / imya

    # dumps из bson пишет даты и числа BSON так, как их ждёт визуализатор плана.
    fayl.write_text(dumps(plan, json_options=RELAXED_JSON_OPTIONS), encoding="utf-8")

    stats = plan["executionStats"]
    shagi, shag = [], stats["executionStages"]
    while shag:
        shagi.append(shag["stage"])
        shag = shag.get("inputStage")

    print("Запрос %s — %s" % (nomer, zapros["opisanie"]))
    print("  файл:                  %s/%s" % (papka, imya))
    print("  возвращено:            %d" % stats["nReturned"])
    print("  документов просмотрено:%d" % stats["totalDocsExamined"])
    print("  ключей просмотрено:    %d" % stats["totalKeysExamined"])
    print("  шаги плана:            %s" % " → ".join(reversed(shagi)))

    indeksy = [i["name"] for i in events.list_indexes()]
    print("  индексы коллекции:     %s" % ", ".join(indeksy))


if __name__ == "__main__":
    main()
