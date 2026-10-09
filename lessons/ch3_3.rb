# Глава 3.3 · Как читать explain: COLLSCAN против IXSCAN — заготовка для примеров.
#
# Перед главой верните базы в исходное состояние (глава 1.0, §8).
# Пример из главы вставьте под чертой вместо проверочных строк и запустите:
#
#     cd lessons
#     ruby ch3_3.rb
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

journal = sandbox[:events]        # копия журнала: на ней строятся индексы главы

# План и счётчики explain одной строкой: стадии | вернул | ключей | документов.
def summary(coll, find)
  info = coll.database.command(explain: { find: coll.name }.merge(find), verbosity: "executionStats").first
  stage = info["queryPlanner"]["winningPlan"]
  stage = stage["queryPlan"] || stage     # MongoDB 7 кладёт план во вложенное поле
  steps = []
  while stage
    steps << stage["stage"] + (stage["indexName"] ? " " + stage["indexName"] : "")
    stage = stage["inputStage"]
  end
  stats = info["executionStats"]
  steps.join(" → ") + " | вернул #{stats["nReturned"]}" +
    " | ключей #{stats["totalKeysExamined"]} | документов #{stats["totalDocsExamined"]}"
end

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.database.command(ping: 1)
puts "Заготовка главы 3.3 подключилась к серверу. Замените проверочные строки примером из главы."
