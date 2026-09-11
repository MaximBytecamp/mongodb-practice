// Практическая работа 01 — эталонное решение на C++ (mongocxx).
//
// Те же две задачи, что и в solution.py:
//   1. Витрина каталога: страница списка товаров, как её отдаёт бэкенд.
//   2. Приёмка поставки: обновить остатки, завести новинки, снять с продажи.
//
// Сборка и запуск:
//     g++ -std=c++17 solution.cpp -o solution \
//         $(pkg-config --cflags --libs libmongocxx1)
//     ./solution
//     MONGO_URI=mongodb://host:27017 ./solution
//
// Перед запуском данные должны быть залиты: bash ../stend/load.sh
// Драйвер: brew install mongo-cxx-driver

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/options/insert.hpp>
#include <mongocxx/uri.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

namespace {

const std::string LINE(62, '=');

// std::setw считает байты, а в кириллице их по два на символ, поэтому
// колонки разъезжаются. Дополняем строку сами — по числу символов UTF-8.
std::string pad(const std::string& text, std::size_t width) {
    std::size_t chars = 0;
    for (unsigned char symbol : text) {
        if ((symbol & 0xC0) != 0x80) ++chars;   // считаем начала последовательностей
    }
    return chars >= width ? text : text + std::string(width - chars, ' ');
}

// Товар витрины: только те поля, которые попадают на страницу.
struct Product {
    std::string title;
    std::int32_t price = 0;
    double rating = 0;
};

// ── Задача 1. Витрина каталога ───────────────────────────────────
// Возвращает страницу каталога и общее число товаров под фильтром:
// без второго числа на странице не нарисовать «2 из 5».
std::pair<std::vector<Product>, std::int64_t> catalog_page(
    mongocxx::collection& products, const std::string& category,
    std::int64_t page, std::int64_t per_page,
    const std::string& sort_field = "price", bool desc = true) {

    bsoncxx::builder::basic::document query{};
    if (!category.empty()) query.append(kvp("category", category));
    auto filter = query.extract();

    auto total = products.count_documents(filter.view());

    mongocxx::options::find options;
    options.projection(make_document(kvp("_id", 0), kvp("sku", 1), kvp("title", 1),
                                     kvp("price", 1), kvp("rating", 1)));
    // Второй ключ сортировки делает порядок однозначным: без него товары
    // с одинаковой ценой прыгают между страницами.
    options.sort(make_document(kvp(sort_field, desc ? -1 : 1), kvp("sku", 1)));
    options.skip((page - 1) * per_page);
    options.limit(per_page);

    std::vector<Product> items;
    for (const auto& doc : products.find(filter.view(), options)) {
        items.push_back(Product{
            std::string{doc["title"].get_string().value},
            doc["price"].get_int32().value,
            doc["rating"] ? doc["rating"].get_double().value : 0,
        });
    }
    return {items, total};
}

void print_items(const std::vector<Product>& items, bool with_rating) {
    for (const auto& item : items) {
        std::cout << "  " << std::setw(7) << item.price << " ₽  "
                  << (with_rating ? pad(item.title, 32) : item.title);
        if (with_rating) std::cout << " рейтинг " << item.rating;
        std::cout << std::endl;
    }
}

void show_catalog(mongocxx::collection& products) {
    std::cout << LINE << "\nЗАДАЧА 1 · витрина каталога\n" << LINE << std::endl;

    auto [items, total] = catalog_page(products, "ноутбуки", 1, 3);
    auto pages = (total + 2) / 3;
    std::cout << "\nНоутбуки, страница 1 из " << pages << " (всего " << total << "):" << std::endl;
    print_items(items, true);

    auto [second, _] = catalog_page(products, "ноутбуки", 2, 3);
    std::cout << "\nСтраница 2:" << std::endl;
    print_items(second, true);

    auto [top, all] = catalog_page(products, "", 1, 5);
    std::cout << "\nВесь каталог, топ-5 по цене (всего " << all << "):" << std::endl;
    print_items(top, false);

    // Пустая страница — обычное дело: запросили дальше, чем есть данных.
    auto [empty, same] = catalog_page(products, "ноутбуки", 9, 3);
    std::cout << "\nСтраница 9 из существующих " << pages << ": найдено "
              << empty.size() << " товаров, total по-прежнему " << same << std::endl;
}

// ── Задача 2. Приёмка поставки ───────────────────────────────────
void receive_supply(mongocxx::collection& box) {
    std::cout << "\n" << LINE << "\nЗАДАЧА 2 · приёмка поставки\n" << LINE << std::endl;

    std::vector<std::pair<std::string, std::int64_t>> report;

    // 1. Остатки: $inc прибавляет на сервере, не читая значение на клиент.
    //    Если поля qty_total нет — оно появится с этим значением.
    const std::vector<std::pair<std::string, std::int32_t>> arrived{
        {"SKU-NB-001", 5}, {"SKU-PH-006", 12}, {"SKU-PR-010", 3}};

    std::int64_t updated = 0;
    for (const auto& [sku, qty] : arrived) {
        auto res = box.update_one(make_document(kvp("sku", sku)),
                                  make_document(kvp("$inc", make_document(kvp("qty_total", qty)))));
        if (res->matched_count() == 0) {
            std::cout << "  ВНИМАНИЕ: артикул " << sku << " не найден в каталоге" << std::endl;
        }
        updated += res->modified_count();
    }
    report.emplace_back("остатки обновлены", updated);

    // 2. Новинки: свой _id делает повторный запуск безопасным —
    //    вторая попытка упрётся в E11000, а не создаст дубли.
    std::vector<bsoncxx::document::value> new_items;
    new_items.push_back(make_document(
        kvp("_id", "p-101"), kvp("sku", "SKU-AC-101"), kvp("title", "Подставка для ноутбука"),
        kvp("brand", "OEM"), kvp("category", "аксессуары"), kvp("price", 2490), kvp("reviews", 0)));
    new_items.push_back(make_document(
        kvp("_id", "p-102"), kvp("sku", "SKU-AC-102"), kvp("title", "Чехол для планшета"),
        kvp("brand", "OEM"), kvp("category", "аксессуары"), kvp("price", 1890), kvp("reviews", 0)));

    mongocxx::options::insert insert_options;
    insert_options.ordered(false);   // дубликат не останавливает приёмку остальных

    try {
        auto many = box.insert_many(new_items, insert_options);
        report.emplace_back("новых позиций", many->inserted_count());
    } catch (const mongocxx::bulk_write_exception& error) {
        auto already = static_cast<std::int64_t>(new_items.size());
        if (error.raw_server_error()) {
            auto view = error.raw_server_error()->view();
            if (view["writeErrors"]) {
                already = std::distance(view["writeErrors"].get_array().value.begin(),
                                        view["writeErrors"].get_array().value.end());
            }
        }
        std::cout << "  " << already << " позиций уже были в каталоге — пропущены" << std::endl;
        report.emplace_back("новых позиций", static_cast<std::int64_t>(new_items.size()) - already);
    }

    // 3. Снятие с продажи: сначала считаем, потом удаляем.
    auto discontinued = make_document(
        kvp("sku", make_document(kvp("$in", make_array("SKU-CP-018")))));

    std::cout << "\n  под снятие с продажи попало: "
              << box.count_documents(discontinued.view()) << std::endl;

    mongocxx::options::find preview;
    preview.projection(make_document(kvp("_id", 1), kvp("title", 1), kvp("price", 1)));
    for (const auto& doc : box.find(discontinued.view(), preview)) {
        std::cout << "    " << bsoncxx::to_json(doc) << std::endl;
    }
    report.emplace_back("снято с продажи",
                        box.delete_many(discontinued.view())->deleted_count());

    // 4. Бесплатная доставка для всей категории — одним запросом.
    auto accessories = make_document(kvp("category", "аксессуары"));
    std::cout << "\n  бесплатная доставка коснётся: "
              << box.count_documents(accessories.view()) << " товаров" << std::endl;

    auto shipped = box.update_many(
        accessories.view(),
        make_document(kvp("$set", make_document(kvp("free_shipping", true)))));
    report.emplace_back("бесплатная доставка", shipped->modified_count());

    report.emplace_back("итого в каталоге", box.count_documents(make_document()));

    std::cout << "\nОтчёт о приёмке:" << std::endl;
    for (const auto& [name, value] : report) {
        std::cout << "  " << pad(name, 24) << value << std::endl;
    }
}

}  // namespace

int main() {
    const char* env = std::getenv("MONGO_URI");
    const std::string uri = env ? env : "mongodb://localhost:27017";

    mongocxx::instance instance{};          // ровно один на программу
    mongocxx::client client{mongocxx::uri{uri}};

    auto products = client["shop"]["products"];   // только читаем
    auto box = client["sandbox"]["products"];     // здесь меняем

    show_catalog(products);
    receive_supply(box);
    return 0;
}
