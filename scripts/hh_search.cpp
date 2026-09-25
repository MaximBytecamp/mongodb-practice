// Подбор кандидатов: что умеет язык фильтров из модуля 2.
//
// Скрипт решает прикладную задачу на базе hh: подобрать кандидатов под
// требования вакансии, посмотреть, сколько резюме проходит каждое требование
// по отдельности, найти подходящие вакансии и проверить данные на пропуски,
// неверные типы и несогласованные поля.
//
//     cd scripts
//     g++ -std=c++17 hh_search.cpp -o hh_search $(pkg-config --cflags --libs libmongocxx1) && ./hh_search
//
//     docker compose run --rm cpp scripts/hh_search.cpp    # в контейнере

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
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

// Строки массива через запятую — для навыков в таблице.
std::string cherez_zapyatuyu(const bsoncxx::array::view& massiv) {
    std::string out;
    for (auto&& element : massiv) {
        if (!out.empty()) out += ", ";
        out += std::string(element.get_string().value);
    }
    return out;
}

int main() {
    mongocxx::instance instance{};
    const char* env = std::getenv("MONGO_URI");
    mongocxx::client client{mongocxx::uri{env ? env : "mongodb://localhost:27017"}};

    auto hh = client["hh"];
    auto resumes = hh["resumes"];
    auto vacancies = hh["vacancies"];
    auto interviews = hh["interviews"];

    // Требования вакансии.
    const std::vector<std::string> navyki = {"Python", "SQL"};
    const std::string gorod = "Ярославль";
    const int potolok = 100000;
    const int opyt_mesyacev = 6;

    // ── 1. Требования ─────────────────────────────────────────────────
    title("1. Требования");

    std::cout << "  навыки: " << navyki[0] << " и " << navyki[1] << std::endl;
    std::cout << "  город: " << gorod << " или готовность к переезду" << std::endl;
    std::cout << "  зарплата: не больше " << potolok << std::endl;
    std::cout << "  опыт: место работы от " << opyt_mesyacev << " месяцев" << std::endl;

    auto spisok_navykov = make_array(navyki[0], navyki[1]);
    auto gorod_ili_pereezd = make_array(make_document(kvp("city", gorod)),
                                        make_document(kvp("ready_to_move", true)));
    auto opyt = make_document(kvp("$elemMatch",
                    make_document(kvp("months", make_document(kvp("$gte", opyt_mesyacev))))));

    // ── 2. Каждое требование по отдельности ───────────────────────────
    title("2. Сколько резюме проходит каждое требование");

    auto vsego = resumes.count_documents(make_document());
    std::vector<std::pair<std::string, bsoncxx::document::value>> kriterii;
    kriterii.emplace_back("навыки $all",
        make_document(kvp("skills", make_document(kvp("$all", spisok_navykov)))));
    kriterii.emplace_back("город или переезд $or",
        make_document(kvp("$or", gorod_ili_pereezd)));
    kriterii.emplace_back("зарплата $lte",
        make_document(kvp("salary", make_document(kvp("$lte", potolok)))));
    kriterii.emplace_back("опыт $elemMatch", make_document(kvp("experience", opyt)));
    kriterii.emplace_back("есть контакты $exists",
        make_document(kvp("contacts", make_document(kvp("$exists", true)))));
    for (const auto& kriteriy : kriterii) {
        auto skolko = resumes.count_documents(kriteriy.second.view());
        std::cout << "  " << pad(kriteriy.first, 24) << " " << right(std::to_string(skolko), 2)
                  << " из " << vsego << std::endl;
    }

    // ── 3. Все требования сразу ───────────────────────────────────────
    title("3. Кандидаты, проходящие все требования");

    auto filtr = make_document(
        kvp("skills", make_document(kvp("$all", spisok_navykov))),
        kvp("salary", make_document(kvp("$lte", potolok))),
        kvp("experience", opyt),
        kvp("$or", gorod_ili_pereezd));

    std::cout << "  " << pad("ФИО", 18) << " " << pad("город", 16) << " "
              << right("зарплата", 9) << "  навыки" << std::endl;
    mongocxx::options::find options;
    options.sort(make_document(kvp("salary", 1)));
    for (auto&& doc : resumes.find(filtr.view(), options)) {
        std::cout << "  " << pad(std::string(doc["fio"].get_string().value), 18)
                  << " " << pad(std::string(doc["city"].get_string().value), 16)
                  << " " << right(std::to_string(doc["salary"].get_int32().value), 9)
                  << "  " << cherez_zapyatuyu(doc["skills"].get_array().value) << std::endl;
    }
    std::cout << "  подошло: " << resumes.count_documents(filtr.view()) << std::endl;

    // ── 4. Поиск по части строки ──────────────────────────────────────
    title("4. Должности, начинающиеся с Junior");

    auto shablon = make_document(kvp("position",
        make_document(kvp("$regex", "^junior"), kvp("$options", "i"))));
    for (auto&& doc : resumes.find(shablon.view())) {
        std::cout << "  " << pad(std::string(doc["fio"].get_string().value), 18)
                  << " " << std::string(doc["position"].get_string().value) << std::endl;
    }

    // ── 5. Подходящие вакансии ────────────────────────────────────────
    title("5. Вакансии под ожидание 90 000");

    const int ozhidanie = 90000;
    auto vilka = make_document(
        kvp("salary.from", make_document(kvp("$lte", ozhidanie))),
        kvp("salary.to", make_document(kvp("$gte", ozhidanie))));
    for (auto&& doc : vacancies.find(vilka.view())) {
        auto zarplata = doc["salary"].get_document().value;
        std::cout << "  " << pad(std::string(doc["title"].get_string().value), 26)
                  << " " << pad(std::string(doc["company"].get_string().value), 16)
                  << " " << pad(std::string(doc["city"].get_string().value), 16)
                  << " " << zarplata["from"].get_int32().value << "–"
                  << zarplata["to"].get_int32().value << std::endl;
    }
    std::cout << "  подходит вакансий: " << vacancies.count_documents(vilka.view()) << std::endl;

    // ── 6. Собеседования за неделю ────────────────────────────────────
    title("6. Собеседования с 21 по 27 сентября");

    auto sep21 = bsoncxx::types::b_date{std::chrono::milliseconds{1789948800000}};  // 21.09.2026 00:00 UTC
    auto sep28 = bsoncxx::types::b_date{std::chrono::milliseconds{1790553600000}};  // 28.09.2026 00:00 UTC
    auto nedelya = make_document(kvp("when", make_document(kvp("$gte", sep21), kvp("$lt", sep28))));
    std::cout << "  всего за неделю: " << interviews.count_documents(nedelya.view()) << std::endl;
    for (const std::string& etap : {"скрининг", "техническое", "финальное"}) {
        auto filtr_etapa = make_document(
            kvp("when", make_document(kvp("$gte", sep21), kvp("$lt", sep28))),
            kvp("stage", etap));
        std::cout << "    " << pad(etap, 12) << " "
                  << right(std::to_string(interviews.count_documents(filtr_etapa.view())), 2) << std::endl;
    }
    auto ochno = make_document(kvp("when", make_document(kvp("$gte", sep21), kvp("$lt", sep28))),
                               kvp("format", "очно"));
    auto video = make_document(kvp("when", make_document(kvp("$gte", sep21), kvp("$lt", sep28))),
                               kvp("format", "видеозвонок"));
    std::cout << "  очно: " << interviews.count_documents(ochno.view())
              << ", видеозвонком: " << interviews.count_documents(video.view()) << std::endl;

    // ── 7. Проверка данных ────────────────────────────────────────────
    title("7. Проверка данных");

    std::vector<std::tuple<std::string, mongocxx::collection, bsoncxx::document::value>> proverki;
    proverki.emplace_back("резюме без даты обновления", resumes,
        make_document(kvp("updated", make_document(kvp("$exists", false)))));
    proverki.emplace_back("зарплата записана не числом", resumes,
        make_document(kvp("salary", make_document(kvp("$not", make_document(kvp("$type", "number")))))));
    proverki.emplace_back("пустой список мест работы", resumes,
        make_document(kvp("experience", make_document(kvp("$size", 0)))));
    proverki.emplace_back("нет ни одного навыка", resumes,
        make_document(kvp("skills.0", make_document(kvp("$exists", false)))));
    proverki.emplace_back("перевёрнутая вилка", vacancies,
        make_document(kvp("$expr", make_document(kvp("$gt", make_array("$salary.from", "$salary.to"))))));
    proverki.emplace_back("вакансия закрыта", vacancies,
        make_document(kvp("is_open", make_document(kvp("$ne", true)))));
    for (auto& proverka : proverki) {
        auto skolko = std::get<1>(proverka).count_documents(std::get<2>(proverka).view());
        std::cout << "  " << pad(std::get<0>(proverka), 30) << " " << right(std::to_string(skolko), 2)
                  << (skolko ? "  ← проверить" : "") << std::endl;
    }

    // ── 8. Кто не подошёл и почему ────────────────────────────────────
    title("8. Кто не прошёл отбор");

    for (auto&& doc : resumes.find(make_document())) {
        std::vector<std::string> prichiny;
        for (const auto& nuzhen : navyki) {
            bool est = false;
            for (auto&& navyk : doc["skills"].get_array().value)
                if (std::string(navyk.get_string().value) == nuzhen) est = true;
            if (!est) { prichiny.push_back("нет навыков"); break; }
        }
        if (doc["salary"].get_int32().value > potolok) prichiny.push_back("зарплата выше потолка");
        bool gotov = doc["ready_to_move"] && doc["ready_to_move"].get_bool().value;
        if (std::string(doc["city"].get_string().value) != gorod && !gotov)
            prichiny.push_back("другой город без переезда");
        bool hvatit_opyta = false;
        if (doc["experience"])
            for (auto&& mesto : doc["experience"].get_array().value)
                if (mesto.get_document().value["months"].get_int32().value >= opyt_mesyacev)
                    hvatit_opyta = true;
        if (!hvatit_opyta) prichiny.push_back("мало опыта");
        if (!prichiny.empty()) {
            std::cout << "  " << pad(std::string(doc["fio"].get_string().value), 18) << " ";
            for (size_t i = 0; i < prichiny.size(); ++i)
                std::cout << (i ? "; " : "") << prichiny[i];
            std::cout << std::endl;
        }
    }
}
