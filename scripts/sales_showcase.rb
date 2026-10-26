# Витрина продаж за квартал: конвейеры агрегации из модуля 4.
#
# Скрипт строит отчёт по заказам магазина за второй квартал 2026 года
# (апрель — июнь): статусы, выручка по месяцам и городам, категории и товары,
# чеки по суммам, самый дорогой заказ каждого города, оплата и доставка.
# Базы только читаются.
#
#     cd scripts
#     ruby sales_showcase.rb
#
#     docker compose run --rm ruby scripts/sales_showcase.rb    # в контейнере

require "mongo"

Mongo::Logger.logger.level = Logger::WARN

client = Mongo::Client.new(ENV.fetch("MONGO_URI", "mongodb://localhost:27017/"))
orders = client.use("shop").database[:orders]

QUARTER = { "created" => { "$gte" => Time.utc(2026, 4, 1), "$lt" => Time.utc(2026, 7, 1) } }.freeze
DELIVERED = { "$match" => QUARTER.merge("status" => "доставлен") }.freeze

def title(text)
  puts "\n" + text
  puts "─" * text.length
end

# ── 1. Заказы квартала ────────────────────────────────────────────────────
title("1. Заказы квартала")

orders.aggregate([{ "$match" => QUARTER }, { "$sortByCount" => "$status" }]).each do |doc|
  puts "  #{doc['_id'].ljust(12)} #{doc['count'].to_s.rjust(4)}"
end

total = orders.aggregate([
  DELIVERED,
  { "$group" => { "_id" => nil, "orders" => { "$sum" => 1 }, "revenue" => { "$sum" => "$total" },
                  "avg" => { "$avg" => "$total" } } },
  { "$project" => { "orders" => 1, "revenue" => 1, "avg" => { "$round" => ["$avg", 0] } } },
]).first
puts "  доставлено: #{total['orders']}, выручка #{total['revenue']}, средний чек #{total['avg'].to_i}"

# ── 2. Выручка по месяцам ─────────────────────────────────────────────────
title("2. Выручка по месяцам")

months = orders.aggregate([
  DELIVERED,
  { "$bucket" => { "groupBy" => "$created",
                   "boundaries" => [Time.utc(2026, 4, 1), Time.utc(2026, 5, 1), Time.utc(2026, 6, 1), Time.utc(2026, 7, 1)],
                   "output" => { "orders" => { "$sum" => 1 }, "revenue" => { "$sum" => "$total" } } } },
]).to_a
%w[апрель май июнь].zip(months).each do |name, doc|
  puts "  #{name.ljust(12)} #{doc['orders'].to_s.rjust(4)} #{doc['revenue'].to_s.rjust(10)}"
end

# ── 3. Города ─────────────────────────────────────────────────────────────
title("3. Города")

puts "  #{'город'.ljust(16)} #{'заказов'.rjust(7)} #{'выручка'.rjust(10)} #{'средний чек'.rjust(12)}"
orders.aggregate([
  DELIVERED,
  { "$group" => { "_id" => "$delivery.city", "orders" => { "$sum" => 1 },
                  "revenue" => { "$sum" => "$total" }, "avg" => { "$avg" => "$total" } } },
  { "$project" => { "orders" => 1, "revenue" => 1, "avg" => { "$round" => ["$avg", 0] } } },
  { "$sort" => { "revenue" => -1 } },
]).each do |doc|
  puts "  #{doc['_id'].ljust(16)} #{doc['orders'].to_s.rjust(7)} #{doc['revenue'].to_s.rjust(10)} #{doc['avg'].to_i.to_s.rjust(12)}"
end

# ── 4. Категории ──────────────────────────────────────────────────────────
title("4. Категории")

puts "  #{'категория'.ljust(16)} #{'штук'.rjust(5)} #{'выручка'.rjust(10)}"
orders.aggregate([
  DELIVERED,
  { "$unwind" => "$items" },
  { "$group" => { "_id" => "$items.category", "qty" => { "$sum" => "$items.qty" },
                  "revenue" => { "$sum" => { "$multiply" => ["$items.qty", "$items.price"] } } } },
  { "$sort" => { "revenue" => -1 } },
]).each do |doc|
  puts "  #{doc['_id'].ljust(16)} #{doc['qty'].to_s.rjust(5)} #{doc['revenue'].to_s.rjust(10)}"
end

# ── 5. Пять самых продаваемых товаров ─────────────────────────────────────
title("5. Пять самых продаваемых товаров")

orders.aggregate([
  DELIVERED,
  { "$unwind" => "$items" },
  { "$group" => { "_id" => "$items.title", "qty" => { "$sum" => "$items.qty" } } },
  { "$sort" => { "qty" => -1, "_id" => 1 } },
  { "$limit" => 5 },
]).each_with_index do |doc, i|
  puts "  #{i + 1}. #{doc['_id'].ljust(32)} #{doc['qty'].to_s.rjust(3)} шт."
end

# ── 6. Чеки по суммам ─────────────────────────────────────────────────────
title("6. Чеки по суммам")

LABELS = { 0 => "до 10 000", 10000 => "10 000 — 50 000", 50000 => "50 000 — 200 000", 200000 => "от 200 000" }.freeze
orders.aggregate([
  DELIVERED,
  { "$bucket" => { "groupBy" => "$total", "boundaries" => [0, 10000, 50000, 200000, 500000],
                   "output" => { "orders" => { "$sum" => 1 } } } },
]).each do |doc|
  puts "  #{LABELS[doc['_id']].ljust(18)} #{doc['orders'].to_s.rjust(4)}"
end

# ── 7. Самый дорогой заказ города ─────────────────────────────────────────
title("7. Самый дорогой заказ города")

orders.aggregate([
  DELIVERED,
  { "$sort" => { "total" => -1 } },
  { "$group" => { "_id" => "$delivery.city", "number" => { "$first" => "$number" }, "total" => { "$first" => "$total" } } },
  { "$sort" => { "total" => -1 } },
]).each do |doc|
  puts "  #{doc['_id'].ljust(16)} #{doc['number']}  #{doc['total'].to_s.rjust(8)}"
end

# ── 8. Оплата и доставка ──────────────────────────────────────────────────
title("8. Оплата и доставка")

facets = orders.aggregate([
  DELIVERED,
  { "$facet" => {
    "payment" => [{ "$sortByCount" => "$payment.method" }],
    "delivery" => [{ "$sortByCount" => "$delivery.type" }],
    "promo" => [{ "$match" => { "promo" => { "$exists" => true } } }, { "$count" => "orders" }],
  } },
]).first
puts "  оплата:   " + facets["payment"].map { |d| "#{d['_id']} #{d['count']}" }.join(", ")
puts "  доставка: " + facets["delivery"].map { |d| "#{d['_id']} #{d['count']}" }.join(", ")
puts "  с кодом скидки: #{facets['promo'].empty? ? 0 : facets['promo'][0]['orders']}"

client.close
