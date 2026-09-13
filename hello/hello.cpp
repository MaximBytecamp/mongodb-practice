// Проверка подключения: что лежит на сервере.
//
//     docker compose run --rm cpp hello/hello.cpp    // из контейнера: соберёт и запустит

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

int main() {
    mongocxx::instance instance{};

    // В контейнере сервер называется mongo, на компьютере — localhost
    const char* env = std::getenv("MONGO_URI");
    const std::string uri = env ? env : "mongodb://localhost:27017";
    mongocxx::client client{mongocxx::uri{uri}};

    auto info = client["admin"].run_command(make_document(kvp("buildInfo", 1)));
    std::cout << "Адрес: " << uri << "\n";
    std::cout << "Сервер: " << info.view()["version"].get_string().value << "\n";

    std::cout << "Базы данных:";
    for (const auto& name : client.list_database_names()) std::cout << " " << name;
    std::cout << "\n";

    auto db = client["shop"];
    auto collections = db.list_collection_names();
    std::sort(collections.begin(), collections.end());
    std::cout << "Коллекции shop:";
    for (const auto& name : collections) std::cout << " " << name;
    std::cout << "\n";

    std::cout << "Товаров: " << db["products"].count_documents(make_document()) << "\n";
    std::cout << "Заказов: " << db["orders"].count_documents(make_document()) << "\n";
}
