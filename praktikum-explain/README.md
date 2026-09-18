# Практикум · Онлайн-разбор запроса: explain и индексы

После модуля 1 справочника. Работа идёт в оболочке `mongosh` на базе `logs`,
коллекция `events` — 1200 записей журнала сервиса. Код на языках драйверов
здесь не нужен: все команды одинаковы для всех.

Полное задание с объяснениями — на странице справочника:
https://maximbytecamp.github.io/mongodb_theory_makarov/praktiki/onlayn-razbor-zaprosa/

Ниже — то же самое коротко, чтобы можно было работать из репозитория.

## Стенд

```bash
docker compose up -d                      # сервер и учебные базы
docker compose run --rm reset             # исходное состояние: 1200 событий, индексов нет
docker compose exec mongo mongosh logs    # оболочка на базе logs
```

Без Docker: `bash stend/load.sh` и `mongosh "mongodb://localhost:27017/logs"`.

## Что разбирается

Три числа из `explain("executionStats")`:

- `nReturned` — сколько документов запрос вернул;
- `totalDocsExamined` — сколько документов сервер для этого прочитал;
- `totalKeysExamined` — сколько записей индекса он просмотрел.

Когда `totalDocsExamined` равно числу документов в коллекции, сервер прочитал
её целиком — в плане это шаг `COLLSCAN`. Когда чтению предшествует индекс,
появляется шаг `IXSCAN`.

## Задачи

**1. Запрос без индекса.** Снять план запроса, записать три числа и шаги,
разобрать план на https://dfrancour.dev/tools/mongodb-paste-the-plan

```js
db.events.find({
  service: "payments",
  level: "error",
  duration_ms: { $gt: 500 }
}).sort({ ts: -1 })
```

**2. Индекс по правилу ESR.** Разобрать запрос на Equality, Sort, Range,
создать индекс `{ service: 1, level: 1, ts: -1, duration_ms: 1 }`, снять план
заново и сравнить.

**3. Порядок полей.** Те же четыре поля в другом порядке — два варианта,
`{ duration_ms, service, level, ts }` и `{ service, level, duration_ms, ts }`.
Заполнить таблицу и объяснить разницу.

**4. Самостоятельно.** Подобрать индекс для второго запроса:

```js
db.events.find({ level: "error" }).sort({ duration_ms: -1 }).limit(10)
```

## Как снять план

Одной командой в терминале (не в оболочке — вывод перенаправляется в файл
на вашем компьютере):

```bash
docker compose exec -T mongo mongosh --quiet logs --eval 'EJSON.stringify(db.events.find({ service: "payments", level: "error", duration_ms: { $gt: 500 } }).sort({ ts: -1 }).explain("executionStats"))' > plan-1.json
```

В PowerShell вместо `>` нужен `Out-File` с явной кодировкой: обычное
перенаправление в Windows PowerShell 5.1 сохраняет файл в UTF-16, и
визуализатор такой файл не прочитает.

```powershell
docker compose exec -T mongo mongosh --quiet logs --eval 'EJSON.stringify(db.events.find({ service: "payments", level: "error", duration_ms: { $gt: 500 } }).sort({ ts: -1 }).explain("executionStats"))' | Out-File plan-1.json -Encoding utf8
```

Команда набирается одной строкой. Если разбить её на несколько строк и
вставить в терминал целиком, оболочка выполнит каждую строку отдельно и
команда оборвётся на первом переносе.

Флаг `-T` отключает выделение терминала, без него перенаправление в файл
ломается. `--quiet` убирает приветствие сервера, чтобы в файле остался
только JSON.

Чтобы не набирать эту строку шесть раз, в папке лежит скрипт. Он снимает все
шесть планов, сам создаёт и удаляет индексы и печатает числа каждого плана:

```bash
bash plans.sh                 # планы лягут в текущую папку
```

```powershell
powershell -ExecutionPolicy Bypass -File plans.ps1
```

Скрипт нужен для проверки и для повторного прогона. Задачи 1–4 проходятся
руками: числа до и после каждый снимает сам.

## Что сдавать

Папка `praktikum-explain` в вашем репозитории, склонированном в `work/`:

```
work/praktikum-explain/
  README.md         отчёт: сводная таблица, числа, ссылки на разборы, ответы
  plan-1.json       запрос 1 без индекса
  plan-2.json       запрос 1 с индексом по ESR
  plan-3a.json      вариант А из задачи 3
  plan-3b.json      вариант Б из задачи 3
  plan-4-bez.json   запрос 2 без индекса
  plan-4.json       запрос 2 с вашим индексом
```

Сводная таблица в начале отчёта — шесть строк по числу снятых планов,
в каждой индекс, три числа и шаги плана.

## Проверить себя

В папке `etalon/` лежат все шесть планов, снятых на исходных данных. Если ваши
числа не сошлись с эталонными, проверьте два места: сброшены ли базы командой
`docker compose run --rm reset` и не остался ли в коллекции индекс от
предыдущей задачи. Список индексов — `db.events.getIndexes()`.

Числа там те же, которые вы получите сами. Разбор в отчёте пишется по своим
планам.
