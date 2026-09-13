# mongodb-practice — стенд и практики курса

Репозиторий к «Справочнику по MongoDB»: учебные базы, практические работы с
готовыми решениями и контрольные точки.

Справочник: **https://maximbytecamp.github.io/mongodb_theory_makarov/**
(исходник — [MaximBytecamp/mongodb_theory_makarov](https://github.com/MaximBytecamp/mongodb_theory_makarov)).
Автор курса — Макаров Максим Николаевич.

## Что внутри

```
compose.yaml    стенд: сервер, учебные базы, mongo-express, запуск кода
docker/         образы с драйверами Python, Ruby, Go и C++
.devcontainer/  тот же стенд в браузере через GitHub Codespaces
stend/          данные: seed/*.json и load.sh для сервера без Docker
hello/          проверка подключения на четырёх языках
practice01/     ПР-01 «Каталог товаров»: задание, решение, самопроверка
kt01/           КТ-01 «Инвентаризация склада»: задание и бланк отчёта
```

## Три способа поднять стенд

| Способ | Что нужно | Когда подходит |
|---|---|---|
| **А. Docker** | Docker Desktop | свой ноутбук, домашний компьютер |
| **Б. Codespaces** | браузер и аккаунт GitHub | ничего нельзя установить |
| **В. Сервер уже стоит** | MongoDB, Python, `bash` | Compass и сервер установились штатно |

Пошагово, с кадрами и типичными ошибками — глава
[1.1а «Если Compass не подключается: стенд в Docker»](https://maximbytecamp.github.io/mongodb_theory_makarov/temy/01a-stend-v-docker/index.html).

### А. Docker

Команды одинаковые в PowerShell, cmd и терминале macOS/Linux.

```bash
git clone https://github.com/MaximBytecamp/mongodb-practice.git
cd mongodb-practice

docker compose up -d                              # сервер + учебные базы + mongo-express
docker compose run --rm python hello/hello.py     # проверка: Товаров 21, Заказов 120
```

Что поднимется:

| Сервис | Адрес | Зачем |
|---|---|---|
| `mongo` | `mongodb://localhost:27017` | сервер MongoDB 7; к нему подключается и Compass, если он установлен |
| `seed` | — | один раз заливает учебные базы и завершается |
| `mongo-express` | http://localhost:8081, `student` / `student` | базы и документы в браузере вместо Compass |
| `python`, `ruby`, `go`, `cpp` | — | запускают решения; ставить языки на компьютер не нужно |

Код запускается одной командой, язык выбирается по расширению файла.
Путь пишется от корня репозитория:

```bash
docker compose run --rm python practice01/solution.py
docker compose run --rm ruby   practice01/solution.rb
docker compose run --rm go     practice01/solution.go
docker compose run --rm cpp    practice01/solution.cpp   # первая сборка образа — несколько минут

docker compose run --rm python practice01/check.py       # самопроверка, одна на все языки
```

Внутри контейнеров сервер называется не `localhost`, а `mongo`: адрес
`mongodb://mongo:27017/` уже лежит в переменной `MONGO_URI`, и все решения
читают его оттуда.

Консоль сервера: `docker exec -it course-mongo mongosh`.

Если порт 27017 уже занят (например, MongoDB установлена в систему), создайте
в корне репозитория файл `.env` и подключайтесь к `localhost:27018`:

```powershell
Set-Content .env "MONGO_PORT=27018"      # Windows, PowerShell
```

```bash
echo "MONGO_PORT=27018" > .env           # macOS и Linux
```

В PowerShell не используйте `echo … > .env`: Windows PowerShell 5 пишет такой
файл в UTF-16, и Docker Compose его не прочитает.

На Linux вместо Docker Desktop ставится Docker Engine:
`curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh`,
затем `sudo usermod -aG docker $USER` и повторный вход в систему.

### Б. Codespaces

1. На странице репозитория: **Code → Codespaces → Create codespace on main**.
2. Подождать 3–5 минут: соберётся окружение, поднимутся сервер и учебные базы.
3. В терминале редактора:

```bash
python hello/hello.py
cd practice01 && python solution.py && python check.py
```

Go и Ruby установлены там же (`cd practice01 && go run solution.go`,
`ruby solution.rb`). Для C++ используйте способ А.

mongo-express открывается на вкладке **Ports** → порт 8081. Бесплатно GitHub
даёт 120 ядро-часов в месяц — около 60 часов на стандартной машине. Останавливайте
codespace, когда закончили: **Code → Codespaces → … → Stop codespace**.

### В. Сервер уже стоит

```bash
bash stend/load.sh                 # учебные базы в mongodb://localhost:27017
pip install -r requirements.txt
cd practice01 && python3 solution.py && python3 check.py
```

## Учебные базы

| База | Коллекции | Для чего |
|---|---|---|
| `shop` | `products` 21, `orders` 120, `customers` 20 | основная: CRUD, агрегация, витрины |
| `hh` | `resumes` 9, `vacancies` 8, `companies` 8, `interviews` 60 | язык фильтров |
| `logs` | `events` 1200 | индексы, время, группировки |
| `org` | `employees` 15, `categories` 10 | иерархии и связи |
| `sandbox` | `products` 21, `warehouse` 21 | песочница: здесь можно ломать |

Базы `shop`, `hh`, `logs`, `org` — эталонные, их не меняют: на них построены
все примеры и числа в справочнике. Все упражнения идут в `sandbox`.

Данные детерминированные: `stend/seed/generate.py` собирает их с фиксированным
зерном, поэтому у всех получаются одни и те же числа.

## Если данные разъехались

Не чините руками — пересоздайте:

```bash
docker compose run --rm reset      # Docker
bash stend/load.sh                 # сервер без Docker
```

Коллекции перезаписываются целиком, поэтому повторный запуск безопасен и не
двоит документы.

## Остановить и удалить

```bash
docker compose stop        # остановить; данные останутся
docker compose down        # удалить контейнеры; данные останутся в томе
docker compose down -v     # удалить всё вместе с данными
```

## Практические работы

| Работа | Тема | После модуля |
|---|---|---|
| [practice01](practice01/) | Каталог товаров: витрина и правки | 1 · подключение и первые данные |

У каждой работы: `README.md` с заданием, решение на четырёх языках и
`check.py` — самопроверка по состоянию базы.

```bash
docker compose run --rm reset                        # чистые данные
docker compose run --rm python practice01/solution.py
docker compose run --rm python practice01/check.py   # Пройдено 12 из 12
```

## Контрольные точки

| Точка | Тема | Что сдаётся |
|---|---|---|
| [kt01](kt01/) | Инвентаризация склада | скрипт и отчёт `inventory.md` |

Решения контрольных точек в репозитории не публикуются: их разбирают на приёмке.
