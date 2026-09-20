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

План снимают тремя способами. Результат одинаковый.

### Способ А — Compass, без терминала

Подключение к `mongodb://localhost:27017`, база `logs`, коллекция `events`.
Фильтр — в поле запроса, сортировка — в `Options → Sort`, дальше кнопка
**Explain** рядом с **Find**.

- вкладка **Visual Tree** — схема шагов и сводка `Query Performance Summary`;
- вкладка **Raw Output** — полный JSON плана и кнопка **Copy**.

Скопированный JSON вставляется прямо в поле на
https://dfrancour.dev/tools/mongodb-paste-the-plan — файл для этого не нужен.
Для отчёта файл всё равно понадобится: либо сохраните скопированное в
редакторе как `plan-N.json`, либо берите способ Б.

### Способ Б — скрипт, одинаково на любой системе

Запрос задан внутри `plan.py`, поэтому через оболочку не проходят ни кавычки,
ни фигурные скобки, ни знак `$`. Команда работает в PowerShell, `cmd.exe`,
Терминале macOS и Linux.

Скопируйте скрипт в свою рабочую папку — он сдаётся вместе с планами:

```bash
mkdir -p work/praktikum-explain
cp praktikum-explain/plan.py work/praktikum-explain/
```

```powershell
mkdir work\praktikum-explain
copy praktikum-explain\plan.py work\praktikum-explain\
```

Внутри файла меняется только блок `ЗАПРОС`: `FILTR`, `SORTIROVKA`, `PREDEL`
и `FAYL`. Запуск:

```
docker compose run --rm python work/praktikum-explain/plan.py
docker compose run --rm python work/praktikum-explain/plan.py plan-2.json
```

Имя файла одним словом в конце перекрывает `FAYL`. Файл ложится рядом со
скриптом. Скрипт печатает то, что записал:

```
Запрос к logs.events, всего документов в коллекции: 1200
  фильтр:                {"service": "payments", "level": "error", "duration_ms": {"$gt": 500}}
  сортировка:            [('ts', -1)]
  предел:                нет

  файл с планом:         plan-1.json
  возвращено:            33
  документов просмотрено:1200
  ключей просмотрено:    0
  шаги плана:            COLLSCAN → SORT
  индексы коллекции:     _id_
```

Последняя строка показывает индексы на момент замера — по ней видно, если от
предыдущей задачи остался лишний. Индексы скрипт не создаёт и не удаляет.

### Способ В — командой mongosh

Работает в PowerShell, Терминале macOS и Linux. В `cmd.exe` не работает: там
одинарные кавычки не группируют текст в один аргумент. Команда набирается
одной строкой.

```bash
docker compose exec -T mongo mongosh --quiet logs --eval 'EJSON.stringify(db.events.find({ service: "payments", level: "error", duration_ms: { $gt: 500 } }).sort({ ts: -1 }).explain("executionStats"))' > plan-1.json
```

```powershell
docker compose exec -T mongo mongosh --quiet logs --eval 'EJSON.stringify(db.events.find({ service: "payments", level: "error", duration_ms: { $gt: 500 } }).sort({ ts: -1 }).explain("executionStats"))' | Out-File plan-1.json -Encoding utf8
```

В PowerShell нужен `Out-File` с явной кодировкой: обычное перенаправление в
Windows PowerShell 5.1 сохраняет файл в UTF-16, и визуализатор его не
прочитает. Флаг `-T` отключает выделение терминала, `--quiet` убирает
приветствие сервера.

Для преподавателя в папке лежат `plans.sh` и `plans.ps1` — они снимают все
шесть планов разом, сами создают и удаляют индексы. Студенту они не нужны:
задачи проходятся руками.

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
