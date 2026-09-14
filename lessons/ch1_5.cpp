// Глава 1.5 · Проекция: какие поля вернуть — заготовка для примеров.
//
// Глава только читает данные — сбрасывать базы не нужно.
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//     cd lessons
//     g++ -std=c++17 ch1_5.cpp -o ch1_5 $(pkg-config --cflags --libs libmongocxx1) && ./ch1_5   # macOS, Homebrew
//     docker compose run --rm cpp lessons/ch1_5.cpp       # то же в контейнере
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

    {
        // ── пример из главы ──────────────────────────────────────
        // Проверочные строки: замените их примером из главы.
        client["admin"].run_command(make_document(kvp("ping", 1)));
        std::cout << "Заготовка главы 1.5 подключилась к серверу. Замените проверочные строки примером из главы." << std::endl;
    }
}
