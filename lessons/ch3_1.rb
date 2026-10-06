# Глава 3.1 · Правила на уровне базы: $jsonSchema — заготовка для примеров.
#
# Перед главой верните базы в исходное состояние (глава 1.0, §8).
# Пример из главы вставьте под чертой вместо проверочных строк и запустите:
#
#     cd lessons
#     ruby ch3_1.rb
#
# Следующий пример вставляйте вместо предыдущего: каждый пример запускается
# один раз и по порядку — именно так получены выводы в справочнике.

require "mongo"

Mongo::Logger.logger.level = Logger::WARN

client = Mongo::Client.new(ENV.fetch("MONGO_URI", "mongodb://localhost:27017/"))
db = client.use("shop").database
products = db[:products]
orders = db[:orders]
events = client.use("logs").database[:events]
sandbox = client.use("sandbox").database
box = sandbox[:products]   # песочница: здесь можно менять

catalog = sandbox[:catalog]                   # коллекция без правила, §1

# Правило товара (§2 главы): обязательные поля и типы значений.
schema = {
  "bsonType" => "object",
  "required" => ["sku", "title", "category", "price"],
  "properties" => {
    "sku" => { "bsonType" => "string" },
    "title" => { "bsonType" => "string" },
    "category" => { "bsonType" => "string" },
    "price" => { "bsonType" => "number", "minimum" => 1 },
  },
}

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.database.command(ping: 1)
puts "Заготовка главы 3.1 подключилась к серверу. Замените проверочные строки примером из главы."
