// Глава 3.2 · Индексы: одиночные, составные, уникальные — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//     cd lessons
//     g++ -std=c++17 ch3_2.cpp -o ch3_2 $(pkg-config --cflags --libs libmongocxx1) && ./ch3_2   # macOS, Homebrew
//     docker compose run --rm cpp lessons/ch3_2.cpp       # то же в контейнере
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

// plan — стадии плана, который выбрал сервер, сверху вниз: FETCH → IXSCAN service_1.
std::string plan(mongocxx::database& db, const std::string& coll, bsoncxx::document::view filtr,
                 bsoncxx::document::view sort = bsoncxx::document::view{}) {
    bsoncxx::builder::basic::document cmd;
    cmd.append(kvp("find", coll), kvp("filter", filtr));
    if (!sort.empty()) cmd.append(kvp("sort", sort));
    auto res = db.run_command(make_document(kvp("explain", cmd.view())));
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
    return steps;
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
    auto clients = sandbox["customers"];   // копия покупателей: уникальный индекс, §5–§7

    {
        // ── пример из главы ──────────────────────────────────────
        // Проверочные строки: замените их примером из главы.
        client["admin"].run_command(make_document(kvp("ping", 1)));
        std::cout << "Заготовка главы 3.2 подключилась к серверу. Замените проверочные строки примером из главы." << std::endl;
    }
}
