# Подбор кандидатов: что умеет язык фильтров из модуля 2.
#
# Скрипт решает прикладную задачу на базе hh: подобрать кандидатов под
# требования вакансии, посмотреть, сколько резюме проходит каждое требование
# по отдельности, найти подходящие вакансии и проверить данные на пропуски,
# неверные типы и несогласованные поля.
#
#     cd scripts
#     ruby hh_search.rb
#
#     docker compose run --rm ruby scripts/hh_search.rb    # в контейнере

require "mongo"

Mongo::Logger.logger.level = Logger::WARN

client = Mongo::Client.new(ENV.fetch("MONGO_URI", "mongodb://localhost:27017/"))
hh = client.use("hh").database
resumes = hh[:resumes]
vacancies = hh[:vacancies]
interviews = hh[:interviews]

def title(text)
  puts "\n" + text
  puts "─" * text.length
end

def pad(text, width)
  text.to_s.ljust(width)
end

# ── 1. Требования вакансии ────────────────────────────────────────────────
NAVYKI = ["Python", "SQL"].freeze
GOROD = "Ярославль"
POTOLOK = 100_000
OPYT_MESYACEV = 6

title("1. Требования")
puts "  навыки: #{NAVYKI.join(' и ')}"
puts "  город: #{GOROD} или готовность к переезду"
puts "  зарплата: не больше #{POTOLOK}"
puts "  опыт: место работы от #{OPYT_MESYACEV} месяцев"

# ── 2. Каждое требование по отдельности ───────────────────────────────────
title("2. Сколько резюме проходит каждое требование")

kriterii = [
  ["навыки $all", { "skills" => { "$all" => NAVYKI } }],
  ["город или переезд $or", { "$or" => [{ "city" => GOROD }, { "ready_to_move" => true }] }],
  ["зарплата $lte", { "salary" => { "$lte" => POTOLOK } }],
  ["опыт $elemMatch", { "experience" => { "$elemMatch" => { "months" => { "$gte" => OPYT_MESYACEV } } } }],
  ["есть контакты $exists", { "contacts" => { "$exists" => true } }],
]
vsego = resumes.count_documents({})
kriterii.each do |nazvanie, filtr|
  puts "  #{pad(nazvanie, 24)} #{resumes.count_documents(filtr).to_s.rjust(2)} из #{vsego}"
end

# ── 3. Все требования сразу ───────────────────────────────────────────────
title("3. Кандидаты, проходящие все требования")

filtr = {
  "skills" => { "$all" => NAVYKI },
  "salary" => { "$lte" => POTOLOK },
  "experience" => { "$elemMatch" => { "months" => { "$gte" => OPYT_MESYACEV } } },
  "$or" => [{ "city" => GOROD }, { "ready_to_move" => true }],
}
puts "  #{pad('ФИО', 18)} #{pad('город', 16)} #{'зарплата'.rjust(9)}  навыки"
resumes.find(filtr).sort({ "salary" => 1 }).each do |doc|
  puts "  #{pad(doc['fio'], 18)} #{pad(doc['city'], 16)} #{doc['salary'].to_s.rjust(9)}  #{doc['skills'].join(', ')}"
end
puts "  подошло: #{resumes.count_documents(filtr)}"

# ── 4. Поиск по части строки ──────────────────────────────────────────────
title("4. Должности, начинающиеся с Junior")

resumes.find({ "position" => { "$regex" => "^junior", "$options" => "i" } },
             projection: { "_id" => 0, "fio" => 1, "position" => 1 })
       .each { |doc| puts "  #{pad(doc['fio'], 18)} #{doc['position']}" }

# ── 5. Подходящие вакансии ────────────────────────────────────────────────
title("5. Вакансии под ожидание 90 000")

OZHIDANIE = 90_000
vilka = { "salary.from" => { "$lte" => OZHIDANIE }, "salary.to" => { "$gte" => OZHIDANIE } }
vacancies.find(vilka, projection: { "_id" => 0, "title" => 1, "company" => 1, "city" => 1, "salary" => 1 })
         .each do |doc|
  zarplata = doc["salary"]
  puts "  #{pad(doc['title'], 26)} #{pad(doc['company'], 16)} #{pad(doc['city'], 16)} " \
       "#{zarplata['from']}–#{zarplata['to']}"
end
puts "  подходит вакансий: #{vacancies.count_documents(vilka)}"

# ── 6. Собеседования за неделю ────────────────────────────────────────────
title("6. Собеседования с 21 по 27 сентября")

nedelya = { "when" => { "$gte" => Time.utc(2026, 9, 21), "$lt" => Time.utc(2026, 9, 28) } }
puts "  всего за неделю: #{interviews.count_documents(nedelya)}"
["скрининг", "техническое", "финальное"].each do |etap|
  puts "    #{pad(etap, 12)} #{interviews.count_documents(nedelya.merge('stage' => etap)).to_s.rjust(2)}"
end
puts "  очно: #{interviews.count_documents(nedelya.merge('format' => 'очно'))}, " \
     "видеозвонком: #{interviews.count_documents(nedelya.merge('format' => 'видеозвонок'))}"

# ── 7. Проверка данных ────────────────────────────────────────────────────
title("7. Проверка данных")

proverki = [
  ["резюме без даты обновления", resumes, { "updated" => { "$exists" => false } }],
  ["зарплата записана не числом", resumes, { "salary" => { "$not" => { "$type" => "number" } } }],
  ["пустой список мест работы", resumes, { "experience" => { "$size" => 0 } }],
  ["нет ни одного навыка", resumes, { "skills.0" => { "$exists" => false } }],
  ["перевёрнутая вилка", vacancies, { "$expr" => { "$gt" => ["$salary.from", "$salary.to"] } }],
  ["вакансия закрыта", vacancies, { "is_open" => { "$ne" => true } }],
]
proverki.each do |nazvanie, col, filtr_proverki|
  skolko = col.count_documents(filtr_proverki)
  puts "  #{pad(nazvanie, 30)} #{skolko.to_s.rjust(2)}#{skolko.zero? ? '' : '  ← проверить'}"
end

# ── 8. Кто не подошёл и почему ────────────────────────────────────────────
title("8. Кто не прошёл отбор")

resumes.find({}, projection: { "_id" => 0, "fio" => 1, "skills" => 1, "salary" => 1,
                               "city" => 1, "ready_to_move" => 1, "experience" => 1 }).each do |doc|
  prichiny = []
  prichiny << "нет навыков" unless (NAVYKI - (doc["skills"] || [])).empty?
  prichiny << "зарплата выше потолка" if doc["salary"] > POTOLOK
  prichiny << "другой город без переезда" if doc["city"] != GOROD && !doc["ready_to_move"]
  prichiny << "мало опыта" unless (doc["experience"] || []).any? { |m| (m["months"] || 0) >= OPYT_MESYACEV }
  puts "  #{pad(doc['fio'], 18)} #{prichiny.join('; ')}" unless prichiny.empty?
end

client.close
