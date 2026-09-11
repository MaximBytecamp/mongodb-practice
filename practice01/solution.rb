# Практическая работа 01 — эталонное решение на Ruby.
#
# Те же две задачи, что и в solution.py:
#   1. Витрина каталога: страница списка товаров, как её отдаёт бэкенд.
#   2. Приёмка поставки: обновить остатки, завести новинки, снять с продажи.
#
# Запуск:
#     ruby solution.rb
#     MONGO_URI=mongodb://host:27017 ruby solution.rb
#
# Перед запуском данные должны быть залиты: bash ../stend/load.sh
# Драйвер: gem install mongo

require "mongo"

Mongo::Logger.logger.level = Logger::WARN

# Константу URI занимать нельзя: так называется модуль стандартной библиотеки,
# и драйвер после этого не разберёт строку подключения.
SERVER = ENV.fetch("MONGO_URI", "mongodb://localhost:27017/")
CLIENT = Mongo::Client.new(SERVER)

PRODUCTS = CLIENT.use("shop").database[:products]     # только читаем
BOX = CLIENT.use("sandbox").database[:products]       # здесь меняем

# ── Задача 1. Витрина каталога ───────────────────────────────────
def catalog_page(category: nil, page: 1, per_page: 5, sort_field: "price", desc: true)
  # Одна страница каталога — то, что бэкенд отдаёт фронтенду.
  # Возвращает [items, total]: список товаров и сколько их всего под фильтром.
  query = {}
  query["category"] = category if category

  total = PRODUCTS.count_documents(query)

  # Второй ключ сортировки делает порядок однозначным: без него товары
  # с одинаковой ценой прыгают между страницами.
  order = { sort_field => desc ? -1 : 1, "sku" => 1 }

  items = PRODUCTS
          .find(query, projection: { "_id" => 0, "sku" => 1, "title" => 1,
                                     "price" => 1, "rating" => 1 })
          .sort(order)
          .skip((page - 1) * per_page)
          .limit(per_page)
          .to_a

  [items, total]
end

def show_catalog
  puts "=" * 62
  puts "ЗАДАЧА 1 · витрина каталога"
  puts "=" * 62

  items, total = catalog_page(category: "ноутбуки", page: 1, per_page: 3)
  pages = (total + 2) / 3
  puts "\nНоутбуки, страница 1 из #{pages} (всего #{total}):"
  items.each { |item| puts format("  %7d ₽  %-32s рейтинг %s", item["price"], item["title"], item["rating"]) }

  items, = catalog_page(category: "ноутбуки", page: 2, per_page: 3)
  puts "\nСтраница 2:"
  items.each { |item| puts format("  %7d ₽  %-32s рейтинг %s", item["price"], item["title"], item["rating"]) }

  items, total = catalog_page(page: 1, per_page: 5)
  puts "\nВесь каталог, топ-5 по цене (всего #{total}):"
  items.each { |item| puts format("  %7d ₽  %s", item["price"], item["title"]) }

  # Пустая страница — обычное дело: запросили дальше, чем есть данных.
  items, total = catalog_page(category: "ноутбуки", page: 9, per_page: 3)
  puts "\nСтраница 9 из существующих #{pages}: найдено #{items.size} товаров, " \
       "total по-прежнему #{total}"
  total
end

# ── Задача 2. Приёмка поставки ───────────────────────────────────
ARRIVED = {                # что приехало: артикул → сколько штук
  "SKU-NB-001" => 5,
  "SKU-PH-006" => 12,
  "SKU-PR-010" => 3,
}.freeze

NEW_ITEMS = [              # новые позиции в каталоге
  { "_id" => "p-101", "sku" => "SKU-AC-101", "title" => "Подставка для ноутбука",
    "brand" => "OEM", "category" => "аксессуары", "price" => 2490, "reviews" => 0 },
  { "_id" => "p-102", "sku" => "SKU-AC-102", "title" => "Чехол для планшета",
    "brand" => "OEM", "category" => "аксессуары", "price" => 1890, "reviews" => 0 },
].freeze

DISCONTINUED = ["SKU-CP-018"].freeze   # снято с продажи

def receive_supply
  puts "\n" + "=" * 62
  puts "ЗАДАЧА 2 · приёмка поставки"
  puts "=" * 62

  report = {}

  # 1. Остатки: $inc прибавляет на сервере, не читая значение на клиент.
  #    Если поля qty_total нет — оно появится с этим значением.
  updated = 0
  ARRIVED.each do |sku, qty|
    result = BOX.update_one({ "sku" => sku }, { "$inc" => { "qty_total" => qty } })
    puts "  ВНИМАНИЕ: артикул #{sku} не найден в каталоге" if result.matched_count.zero?
    updated += result.modified_count
  end
  report["остатки обновлены"] = updated

  # 2. Новинки: свой _id делает повторный запуск безопасным —
  #    вторая попытка упрётся в E11000, а не создаст дубли.
  begin
    result = BOX.insert_many(NEW_ITEMS, ordered: false)
    report["новых позиций"] = result.inserted_count
  rescue Mongo::Error::BulkWriteError => error
    # Частичный отказ пачки — это BulkWriteError, а не OperationFailure:
    # в result лежит список документов, которые не прошли.
    already = error.result["writeErrors"].size
    report["новых позиций"] = NEW_ITEMS.size - already
    puts "  #{already} позиций уже были в каталоге — пропущены"
  end

  # 3. Снятие с продажи: сначала считаем, потом удаляем.
  query = { "sku" => { "$in" => DISCONTINUED } }
  puts "\n  под снятие с продажи попало: #{BOX.count_documents(query)}"
  BOX.find(query, projection: { "_id" => 1, "title" => 1, "price" => 1 })
     .each { |doc| puts "    #{doc.inspect}" }
  report["снято с продажи"] = BOX.delete_many(query).deleted_count

  # 4. Бесплатная доставка для всей категории — одним запросом.
  query = { "category" => "аксессуары" }
  puts "\n  бесплатная доставка коснётся: #{BOX.count_documents(query)} товаров"
  report["бесплатная доставка"] =
    BOX.update_many(query, { "$set" => { "free_shipping" => true } }).modified_count

  report["итого в каталоге"] = BOX.count_documents({})

  puts "\nОтчёт о приёмке:"
  report.each { |name, value| puts format("  %-24s %s", name, value) }
  report
end

if __FILE__ == $PROGRAM_NAME
  show_catalog
  receive_supply
  CLIENT.close
end
