#!/usr/bin/env bash
# Снимает все шесть планов выполнения для практикума «Онлайн-разбор запроса».
#
#   bash plans.sh              # планы лягут в текущую папку
#   bash plans.sh etalon       # или в указанную
#
# Скрипт создаёт индексы, снимает план и удаляет их за собой: коллекция
# остаётся в том же виде, в каком была до запуска. Данные не меняются.
#
# Работает и через контейнер стенда, и с локальным mongosh:
#   MONGO_URI=mongodb://localhost:27017 bash plans.sh

set -euo pipefail

OUT="${1:-.}"
mkdir -p "$OUT"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Где выполнять команды: локальный mongosh, названный контейнер или стенд из compose.
if command -v mongosh >/dev/null 2>&1; then
  URI="${MONGO_URI:-mongodb://localhost:27017}"
  shell() { mongosh "$URI/logs" --quiet --eval "$1"; }
elif [ -n "${MONGO_CONTAINER:-}" ]; then
  shell() { docker exec -i "$MONGO_CONTAINER" mongosh logs --quiet --eval "$1"; }
elif [ -n "$(docker compose -f "$HERE/../compose.yaml" ps -q mongo 2>/dev/null)" ]; then
  shell() { docker compose -f "$HERE/../compose.yaml" exec -T mongo mongosh logs --quiet --eval "$1"; }
else
  echo "Нужен mongosh или запущенный стенд: docker compose up -d" >&2
  echo "Другой контейнер с сервером: MONGO_CONTAINER=имя bash plans.sh" >&2
  exit 1
fi

# Запрос 1: медленные ошибки платежей, свежие сверху.
Q1='db.events.find({ service: "payments", level: "error", duration_ms: { $gt: 500 } }).sort({ ts: -1 })'
# Запрос 2: десять самых долгих ошибок.
Q2='db.events.find({ level: "error" }).sort({ duration_ms: -1 }).limit(10)'

# $1 — имя файла, $2 — запрос, $3 — индекс (пустая строка, если без индекса).
plan() {
  local file="$1" query="$2" index="${3:-}" make="" drop=""
  if [ -n "$index" ]; then
    # Имя индексу не задаём: в плане должно стоять то же имя, которое получит
    # студент после обычного createIndex. Удаляем по описанию ключей.
    make="db.events.createIndex($index);"
    drop="db.events.dropIndex($index);"
  fi

  # Весь запуск обёрнут в функцию: mongosh печатает только то, что она вернула,
  # то есть строку плана. Вывод createIndex и dropIndex в файл не попадает.
  shell "(() => { $make const plan = EJSON.stringify($query.explain('executionStats')); $drop return plan; })()" > "$OUT/$file"
  printf '%-22s %s\n' "$file" "$(python3 - "$OUT/$file" <<'PY'
import json, sys
e = json.load(open(sys.argv[1]))["executionStats"]
stages, s = [], e["executionStages"]
while s:
    stages.append(s["stage"])
    s = s.get("inputStage")
print("возвращено %d, документов %d, ключей %d — %s"
      % (e["nReturned"], e["totalDocsExamined"], e["totalKeysExamined"],
         " ← ".join(stages)))
PY
)"
}

echo "Запрос 1 — ошибки payments дольше 500 мс, сортировка по времени"
plan plan-1.json       "$Q1"
plan plan-2.json       "$Q1" '{ service: 1, level: 1, ts: -1, duration_ms: 1 }'
plan plan-3a.json      "$Q1" '{ duration_ms: 1, service: 1, level: 1, ts: -1 }'
plan plan-3b.json      "$Q1" '{ service: 1, level: 1, duration_ms: 1, ts: -1 }'

echo
echo "Запрос 2 — десять самых долгих ошибок"
plan plan-4-bez.json   "$Q2"
plan plan-4.json       "$Q2" '{ level: 1, duration_ms: -1 }'

echo
echo "Индексы коллекции после работы скрипта:"
shell 'db.events.getIndexes().map(i => i.name).join(", ")'
