# Глава 1.4 · Чтение: find, find_one и курсор — заготовка для примеров.
#
# Глава только читает данные — сбрасывать базы не нужно.
# Пример из главы вставьте в конец файла, под чертой, и запустите:
#
#     cd lessons
#     ruby ch1_4.rb
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

# ── пример из главы ───────────────────────────────────────────
