// Отчёт по магазину: что умеет MongoDB из модуля 1.
//
// Скрипт проходит все операции модуля 1 на базе shop и в песочнице sandbox
// и печатает результат так, как его показывают в консоли: счётчики, таблицы,
// карточку документа, страницы и итоги записи.
//
//     cd scripts
//     g++ -std=c++17 shop_report.cpp -o shop_report $(pkg-config --cflags --libs libmongocxx1) && ./shop_report
//
//     docker compose run --rm cpp scripts/shop_report.cpp    # в контейнере

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
#include <mongocxx/uri.hpp>

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

// Ширина столбца считается по символам: в кириллице на букву приходится
// два байта, и обычный setw сдвинул бы столбцы.
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

// Значение любого типа BSON в строку — для карточки документа.
std::string znachenie(const bsoncxx::document::element& field) {
    switch (field.type()) {
        case bsoncxx::type::k_string: return std::string(field.get_string().value);
        case bsoncxx::type::k_int32: return std::to_string(field.get_int32().value);
        case bsoncxx::type::k_int64: return std::to_string(field.get_int64().value);
        case bsoncxx::type::k_double: {
            std::ostringstream out;
            out << field.get_double().value;          // без лишних нулей: 4.2, а не 4.200000
            return out.str();
        }
        case bsoncxx::type::k_bool: return field.get_bool().value ? "true" : "false";
        case bsoncxx::type::k_date: {
            std::time_t sekundy = field.get_date().to_int64() / 1000;
            std::ostringstream out;
            out << std::put_time(std::gmtime(&sekundy), "%Y-%m-%d %H:%M:%S");
            return out.str();
        }
        case bsoncxx::type::k_document: return bsoncxx::to_json(field.get_document().value);
        case bsoncxx::type::k_array: return bsoncxx::to_json(field.get_array().value);
        default: {
            // to_json берёт документ или массив, поэтому значение заворачиваем
            std::string json = bsoncxx::to_json(make_document(kvp("v", field.get_value())).view());
            return json.substr(json.find(':') + 2, json.rfind('}') - json.find(':') - 3);
        }
    }
}

int main() {
    mongocxx::instance instance{};
    const char* env = std::getenv("MONGO_URI");
    mongocxx::client client{mongocxx::uri{env ? env : "mongodb://localhost:27017"}};

    auto shop = client["shop"];
    auto products = shop["products"];
    auto orders = shop["orders"];
    auto box = client["sandbox"]["products"];

    // ── 1. Что лежит на сервере ───────────────────────────────────────
    title("1. Связь и база shop");

    client["admin"].run_command(make_document(kvp("ping", 1)));
    auto bases = client.list_database_names();
    std::sort(bases.begin(), bases.end());
    std::cout << "базы на сервере: ";
    for (size_t i = 0; i < bases.size(); ++i) std::cout << (i ? ", " : "") << bases[i];
    std::cout << std::endl;

    auto names = shop.list_collection_names();
    std::sort(names.begin(), names.end());
    std::cout << "коллекции shop: ";
    for (size_t i = 0; i < names.size(); ++i) std::cout << (i ? ", " : "") << names[i];
    std::cout << std::endl;

    auto vsego_tovarov = products.count_documents(make_document());
    auto vsego_zakazov = orders.count_documents(make_document());
    std::cout << "товаров " << vsego_tovarov << ", заказов " << vsego_zakazov
              << ", покупателей " << shop["customers"].count_documents(make_document()) << std::endl;

    // ── 2. Карточка одного документа ──────────────────────────────────
    title("2. Один товар целиком");

    auto tovar = products.find_one(make_document(kvp("_id", "p-001")));
    for (auto&& field : tovar->view())
        std::cout << "  " << pad(std::string(field.key()), 10) << " " << znachenie(field) << std::endl;

    // ── 3. Таблица: пять самых дорогих товаров ────────────────────────
    title("3. Пять самых дорогих товаров");

    std::cout << "  " << pad("название", 32) << " " << pad("категория", 14)
              << " " << right("цена", 8) << std::endl;
    mongocxx::options::find options;
    options.projection(make_document(kvp("_id", 0), kvp("title", 1), kvp("category", 1), kvp("price", 1)));
    options.sort(make_document(kvp("price", -1)));
    options.limit(5);
    for (auto&& doc : products.find(make_document(), options)) {
        std::cout << "  " << pad(std::string(doc["title"].get_string().value), 32)
                  << " " << pad(std::string(doc["category"].get_string().value), 14)
                  << " " << right(std::to_string(doc["price"].get_int32().value), 8) << std::endl;
    }

    // ── 4. Сортировка по двум полям ───────────────────────────────────
    title("4. Каталог: категории по алфавиту, внутри — от дорогих к дешёвым");

    mongocxx::options::find katalog_options;
    katalog_options.projection(make_document(kvp("_id", 0), kvp("category", 1), kvp("title", 1), kvp("price", 1)));
    katalog_options.sort(make_document(kvp("category", 1), kvp("price", -1)));
    katalog_options.limit(8);
    for (auto&& doc : products.find(make_document(), katalog_options)) {
        std::cout << "  " << pad(std::string(doc["category"].get_string().value), 14)
                  << " " << pad(std::string(doc["title"].get_string().value), 32)
                  << " " << right(std::to_string(doc["price"].get_int32().value), 8) << std::endl;
    }

    // ── 5. Счётчики по категориям ─────────────────────────────────────
    title("5. Товары по категориям");

    std::vector<std::string> kategorii;
    for (auto&& doc : products.distinct("category", make_document()))
        for (auto&& value : doc["values"].get_array().value)
            kategorii.push_back(std::string(value.get_string().value));
    std::sort(kategorii.begin(), kategorii.end());
    for (const auto& category : kategorii) {
        auto skolko = products.count_documents(make_document(kvp("category", category)));
        std::cout << "  " << pad(category, 14) << " " << right(std::to_string(skolko), 2) << " шт" << std::endl;
    }

    // ── 6. Страницы заказов ───────────────────────────────────────────
    title("6. Заказы страницами по 5");

    for (int page = 1; page <= 2; ++page) {
        std::cout << "  страница " << page << std::endl;
        mongocxx::options::find page_options;
        page_options.projection(make_document(kvp("_id", 0), kvp("number", 1), kvp("total", 1)));
        page_options.sort(make_document(kvp("number", 1)));
        page_options.skip((page - 1) * 5);
        page_options.limit(5);
        for (auto&& doc : orders.find(make_document(kvp("status", "доставлен")), page_options)) {
            std::cout << "    " << std::string(doc["number"].get_string().value) << "  "
                      << right(std::to_string(doc["total"].get_int32().value), 7) << std::endl;
        }
    }

    // ── 7. Вложенные поля и массивы в выводе ──────────────────────────
    title("7. Заказ с доставкой и позициями");

    // Документ сохраняем в переменную: view указывает внутрь неё, и временное
    // значение из find_one исчезло бы сразу после строки.
    auto zakaz_doc = orders.find_one(make_document(kvp("_id", "o-0001")));
    auto zakaz = zakaz_doc->view();
    std::cout << "  номер " << std::string(zakaz["number"].get_string().value)
              << ", статус " << std::string(zakaz["status"].get_string().value)
              << ", сумма " << zakaz["total"].get_int32().value << std::endl;
    auto dostavka = zakaz["delivery"].get_document().value;
    std::cout << "  доставка: " << std::string(dostavka["city"].get_string().value)
              << ", " << std::string(dostavka["type"].get_string().value)
              << ", " << dostavka["days"].get_int32().value << " дн." << std::endl;
    auto oplata = zakaz["payment"].get_document().value;
    std::cout << "  оплата: " << std::string(oplata["method"].get_string().value)
              << ", проведена: " << (oplata["paid"].get_bool().value ? "true" : "false") << std::endl;
    for (auto&& position : zakaz["items"].get_array().value) {
        auto item = position.get_document().value;
        std::cout << "    " << item["qty"].get_int32().value << " × "
                  << pad(std::string(item["title"].get_string().value), 34) << " "
                  << right(std::to_string(item["price"].get_int32().value), 7) << std::endl;
    }

    // ── 8. Запись: вставка, обновление, удаление ──────────────────────
    title("8. Песочница: вставка, обновление, удаление");

    for (const std::string& key : {"p-301", "p-302"})
        box.delete_one(make_document(kvp("_id", key)));

    std::vector<bsoncxx::document::value> novye;
    novye.push_back(make_document(kvp("_id", "p-301"), kvp("title", "Док-станция USB-C"),
                                  kvp("category", "аксессуары"), kvp("price", 6490), kvp("reviews", 0)));
    novye.push_back(make_document(kvp("_id", "p-302"), kvp("title", "Кабель HDMI 2 м"),
                                  kvp("category", "аксессуары"), kvp("price", 890), kvp("reviews", 0)));
    auto added = box.insert_many(novye);
    std::cout << "  вставлено: " << added->inserted_count() << " ключи: ";
    bool first = true;
    for (auto&& pair : added->inserted_ids()) {
        std::cout << (first ? "" : ", ") << std::string(pair.second.get_string().value);
        first = false;
    }
    std::cout << std::endl;

    auto changed = box.update_one(make_document(kvp("_id", "p-301")),
                                  make_document(kvp("$set", make_document(kvp("price", 5990))),
                                                kvp("$inc", make_document(kvp("reviews", 1)))));
    std::cout << "  обновление одного: найдено " << changed->matched_count()
              << ", изменено " << changed->modified_count() << std::endl;

    auto many = box.update_many(make_document(kvp("category", "аксессуары")),
                                make_document(kvp("$set", make_document(kvp("sale", true)))));
    std::cout << "  пометка распродажи: найдено " << many->matched_count()
              << ", изменено " << many->modified_count() << std::endl;

    mongocxx::options::find after_options;
    after_options.projection(make_document(kvp("_id", 0), kvp("title", 1), kvp("price", 1),
                                           kvp("reviews", 1), kvp("sale", 1)));
    auto after = box.find_one(make_document(kvp("_id", "p-301")), after_options);
    std::cout << "  что стало: " << bsoncxx::to_json(after->view()) << std::endl;

    auto gone = box.delete_one(make_document(kvp("_id", "p-302")));
    std::cout << "  удалено: " << gone->deleted_count()
              << " · товаров в песочнице: " << box.count_documents(make_document()) << std::endl;

    // ── 9. Итог ───────────────────────────────────────────────────────
    title("9. Итог");

    std::cout << "  прочитано из shop: " << vsego_tovarov << " товаров и "
              << vsego_zakazov << " заказов" << std::endl;
    std::cout << "  изменено в песочнице: 1 вставка пачкой, 2 обновления, 1 удаление" << std::endl;
}
