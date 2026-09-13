#!/bin/sh
# Запускает файл решения из его папки. Чем запускать, решает расширение.
#
#   run practice01/solution.py
#   run practice01\solution.go      обратные слэши из PowerShell тоже годятся

set -e

if [ $# -eq 0 ]; then
  echo "Укажите файл, например: docker compose run --rm python practice01/solution.py" >&2
  exit 2
fi

target=$(printf '%s' "$1" | tr '\\' '/' | sed 's#^\./##')
shift

if [ ! -f "/work/$target" ]; then
  echo "Файл не найден: $target (путь считается от корня репозитория)" >&2
  exit 2
fi

cd "/work/$(dirname "$target")"
file=$(basename "$target")

case "$file" in
  *.py)
    exec python "$file" "$@" ;;
  *.rb)
    exec ruby "$file" "$@" ;;
  *.go)
    exec go run "$file" "$@" ;;
  *.cpp)
    binary="/tmp/${file%.cpp}"
    driver=$(pkg-config --list-all | awk '/^libmongocxx/ { print $1; exit }')
    # shellcheck disable=SC2046
    g++ -std=c++17 "$file" -o "$binary" $(pkg-config --cflags --libs "$driver")
    exec "$binary" "$@" ;;
  *)
    echo "Не знаю, чем запускать $file: нужен .py, .rb, .go или .cpp" >&2
    exit 2 ;;
esac
