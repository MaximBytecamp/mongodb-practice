# Аудит журнала сервиса: что умеют правила, индексы и explain из модуля 3.
#
# Скрипт работает с копией журнала logs.events в песочнице: проверяет события
# правилом $jsonSchema и включает его как валидатор, снимает планы трёх
# запросов дежурного, строит под них индексы по правилу ESR и сравнивает
# планы до и после. База logs остаётся без изменений.
#
#     cd scripts
#     ruby logs_audit.rb
#
#     docker compose run --rm ruby scripts/logs_audit.rb    # в контейнере

require "mongo"

Mongo::Logger.logger.level = Logger::WARN

client = Mongo::Client.new(ENV.fetch("MONGO_URI", "mongodb://localhost:27017/"))
events = client.use("logs").database[:events]
sandbox = client.use("sandbox").database
audit = sandbox[:audit]

def title(text)
  puts "\n" + text
  puts "─" * text.length
end

# План и счётчики explain: стадии сверху вниз, вернул, ключей, документов.
def summary(sandbox, find)
  info = sandbox.command(explain: { find: "audit" }.merge(find), verbosity: "executionStats").first
  stage = info["queryPlanner"]["winningPlan"]
  stage = stage["queryPlan"] || stage
  steps = []
  while stage
    steps << stage["stage"] + (stage["indexName"] ? " " + stage["indexName"] : "")
    stage = stage["inputStage"]
  end
  stats = info["executionStats"]
  [steps.join(" → "), stats["nReturned"], stats["totalKeysExamined"], stats["totalDocsExamined"]]
end

# ── 1. Копия журнала ──────────────────────────────────────────────────────
title("1. Копия журнала в песочнице")

audit.drop
audit.insert_many(events.find.to_a)
puts "  событий в копии: #{audit.count_documents({})}"
puts "  индексов: #{audit.indexes.count}"

# ── 2. Правило для событий ────────────────────────────────────────────────
title("2. Правило для событий")

RULE = {
  "bsonType" => "object",
  "required" => ["ts", "service", "level", "route", "status", "duration_ms"],
  "properties" => {
    "ts" => { "bsonType" => "date" },
    "service" => { "enum" => ["api-gateway", "notifier", "orders", "payments", "search"] },
    "level" => { "enum" => ["info", "warn", "error"] },
    "route" => { "bsonType" => "string" },
    "status" => { "bsonType" => "int", "minimum" => 100, "maximum" => 599 },
    "duration_ms" => { "bsonType" => "number", "minimum" => 0 },
  },
}.freeze
puts "  соответствуют правилу:    #{audit.count_documents({ "$jsonSchema" => RULE })}"
puts "  не соответствуют правилу: #{audit.count_documents({ "$nor" => [{ "$jsonSchema" => RULE }] })}"

sandbox.command(collMod: "audit", validator: { "$jsonSchema" => RULE },
                validationLevel: "strict", validationAction: "error")
puts "  правило включено: strict, error"

# ── 3. Проверка правила на записи ─────────────────────────────────────────
title("3. Неверные события")

good = { "ts" => Time.utc(2026, 9, 2, 18, 30), "service" => "payments", "level" => "error",
         "route" => "/api/pay", "status" => 500, "duration_ms" => 300 }
wrong = [
  ["статус 700", good.merge("status" => 700)],
  ["уровень fatal", good.merge("level" => "fatal")],
  ["без времени", good.reject { |key, _| key == "ts" }],
]
wrong.each do |label, doc|
  begin
    audit.insert_one(doc)
    puts "  #{label.ljust(14)} записано"
  rescue Mongo::Error::OperationFailure => e
    reasons = []
    e.details["details"]["schemaRulesNotSatisfied"].each do |rule|
      if rule["operatorName"] == "required"
        reasons += rule["missingProperties"].map { |name| "нет #{name}" }
      end
      if rule["operatorName"] == "properties"
        rule["propertiesNotSatisfied"].each do |prop|
          reasons += prop["details"].map { |d| "#{prop["propertyName"]}: #{d["operatorName"]}" }
        end
      end
    end
    puts "  #{label.ljust(14)} отклонено, код #{e.code}: #{reasons.join(", ")}"
  end
end
puts "  событий в копии: #{audit.count_documents({})}"

# ── 4. Запросы дежурного без индексов ─────────────────────────────────────
QUERIES = [
  ["ошибки платежей", { filter: { "service" => "payments", "status" => { "$gte" => 500 } },
                        sort: { "ts" => -1 }, limit: 5 }],
  ["медленный поиск", { filter: { "service" => "search", "duration_ms" => { "$gte" => 1000 } },
                        sort: { "duration_ms" => -1 }, limit: 5 }],
  ["события покупателя", { filter: { "user_id" => "c-015" }, sort: { "ts" => -1 } }],
].freeze

def report(sandbox)
  puts "  #{"запрос".ljust(20)} #{"вернул".rjust(6)} #{"ключей".rjust(7)} #{"документов".rjust(11)}"
  QUERIES.each do |name, find|
    plan, returned, keys, docs = summary(sandbox, find)
    puts "  #{name.ljust(20)} #{returned.to_s.rjust(6)} #{keys.to_s.rjust(7)} #{docs.to_s.rjust(11)}"
    puts "      #{plan}"
  end
end

title("4. Запросы дежурного без индексов")
report(sandbox)

# ── 5. Индексы по правилу ESR ─────────────────────────────────────────────
title("5. Индексы: равенство, сортировка, диапазон")

[{ "service" => 1, "ts" => -1, "status" => 1 },
 { "service" => 1, "duration_ms" => -1 },
 { "user_id" => 1, "ts" => -1 }].each do |keys|
  audit.indexes.create_one(keys)
  puts "  создан #{audit.indexes.get(keys)["name"]}"
end

# ── 6. Те же запросы с индексами ──────────────────────────────────────────
title("6. Запросы дежурного с индексами")
report(sandbox)

# ── 7. Пять последних ошибок платежей ─────────────────────────────────────
title("7. Пять последних ошибок платежей")

_, find = QUERIES[0]
audit.find(find[:filter]).sort({ "ts" => -1 }).limit(find[:limit]).each do |doc|
  puts "  #{doc["ts"].utc.strftime("%d.%m %H:%M:%S")}  #{doc["route"].ljust(18)} #{doc["status"]}  " \
       "#{doc["duration_ms"].to_s.rjust(5)} мс"
end

client.close
