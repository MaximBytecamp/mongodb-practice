// Глава 1.6 · Порядок и порции: sort, limit, skip — заготовка для примеров.
//
// Глава только читает данные — сбрасывать базы не нужно.
// Пример из главы вставьте внутрь фигурных скобок в конце main и запустите:
//
//     cd lessons
//     g++ -std=c++17 ch1_6.cpp -o ch1_6 $(pkg-config --cflags --libs libmongocxx1) && ./ch1_6   # macOS, Homebrew
//     docker compose run --rm cpp lessons/ch1_6.cpp       # то же в контейнере
//
// Если пример — целая функция, его место — над main, у отметки.
// Следующий пример вставляйте вместо предыдущего: каждый пример запускается
// один раз и по порядку — именно так получены выводы в справочнике.

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/insert.hpp>
#include <mongocxx/options/update.hpp>
#include <mongocxx/uri.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

// ── функции из примеров главы ─────────────────────────────────────


int main() {
    mongocxx::instance instance{};
    const char* env = std::getenv("MONGO_URI");
    mongocxx::client client{mongocxx::uri{env ? env : "mongodb://localhost:27017"}};

    auto db = client["shop"];
    auto products = db["products"];
    auto orders = db["orders"];
    auto box = client["sandbox"]["products"];   // песочница: здесь можно менять

    // Коллекция на 20 000 документов для замера в §6. Создаётся один раз.
    auto col = client["sandbox"]["bench"];
    if (col.count_documents(make_document()) == 0) {
        std::vector<bsoncxx::document::value> docs;
        for (int n = 0; n < 20000; ++n) docs.push_back(make_document(kvp("_id", n)));
        col.insert_many(docs);
    }
    int last_seen_id = 19989;   // последний _id предыдущей страницы

    {
        // ── пример из главы ──────────────────────────────────────

    }
}
