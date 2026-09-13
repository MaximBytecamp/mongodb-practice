#!/usr/bin/env bash
# Заливка учебных данных в MongoDB.
#
#   bash load.sh                     # сервер на localhost:27017
#   MONGO_URI=... bash load.sh       # другой адрес
#
# Скрипт полностью пересоздаёт четыре учебные базы: shop, hh, logs, org.
# Данные детерминированные, поэтому после повторного запуска все числа
# в справочнике и в ответах к практикам совпадут. --maintainInsertionOrder
# сохраняет порядок документов из файла: запросы без сортировки выводят их
# в том же порядке, что в справочнике.

set -euo pipefail

URI="${MONGO_URI:-mongodb://localhost:27017}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SEED="$HERE/seed"

# mongoimport берём либо локальный, либо из контейнера с сервером.
if command -v mongoimport >/dev/null 2>&1; then
  import() { mongoimport --uri "$URI" --db "$1" --collection "$2" --file "$3" --jsonArray --drop --maintainInsertionOrder --quiet; }
elif CONTAINER="$(docker ps --format '{{.Names}}' | grep -m1 -E "^${MONGO_CONTAINER:-course-mongo|hh-mongo}$")"; then
  echo "mongoimport не найден локально — работаем через контейнер $CONTAINER"
  import() { docker exec -i "$CONTAINER" mongoimport --db "$1" --collection "$2" --jsonArray --drop --maintainInsertionOrder --quiet < "$3"; }
else
  echo "Нужен mongoimport или запущенный контейнер с сервером (MONGO_CONTAINER=имя)." >&2
  exit 1
fi

for file in "$SEED"/*.json; do
  name="$(basename "$file" .json)"   # например shop.orders
  db="${name%%.*}"
  collection="${name#*.}"
  import "$db" "$collection" "$file"
  printf '%-22s ← %s\n' "$db.$collection" "$(basename "$file")"
done

echo
echo "Готово. Проверка:"
echo "  mongosh \"$URI\" --quiet --eval 'db.getSiblingDB(\"shop\").orders.countDocuments()'   # 120"
