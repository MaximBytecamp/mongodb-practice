# Глава 3.5 · География: 2dsphere, $near, $geoWithin, $geoIntersects — заготовка для примеров.
#
# Перед главой верните базы в исходное состояние (глава 1.0, §8).
# Пример из главы вставьте под чертой вместо проверочных строк и запустите:
#
#     cd lessons
#     ruby ch3_5.rb
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

pickup = sandbox[:pickup]         # пункты выдачи: точки на карте
zones = sandbox[:zones]           # зоны доставки: многоугольники

# Покупатель в центре Ярославля: долгота, затем широта.
here = { "type" => "Point", "coordinates" => [39.8875, 57.6300] }

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.database.command(ping: 1)
puts "Заготовка главы 3.5 подключилась к серверу. Замените проверочные строки примером из главы."
