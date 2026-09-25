# Отчёт по магазину: что умеет MongoDB из модуля 1.
#
# Скрипт проходит все операции модуля 1 на базе shop и в песочнице sandbox
# и печатает результат так, как его показывают в консоли: счётчики, таблицы,
# карточку документа, страницы и итоги записи.
#
#     cd scripts
#     ruby shop_report.rb
#
#     docker compose run --rm ruby scripts/shop_report.rb    # в контейнере

require "mongo"

Mongo::Logger.logger.level = Logger::WARN

client = Mongo::Client.new(ENV.fetch("MONGO_URI", "mongodb://localhost:27017/"))
shop = client.use("shop").database
products = shop[:products]
orders = shop[:orders]
box = client.use("sandbox").database[:products]

def title(text)
  puts "\n" + text
  puts "─" * text.length
end

def pad(text, width)                 # ljust по видимым символам, как в других языках
  text.to_s.ljust(width)
end

# ── 1. Что лежит на сервере ───────────────────────────────────────────────
title("1. Связь и база shop")

client.use("admin").database.command(ping: 1)
puts "базы на сервере: " + client.database_names.sort.join(", ")
puts "коллекции shop: " + shop.collection_names.sort.join(", ")
puts "товаров #{products.count_documents({})}, " \
     "заказов #{orders.count_documents({})}, " \
     "покупателей #{shop[:customers].count_documents({})}"

# ── 2. Карточка одного документа ──────────────────────────────────────────
title("2. Один товар целиком")

tovar = products.find({ "_id" => "p-001" }).first
tovar.each { |field, value| puts "  #{pad(field, 10)} #{value}" }

# ── 3. Таблица: пять самых дорогих товаров ────────────────────────────────
title("3. Пять самых дорогих товаров")

puts "  #{pad('название', 32)} #{pad('категория', 14)} #{'цена'.rjust(8)}"
products.find({}, projection: { "_id" => 0, "title" => 1, "category" => 1, "price" => 1 })
        .sort({ "price" => -1 })
        .limit(5)
        .each do |doc|
  puts "  #{pad(doc['title'], 32)} #{pad(doc['category'], 14)} #{doc['price'].to_s.rjust(8)}"
end

# ── 4. Сортировка по двум полям ───────────────────────────────────────────
title("4. Каталог: категории по алфавиту, внутри — от дорогих к дешёвым")

products.find({}, projection: { "_id" => 0, "category" => 1, "title" => 1, "price" => 1 })
        .sort({ "category" => 1, "price" => -1 })
        .limit(8)
        .each do |doc|
  puts "  #{pad(doc['category'], 14)} #{pad(doc['title'], 32)} #{doc['price'].to_s.rjust(8)}"
end

# ── 5. Счётчики по категориям ─────────────────────────────────────────────
title("5. Товары по категориям")

products.distinct("category").sort.each do |category|
  puts "  #{pad(category, 14)} #{products.count_documents({ 'category' => category }).to_s.rjust(2)} шт"
end

# ── 6. Страницы заказов ───────────────────────────────────────────────────
title("6. Заказы страницами по 5")

[1, 2].each do |page|
  puts "  страница #{page}"
  orders.find({ "status" => "доставлен" }, projection: { "_id" => 0, "number" => 1, "total" => 1 })
        .sort({ "number" => 1 })
        .skip((page - 1) * 5)
        .limit(5)
        .each { |doc| puts "    #{doc['number']}  #{doc['total'].to_s.rjust(7)}" }
end

# ── 7. Вложенные поля и массивы в выводе ──────────────────────────────────
title("7. Заказ с доставкой и позициями")

zakaz = orders.find({ "_id" => "o-0001" }).first
puts "  номер #{zakaz['number']}, статус #{zakaz['status']}, сумма #{zakaz['total']}"
puts "  доставка: #{zakaz['delivery']['city']}, #{zakaz['delivery']['type']}, " \
     "#{zakaz['delivery']['days']} дн."
puts "  оплата: #{zakaz['payment']['method']}, проведена: #{zakaz['payment']['paid']}"
zakaz["items"].each do |position|
  puts "    #{position['qty']} × #{pad(position['title'], 34)} #{position['price'].to_s.rjust(7)}"
end

# ── 8. Запись: вставка, обновление, удаление ──────────────────────────────
title("8. Песочница: вставка, обновление, удаление")

["p-301", "p-302"].each { |key| box.delete_one({ "_id" => key }) }

added = box.insert_many([
  { "_id" => "p-301", "title" => "Док-станция USB-C", "category" => "аксессуары", "price" => 6490, "reviews" => 0 },
  { "_id" => "p-302", "title" => "Кабель HDMI 2 м", "category" => "аксессуары", "price" => 890, "reviews" => 0 },
])
puts "  вставлено: #{added.inserted_count} ключи: #{added.inserted_ids.join(', ')}"

changed = box.update_one({ "_id" => "p-301" }, { "$set" => { "price" => 5990 }, "$inc" => { "reviews" => 1 } })
puts "  обновление одного: найдено #{changed.matched_count}, изменено #{changed.modified_count}"

many = box.update_many({ "category" => "аксессуары" }, { "$set" => { "sale" => true } })
puts "  пометка распродажи: найдено #{many.matched_count}, изменено #{many.modified_count}"

after = box.find({ "_id" => "p-301" },
                 projection: { "_id" => 0, "title" => 1, "price" => 1, "reviews" => 1, "sale" => 1 }).first
puts "  что стало: #{after.inspect}"

gone = box.delete_one({ "_id" => "p-302" })
puts "  удалено: #{gone.deleted_count} · товаров в песочнице: #{box.count_documents({})}"

# ── 9. Итог ───────────────────────────────────────────────────────────────
title("9. Итог")

puts "  прочитано из shop: #{products.count_documents({})} товаров и " \
     "#{orders.count_documents({})} заказов"
puts "  изменено в песочнице: 1 вставка пачкой, 2 обновления, 1 удаление"
client.close
