# Глава 1.7 · Обновление: $set, $inc, $unset — заготовка для примеров.
#
# Перед главой верните базы в исходное состояние (глава 1.0, §8).
# Пример из главы вставьте под чертой вместо проверочных строк и запустите:
#
#     cd lessons
#     ruby ch1_7.rb
#
# Следующий пример вставляйте вместо предыдущего: каждый пример запускается
# один раз и по порядку — именно так получены выводы в справочнике.

require "mongo"

Mongo::Logger.logger.level = Logger::WARN

client = Mongo::Client.new(ENV.fetch("MONGO_URI", "mongodb://localhost:27017/"))
db = client.use("shop").database
products = db[:products]
orders = db[:orders]
box = client.use("sandbox").database[:products]   # песочница: здесь можно менять

# Товары, которые добавлялись в песочницу в главе 1.3. Если песочницу
# сбрасывали, заготовка добавит их снова — обновлять будет что.
if box.count_documents({ "_id" => "p-101" }).zero?
  box.insert_many([
    { "_id" => "p-101", "title" => "Подставка для ноутбука", "category" => "аксессуары", "price" => 2490 },
    { "_id" => "p-102", "title" => "Чехол для планшета", "category" => "аксессуары", "price" => 1890 },
    { "sku" => "SKU-AC-022", "title" => "Коврик для мыши XL", "brand" => "OEM",
      "category" => "аксессуары", "price" => 1290 },
  ])
end

orders_box = client.use("sandbox").database[:orders]   # копия заказов: их можно менять
orders_box.insert_many(orders.find.to_a) if orders_box.count_documents({}).zero?

stats = client.use("sandbox").database[:product_stats] # счётчики просмотров для §6
stats.drop                                             # каждый запуск заготовки считает с нуля
require "date"
today = Date.today.iso8601

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.database.command(ping: 1)
puts "Заготовка главы 1.7 подключилась к серверу. Замените проверочные строки примером из главы."
