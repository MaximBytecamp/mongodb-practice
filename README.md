# mongodb-practice — стенд и практики курса

Репозиторий к «Справочнику по MongoDB»: учебные базы, практические работы с
готовыми решениями и контрольные точки.

Справочник: **https://maximbytecamp.github.io/mongodb_theory_makarov/**
(исходник — [MaximBytecamp/mongodb_theory_makarov](https://github.com/MaximBytecamp/mongodb_theory_makarov)).
Автор курса — Макаров Максим Николаевич.

## Что внутри

```
stend/          сервер и данные: docker-compose.yml, seed/*.json, load.sh
practice01/     ПР-01 «Каталог товаров»: задание, решение, самопроверка
kt01/           КТ-01 «Инвентаризация склада»: задание и бланк отчёта
```

## Поднять стенд

Нужен Docker и Python 3 с `pymongo`.

```bash
git clone https://github.com/MaximBytecamp/mongodb-practice.git
cd mongodb-practice/stend

docker compose up -d       # MongoDB 7 на localhost:27017
bash load.sh               # залить учебные базы
```

Проверка:

```bash
docker exec course-mongo mongosh --quiet --eval \
  'db.getSiblingDB("shop").orders.countDocuments()'    # 120
```

Если MongoDB у вас уже поднята другим способом, `docker compose up` пропустите:
`load.sh` работает с любым сервером, адрес задаётся переменной `MONGO_URI`.

## Учебные базы

| База | Коллекции | Для чего |
|---|---|---|
| `shop` | `products` 21, `orders` 120, `customers` 20 | основная: CRUD, агрегация, витрины |
| `hh` | `resumes` 9, `vacancies` 8, `companies` 8, `interviews` 60 | язык фильтров |
| `logs` | `events` 1200 | индексы, время, группировки |
| `org` | `employees` 15, `categories` 10 | иерархии и связи |
| `sandbox` | `products` 21 | песочница: здесь можно ломать |

Базы `shop`, `hh`, `logs`, `org` — эталонные, их не меняют: на них построены
все примеры и числа в справочнике. Все упражнения идут в `sandbox`.

Данные детерминированные: `seed/generate.py` собирает их с фиксированным
зерном, поэтому у всех получаются одни и те же числа. Пересобрать:

```bash
cd stend/seed && python3 generate.py && cd .. && bash load.sh
```

## Если данные разъехались

Не чините руками — пересоздайте:

```bash
bash stend/load.sh
```

Скрипт перезаписывает коллекции целиком (`--drop`), поэтому повторный запуск
безопасен и не двоит документы.

## Практические работы

| Работа | Тема | После модуля |
|---|---|---|
| [practice01](practice01/) | Каталог товаров: витрина и правки | 1 · подключение и первые данные |

У каждой работы: `README.md` с заданием, `solution.py` с разобранным решением
и `check.py` — самопроверка по состоянию базы.

```bash
cd practice01
bash ../stend/load.sh
python3 solution.py
python3 check.py          # Пройдено 12 из 12
```

## Контрольные точки

| Точка | Тема | Что сдаётся |
|---|---|---|
| [kt01](kt01/) | Инвентаризация склада | скрипт и отчёт `inventory.md` |

Решения контрольных точек в репозитории не публикуются: их разбирают на приёмке.
