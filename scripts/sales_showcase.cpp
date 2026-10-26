// Витрина продаж за квартал: конвейеры агрегации из модуля 4.
//
// Скрипт строит отчёт по заказам магазина за второй квартал 2026 года
// (апрель — июнь): статусы, выручка по месяцам и городам, категории и товары,
// чеки по суммам, самый дорогой заказ каждого города, оплата и доставка.
// Базы только читаются.
//
//     cd scripts
//     g++ -std=c++17 sales_showcase.cpp -o sales_showcase $(pkg-config --cflags --libs libmongocxx1) && ./sales_showcase
//
//     docker compose run --rm cpp scripts/sales_showcase.cpp    # в контейнере

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pipeline.hpp>
#include <mongocxx/uri.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

int simvolov(const std::string& text) {
    int n = 0;
    for (unsigned char c : text)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

std::string pad(const std::string& text, int width) {
    int add = width - simvolov(text);
    return add > 0 ? text + std::string(add, ' ') : text;
}

std::string right(const std::string& text, int width) {
    int add = width - simvolov(text);
    return add > 0 ? std::string(add, ' ') + text : text;
}

void title(const std::string& text) {
    std::cout << "\n" << text << "\n";
    for (int i = 0; i < simvolov(text); ++i) std::cout << "─";
    std::cout << std::endl;
}

// Число из ответа сервера: суммы приходят как int32 или int64, среднее после $round — как double.
std::int64_t number(bsoncxx::document::element value) {
    if (value.type() == bsoncxx::type::k_int32) return value.get_int32().value;
    if (value.type() == bsoncxx::type::k_double) return static_cast<std::int64_t>(value.get_double().value);
    return value.get_int64().value;
}

std::string text(bsoncxx::document::element value) {
    return std::string{value.get_string().value};
}

// Начало месяца 2026 года в UTC: миллисекунды от 01.01.1970.
bsoncxx::types::b_date month_start(std::int64_t ms) {
    return bsoncxx::types::b_date{std::chrono::milliseconds{ms}};
}

int main() {
    mongocxx::instance instance{};
    const char* env = std::getenv("MONGO_URI");
    mongocxx::client client{mongocxx::uri{env ? env : "mongodb://localhost:27017"}};
    auto orders = client["shop"]["orders"];

    auto april = month_start(1775001600000), may = month_start(1777593600000),
         june = month_start(1780272000000), july = month_start(1782864000000);
    auto quarter = make_document(kvp("created", make_document(kvp("$gte", april), kvp("$lt", july))));
    auto delivered = make_document(kvp("created", make_document(kvp("$gte", april), kvp("$lt", july))),
                                   kvp("status", "доставлен"));

    // ── 1. Заказы квартала ────────────────────────────────────────────────
    title("1. Заказы квартала");

    mongocxx::pipeline statuses;
    statuses.match(quarter.view());
    statuses.sort_by_count("$status");
    for (auto&& doc : orders.aggregate(statuses)) {
        std::cout << "  " << pad(text(doc["_id"]), 12) << " " << std::setw(4) << number(doc["count"]) << std::endl;
    }

    mongocxx::pipeline totals;
    totals.match(delivered.view());
    totals.group(make_document(
        kvp("_id", bsoncxx::types::b_null{}),
        kvp("orders", make_document(kvp("$sum", 1))),
        kvp("revenue", make_document(kvp("$sum", "$total"))),
        kvp("avg", make_document(kvp("$avg", "$total")))));
    totals.project(make_document(kvp("orders", 1), kvp("revenue", 1),
                                 kvp("avg", make_document(kvp("$round", make_array("$avg", 0))))));
    for (auto&& doc : orders.aggregate(totals)) {
        std::cout << "  доставлено: " << number(doc["orders"]) << ", выручка " << number(doc["revenue"])
                  << ", средний чек " << number(doc["avg"]) << std::endl;
    }

    // ── 2. Выручка по месяцам ─────────────────────────────────────────────
    title("2. Выручка по месяцам");

    mongocxx::pipeline months;
    months.match(delivered.view());
    months.bucket(make_document(
        kvp("groupBy", "$created"),
        kvp("boundaries", make_array(april, may, june, july)),
        kvp("output", make_document(kvp("orders", make_document(kvp("$sum", 1))),
                                    kvp("revenue", make_document(kvp("$sum", "$total")))))));
    std::vector<std::string> names{"апрель", "май", "июнь"};
    std::size_t i = 0;
    for (auto&& doc : orders.aggregate(months)) {
        std::cout << "  " << pad(names[i++], 12) << " " << std::setw(4) << number(doc["orders"]) << " "
                  << std::setw(10) << number(doc["revenue"]) << std::endl;
    }

    // ── 3. Города ─────────────────────────────────────────────────────────
    title("3. Города");

    std::cout << "  " << pad("город", 16) << " " << right("заказов", 7) << " " << right("выручка", 10) << " "
              << right("средний чек", 12) << std::endl;
    mongocxx::pipeline cities;
    cities.match(delivered.view());
    cities.group(make_document(
        kvp("_id", "$delivery.city"),
        kvp("orders", make_document(kvp("$sum", 1))),
        kvp("revenue", make_document(kvp("$sum", "$total"))),
        kvp("avg", make_document(kvp("$avg", "$total")))));
    cities.project(make_document(kvp("orders", 1), kvp("revenue", 1),
                                 kvp("avg", make_document(kvp("$round", make_array("$avg", 0))))));
    cities.sort(make_document(kvp("revenue", -1)));
    for (auto&& doc : orders.aggregate(cities)) {
        std::cout << "  " << pad(text(doc["_id"]), 16) << " " << std::setw(7) << number(doc["orders"]) << " "
                  << std::setw(10) << number(doc["revenue"]) << " " << std::setw(12) << number(doc["avg"]) << std::endl;
    }

    // ── 4. Категории ──────────────────────────────────────────────────────
    title("4. Категории");

    std::cout << "  " << pad("категория", 16) << " " << right("штук", 5) << " " << right("выручка", 10) << std::endl;
    mongocxx::pipeline categories;
    categories.match(delivered.view());
    categories.unwind("$items");
    categories.group(make_document(
        kvp("_id", "$items.category"),
        kvp("qty", make_document(kvp("$sum", "$items.qty"))),
        kvp("revenue", make_document(kvp("$sum", make_document(kvp("$multiply", make_array("$items.qty", "$items.price"))))))));
    categories.sort(make_document(kvp("revenue", -1)));
    for (auto&& doc : orders.aggregate(categories)) {
        std::cout << "  " << pad(text(doc["_id"]), 16) << " " << std::setw(5) << number(doc["qty"]) << " "
                  << std::setw(10) << number(doc["revenue"]) << std::endl;
    }

    // ── 5. Пять самых продаваемых товаров ─────────────────────────────────
    title("5. Пять самых продаваемых товаров");

    mongocxx::pipeline top;
    top.match(delivered.view());
    top.unwind("$items");
    top.group(make_document(kvp("_id", "$items.title"), kvp("qty", make_document(kvp("$sum", "$items.qty")))));
    top.sort(make_document(kvp("qty", -1), kvp("_id", 1)));
    top.limit(5);
    int place = 0;
    for (auto&& doc : orders.aggregate(top)) {
        std::cout << "  " << ++place << ". " << pad(text(doc["_id"]), 32) << " " << std::setw(3) << number(doc["qty"])
                  << " шт." << std::endl;
    }

    // ── 6. Чеки по суммам ─────────────────────────────────────────────────
    title("6. Чеки по суммам");

    std::map<std::int64_t, std::string> labels{
        {0, "до 10 000"}, {10000, "10 000 — 50 000"}, {50000, "50 000 — 200 000"}, {200000, "от 200 000"}};
    mongocxx::pipeline checks;
    checks.match(delivered.view());
    checks.bucket(make_document(
        kvp("groupBy", "$total"),
        kvp("boundaries", make_array(0, 10000, 50000, 200000, 500000)),
        kvp("output", make_document(kvp("orders", make_document(kvp("$sum", 1)))))));
    for (auto&& doc : orders.aggregate(checks)) {
        std::cout << "  " << pad(labels[number(doc["_id"])], 18) << " " << std::setw(4) << number(doc["orders"])
                  << std::endl;
    }

    // ── 7. Самый дорогой заказ города ─────────────────────────────────────
    title("7. Самый дорогой заказ города");

    mongocxx::pipeline best;
    best.match(delivered.view());
    best.sort(make_document(kvp("total", -1)));
    best.group(make_document(
        kvp("_id", "$delivery.city"),
        kvp("number", make_document(kvp("$first", "$number"))),
        kvp("total", make_document(kvp("$first", "$total")))));
    best.sort(make_document(kvp("total", -1)));
    for (auto&& doc : orders.aggregate(best)) {
        std::cout << "  " << pad(text(doc["_id"]), 16) << " " << text(doc["number"]) << "  " << std::setw(8)
                  << number(doc["total"]) << std::endl;
    }

    // ── 8. Оплата и доставка ──────────────────────────────────────────────
    title("8. Оплата и доставка");

    mongocxx::pipeline summary;
    summary.match(delivered.view());
    summary.facet(make_document(
        kvp("payment", make_array(make_document(kvp("$sortByCount", "$payment.method")))),
        kvp("delivery", make_array(make_document(kvp("$sortByCount", "$delivery.type")))),
        kvp("promo", make_array(make_document(kvp("$match", make_document(kvp("promo", make_document(kvp("$exists", true)))))),
                                make_document(kvp("$count", "orders"))))));
    for (auto&& doc : orders.aggregate(summary)) {
        auto line = [&](const char* field) {
            std::string out;
            for (auto&& group : doc[field].get_array().value) {
                auto g = group.get_document().value;
                if (!out.empty()) out += ", ";
                out += text(g["_id"]) + " " + std::to_string(number(g["count"]));
            }
            return out;
        };
        std::cout << "  оплата:   " << line("payment") << std::endl;
        std::cout << "  доставка: " << line("delivery") << std::endl;
        auto promo = doc["promo"].get_array().value;
        std::int64_t with_promo = promo.begin() == promo.end() ? 0 : number(promo.begin()->get_document().value["orders"]);
        std::cout << "  с кодом скидки: " << with_promo << std::endl;
    }
}
