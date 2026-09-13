# Проверка подключения: что лежит на сервере.
#
#     docker compose run --rm ruby hello/hello.rb    # из контейнера
#     ruby hello/hello.rb                            # с компьютера, если стоит gem mongo

require "mongo"

Mongo::Logger.logger.level = Logger::WARN

# В контейнере сервер называется mongo, на компьютере — localhost
SERVER = ENV.fetch("MONGO_URI", "mongodb://localhost:27017/")
client = Mongo::Client.new(SERVER, server_selection_timeout: 5)

puts "Адрес: #{SERVER}"
puts "Сервер: #{client.database.command(buildInfo: 1).first["version"]}"
puts "Базы данных: #{client.database_names}"

db = client.use("shop").database
puts "Коллекции shop: #{db.collection_names.sort}"
puts "Товаров: #{db[:products].count_documents({})}"
puts "Заказов: #{db[:orders].count_documents({})}"

client.close
