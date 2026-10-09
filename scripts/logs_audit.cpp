// Аудит журнала сервиса: что умеют правила, индексы и explain из модуля 3.
//
// Скрипт работает с копией журнала logs.events в песочнице: проверяет события
// правилом $jsonSchema и включает его как валидатор, снимает планы трёх
// запросов дежурного, строит под них индексы по правилу ESR и сравнивает
// планы до и после. База logs остаётся без изменений.
//
//     cd scripts
//     g++ -std=c++17 logs_audit.cpp -o logs_audit $(pkg-config --cflags --libs libmongocxx1) && ./logs_audit
//
//     docker compose run --rm cpp scripts/logs_audit.cpp    # в контейнере

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/exception/operation_exception.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/find.hpp>
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

// Число из ответа сервера: счётчики приходят как int32 или int64.
std::int64_t number(bsoncxx::document::element value) {
    return value.type() == bsoncxx::type::k_int64 ? value.get_int64().value : value.get_int32().value;
}

// План и счётчики explain: стадии сверху вниз, вернул, ключей, документов.
std::tuple<std::string, std::int64_t, std::int64_t, std::int64_t> summary(mongocxx::database& sandbox,
                                                                          bsoncxx::document::view find) {
    bsoncxx::builder::basic::document cmd;
    cmd.append(kvp("find", "audit"));
    for (auto&& field : find) cmd.append(kvp(field.key(), field.get_value()));
    auto info = sandbox.run_command(make_document(kvp("explain", cmd.view()), kvp("verbosity", "executionStats")));
    auto stage = info.view()["queryPlanner"]["winningPlan"].get_document().value;
    if (stage["queryPlan"]) stage = stage["queryPlan"].get_document().value;
    std::string steps;
    while (true) {
        steps += std::string{stage["stage"].get_string().value};
        if (stage["indexName"]) steps += " " + std::string{stage["indexName"].get_string().value};
        if (!stage["inputStage"]) break;
        steps += " → ";
        stage = stage["inputStage"].get_document().value;
    }
    auto stats = info.view()["executionStats"].get_document().value;
    return {steps, number(stats["nReturned"]), number(stats["totalKeysExamined"]), number(stats["totalDocsExamined"])};
}

// Запросы дежурного: название и части команды find.
std::vector<std::pair<std::string, bsoncxx::document::value>> queries() {
    std::vector<std::pair<std::string, bsoncxx::document::value>> out;
    out.emplace_back("ошибки платежей", make_document(
        kvp("filter", make_document(kvp("service", "payments"), kvp("status", make_document(kvp("$gte", 500))))),
        kvp("sort", make_document(kvp("ts", -1))),
        kvp("limit", 5)));
    out.emplace_back("медленный поиск", make_document(
        kvp("filter", make_document(kvp("service", "search"), kvp("duration_ms", make_document(kvp("$gte", 1000))))),
        kvp("sort", make_document(kvp("duration_ms", -1))),
        kvp("limit", 5)));
    out.emplace_back("события покупателя", make_document(
        kvp("filter", make_document(kvp("user_id", "c-015"))),
        kvp("sort", make_document(kvp("ts", -1)))));
    return out;
}

void report(mongocxx::database& sandbox) {
    std::cout << "  " << pad("запрос", 20) << " " << right("вернул", 6) << " " << right("ключей", 7) << " "
              << right("документов", 11) << std::endl;
    for (const auto& [name, find] : queries()) {
        auto [plan, returned, keys, docs] = summary(sandbox, find.view());
        std::cout << "  " << pad(name, 20) << " " << std::setw(6) << returned << " " << std::setw(7) << keys << " "
                  << std::setw(11) << docs << std::endl;
        std::cout << "      " << plan << std::endl;
    }
}

int main() {
    mongocxx::instance instance{};
    const char* env = std::getenv("MONGO_URI");
    mongocxx::client client{mongocxx::uri{env ? env : "mongodb://localhost:27017"}};

    auto events = client["logs"]["events"];
    auto sandbox = client["sandbox"];
    auto audit = sandbox["audit"];

    // ── 1. Копия журнала ──────────────────────────────────────────────────
    title("1. Копия журнала в песочнице");

    audit.drop();
    std::vector<bsoncxx::document::value> all;
    for (auto&& doc : events.find(make_document())) all.emplace_back(doc);
    audit.insert_many(all);
    std::cout << "  событий в копии: " << audit.count_documents(make_document()) << std::endl;
    int indexes = 0;
    for (auto&& ix : audit.list_indexes()) {
        (void)ix;
        ++indexes;
    }
    std::cout << "  индексов: " << indexes << std::endl;

    // ── 2. Правило для событий ────────────────────────────────────────────
    title("2. Правило для событий");

    auto rule = make_document(
        kvp("bsonType", "object"),
        kvp("required", make_array("ts", "service", "level", "route", "status", "duration_ms")),
        kvp("properties", make_document(
            kvp("ts", make_document(kvp("bsonType", "date"))),
            kvp("service", make_document(kvp("enum", make_array("api-gateway", "notifier", "orders", "payments", "search")))),
            kvp("level", make_document(kvp("enum", make_array("info", "warn", "error")))),
            kvp("route", make_document(kvp("bsonType", "string"))),
            kvp("status", make_document(kvp("bsonType", "int"), kvp("minimum", 100), kvp("maximum", 599))),
            kvp("duration_ms", make_document(kvp("bsonType", "number"), kvp("minimum", 0))))));
    std::cout << "  соответствуют правилу:    "
              << audit.count_documents(make_document(kvp("$jsonSchema", rule.view()))) << std::endl;
    std::cout << "  не соответствуют правилу: "
              << audit.count_documents(make_document(kvp("$nor", make_array(make_document(kvp("$jsonSchema", rule.view()))))))
              << std::endl;

    sandbox.run_command(make_document(
        kvp("collMod", "audit"),
        kvp("validator", make_document(kvp("$jsonSchema", rule.view()))),
        kvp("validationLevel", "strict"),
        kvp("validationAction", "error")));
    std::cout << "  правило включено: strict, error" << std::endl;

    // ── 3. Проверка правила на записи ─────────────────────────────────────
    title("3. Неверные события");

    auto ts = bsoncxx::types::b_date{std::chrono::milliseconds{1788373800000}};   // 02.09.2026 18:30 UTC
    std::vector<std::pair<std::string, bsoncxx::document::value>> wrong;
    wrong.emplace_back("статус 700", make_document(kvp("ts", ts), kvp("service", "payments"), kvp("level", "error"),
                                                   kvp("route", "/api/pay"), kvp("status", 700), kvp("duration_ms", 300)));
    wrong.emplace_back("уровень fatal", make_document(kvp("ts", ts), kvp("service", "payments"), kvp("level", "fatal"),
                                                      kvp("route", "/api/pay"), kvp("status", 500), kvp("duration_ms", 300)));
    wrong.emplace_back("без времени", make_document(kvp("service", "payments"), kvp("level", "error"),
                                                    kvp("route", "/api/pay"), kvp("status", 500), kvp("duration_ms", 300)));
    for (const auto& [label, doc] : wrong) {
        try {
            audit.insert_one(doc.view());
            std::cout << "  " << pad(label, 14) << " записано" << std::endl;
        } catch (const mongocxx::operation_exception& e) {
            std::string reasons;
            auto add = [&](const std::string& text) { reasons += (reasons.empty() ? "" : ", ") + text; };
            auto info = e.raw_server_error()->view()["writeErrors"][0]["errInfo"];
            for (auto&& item : info["details"]["schemaRulesNotSatisfied"].get_array().value) {
                auto r = item.get_document().value;
                std::string op{r["operatorName"].get_string().value};
                if (op == "required") {
                    for (auto&& name : r["missingProperties"].get_array().value)
                        add("нет " + std::string{name.get_string().value});
                }
                if (op == "properties") {
                    for (auto&& prop : r["propertiesNotSatisfied"].get_array().value)
                        for (auto&& d : prop["details"].get_array().value)
                            add(std::string{prop["propertyName"].get_string().value} + ": " +
                                std::string{d["operatorName"].get_string().value});
                }
            }
            std::cout << "  " << pad(label, 14) << " отклонено, код " << e.code().value() << ": " << reasons << std::endl;
        }
    }
    std::cout << "  событий в копии: " << audit.count_documents(make_document()) << std::endl;

    // ── 4. Запросы дежурного без индексов ─────────────────────────────────
    title("4. Запросы дежурного без индексов");
    report(sandbox);

    // ── 5. Индексы по правилу ESR ─────────────────────────────────────────
    title("5. Индексы: равенство, сортировка, диапазон");

    std::vector<bsoncxx::document::value> keys;
    keys.push_back(make_document(kvp("service", 1), kvp("ts", -1), kvp("status", 1)));
    keys.push_back(make_document(kvp("service", 1), kvp("duration_ms", -1)));
    keys.push_back(make_document(kvp("user_id", 1), kvp("ts", -1)));
    for (const auto& key : keys) {
        auto created = audit.create_index(key.view());
        std::cout << "  создан " << created.view()["name"].get_string().value << std::endl;
    }

    // ── 6. Те же запросы с индексами ──────────────────────────────────────
    title("6. Запросы дежурного с индексами");
    report(sandbox);

    // ── 7. Пять последних ошибок платежей ─────────────────────────────────
    title("7. Пять последних ошибок платежей");

    mongocxx::options::find newest;
    newest.sort(make_document(kvp("ts", -1)));
    newest.limit(5);
    auto errors = make_document(kvp("service", "payments"), kvp("status", make_document(kvp("$gte", 500))));
    for (auto&& doc : audit.find(errors.view(), newest)) {
        std::time_t seconds = doc["ts"].get_date().to_int64() / 1000;
        std::ostringstream when;
        when << std::put_time(std::gmtime(&seconds), "%d.%m %H:%M:%S");
        std::cout << "  " << when.str() << "  " << pad(std::string{doc["route"].get_string().value}, 18) << " "
                  << number(doc["status"]) << "  " << std::setw(5) << number(doc["duration_ms"]) << " мс" << std::endl;
    }
}
