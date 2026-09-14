// Глава 1.7 · Обновление: $set, $inc, $unset — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//     cd lessons
//     g++ -std=c++17 ch1_7.cpp -o ch1_7 $(pkg-config --cflags --libs libmongocxx1) && ./ch1_7   # macOS, Homebrew
//     docker compose run --rm cpp lessons/ch1_7.cpp       # то же в контейнере
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

    // Товары, которые добавлялись в песочницу в главе 1.3. Если песочницу
    // сбрасывали, заготовка добавит их снова — обновлять будет что.
    if (box.count_documents(make_document(kvp("_id", "p-101"))) == 0) {
        std::vector<bsoncxx::document::value> added;
        added.push_back(make_document(kvp("_id", "p-101"), kvp("title", "Подставка для ноутбука"),
                                      kvp("category", "аксессуары"), kvp("price", 2490)));
        added.push_back(make_document(kvp("_id", "p-102"), kvp("title", "Чехол для планшета"),
                                      kvp("category", "аксессуары"), kvp("price", 1890)));
        added.push_back(make_document(kvp("sku", "SKU-AC-022"), kvp("title", "Коврик для мыши XL"),
                                      kvp("brand", "OEM"), kvp("category", "аксессуары"), kvp("price", 1290)));
        box.insert_many(added);
    }

    auto orders_box = client["sandbox"]["orders"];   // копия заказов: их можно менять
    if (orders_box.count_documents(make_document()) == 0) {
        std::vector<bsoncxx::document::value> all;
        for (auto&& doc : orders.find(make_document())) all.emplace_back(doc);
        orders_box.insert_many(all);
    }

    auto stats = client["sandbox"]["product_stats"];   // счётчики просмотров для §6
    stats.drop();                                      // каждый запуск заготовки считает с нуля
    auto now = std::time(nullptr);
    std::ostringstream day;
    day << std::put_time(std::gmtime(&now), "%Y-%m-%d");
    std::string today = day.str();

    {
        // ── пример из главы ──────────────────────────────────────
        // Проверочные строки: замените их примером из главы.
        client["admin"].run_command(make_document(kvp("ping", 1)));
        std::cout << "Заготовка главы 1.7 подключилась к серверу. Замените проверочные строки примером из главы." << std::endl;
    }
}
