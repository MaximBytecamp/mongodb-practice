// Глава 3.1 · Правила на уровне базы: $jsonSchema — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//     cd lessons
//     g++ -std=c++17 ch3_1.cpp -o ch3_1 $(pkg-config --cflags --libs libmongocxx1) && ./ch3_1   # macOS, Homebrew
//     docker compose run --rm cpp lessons/ch3_1.cpp       # то же в контейнере
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
    auto events = client["logs"]["events"];
    auto sandbox = client["sandbox"];
    auto box = sandbox["products"];   // песочница: здесь можно менять

    auto catalog = sandbox["catalog"];   // коллекция без правила, §1

    // Правило товара (§2 главы): обязательные поля и типы значений.
    auto schema = make_document(
        kvp("bsonType", "object"),
        kvp("required", make_array("sku", "title", "category", "price")),
        kvp("properties", make_document(
            kvp("sku", make_document(kvp("bsonType", "string"))),
            kvp("title", make_document(kvp("bsonType", "string"))),
            kvp("category", make_document(kvp("bsonType", "string"))),
            kvp("price", make_document(kvp("bsonType", "number"), kvp("minimum", 1))))));

    {
        // ── пример из главы ──────────────────────────────────────
        // Проверочные строки: замените их примером из главы.
        client["admin"].run_command(make_document(kvp("ping", 1)));
        std::cout << "Заготовка главы 3.1 подключилась к серверу. Замените проверочные строки примером из главы." << std::endl;
    }
}
