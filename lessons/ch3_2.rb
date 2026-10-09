# Глава 3.2 · Индексы: одиночные, составные, уникальные — заготовка для примеров.
#
# Перед главой верните базы в исходное состояние (глава 1.0, §8).
# Пример из главы вставьте под чертой вместо проверочных строк и запустите:
#
#     cd lessons
#     ruby ch3_2.rb
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
clients = sandbox[:customers]     # копия покупателей: уникальный индекс, §5–§7

# Стадии плана, который выбрал сервер, сверху вниз: FETCH → IXSCAN service_1.
def plan(coll, filtr, sort = nil)
  view = coll.find(filtr)
  view = view.sort(sort) if sort
  stage = view.explain["queryPlanner"]["winningPlan"]
  stage = stage["queryPlan"] || stage     # MongoDB 7 кладёт план во вложенное поле
  steps = []
  while stage
    steps << stage["stage"] + (stage["indexName"] ? " " + stage["indexName"] : "")
    stage = stage["inputStage"]
  end
  steps.join(" → ")
end

# ── пример из главы ───────────────────────────────────────────
# Проверочные строки: замените их примером из главы.
client.database.command(ping: 1)
puts "Заготовка главы 3.2 подключилась к серверу. Замените проверочные строки примером из главы."
