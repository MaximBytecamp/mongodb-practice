// Глава 3.3 · Как читать explain: COLLSCAN против IXSCAN — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//     cd lessons
//     g++ -std=c++17 ch3_3.cpp -o ch3_3 $(pkg-config --cflags --libs libmongocxx1) && ./ch3_3   # macOS, Homebrew
//     docker compose run --rm cpp lessons/ch3_3.cpp       # то же в контейнере
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

// summary — план и счётчики explain одной строкой: стадии | вернул | ключей | документов.
std::string summary(mongocxx::database& db, const std::string& coll, bsoncxx::document::view find) {
    bsoncxx::builder::basic::document cmd;
    cmd.append(kvp("find", coll));
    for (auto&& field : find) cmd.append(kvp(field.key(), field.get_value()));
    auto res = db.run_command(make_document(kvp("explain", cmd.view()), kvp("verbosity", "executionStats")));
    auto stage = res.view()["queryPlanner"]["winningPlan"].get_document().value;
    if (stage["queryPlan"]) stage = stage["queryPlan"].get_document().value;   // MongoDB 7 кладёт план во вложенное поле
    std::string steps;
    while (true) {
        steps += std::string{stage["stage"].get_string().value};
        if (stage["indexName"]) steps += " " + std::string{stage["indexName"].get_string().value};
        if (!stage["inputStage"]) break;
        steps += " → ";
        stage = stage["inputStage"].get_document().value;
    }
    auto stats = res.view()["executionStats"].get_document().value;
    auto number = [&](const char* name) {
        auto value = stats[name];
        return std::to_string(value.type() == bsoncxx::type::k_int64 ? value.get_int64().value : value.get_int32().value);
    };
    return steps + " | вернул " + number("nReturned") + " | ключей " + number("totalKeysExamined")
         + " | документов " + number("totalDocsExamined");
}


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

    auto journal = sandbox["events"];      // копия журнала: на ней строятся индексы главы

    {
        // ── пример из главы ──────────────────────────────────────
        // Проверочные строки: замените их примером из главы.
        client["admin"].run_command(make_document(kvp("ping", 1)));
        std::cout << "Заготовка главы 3.3 подключилась к серверу. Замените проверочные строки примером из главы." << std::endl;
    }
}
