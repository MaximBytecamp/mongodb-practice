// Подбор кандидатов: что умеет язык фильтров из модуля 2.
//
// Скрипт решает прикладную задачу на базе hh: подобрать кандидатов под
// требования вакансии, посмотреть, сколько резюме проходит каждое требование
// по отдельности, найти подходящие вакансии и проверить данные на пропуски,
// неверные типы и несогласованные поля.
//
//	cd scripts
//	go run hh_search.go
//
//	docker compose run --rm go scripts/hh_search.go    # в контейнере
package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"strings"
	"time"
	"unicode/utf8"

	"go.mongodb.org/mongo-driver/v2/bson"
	"go.mongodb.org/mongo-driver/v2/mongo"
	"go.mongodb.org/mongo-driver/v2/mongo/options"
)

// Resume — поля резюме, которые читает отбор.
type Resume struct {
	Fio         string   `bson:"fio"`
	Position    string   `bson:"position"`
	City        string   `bson:"city"`
	Salary      int      `bson:"salary"`
	ReadyToMove bool     `bson:"ready_to_move"`
	Skills      []string `bson:"skills"`
	Experience  []struct {
		Company string `bson:"company"`
		Role    string `bson:"role"`
		Months  int    `bson:"months"`
	} `bson:"experience"`
}

// Vacancy — вакансия с вилкой зарплаты во вложенном документе.
type Vacancy struct {
	Title   string `bson:"title"`
	Company string `bson:"company"`
	City    string `bson:"city"`
	Salary  struct {
		From int `bson:"from"`
		To   int `bson:"to"`
	} `bson:"salary"`
}

func pad(text string, width int) string {
	if n := utf8.RuneCountInString(text); n < width {
		return text + strings.Repeat(" ", width-n)
	}
	return text
}

func right(text string, width int) string {
	if n := utf8.RuneCountInString(text); n < width {
		return strings.Repeat(" ", width-n) + text
	}
	return text
}

func title(text string) {
	fmt.Println("\n" + text)
	fmt.Println(strings.Repeat("─", utf8.RuneCountInString(text)))
}

// Требования вакансии.
var (
	navyki       = []string{"Python", "SQL"}
	gorod        = "Ярославль"
	potolok      = 100000
	opytMesyacev = 6
)

func main() {
	uri := os.Getenv("MONGO_URI")
	if uri == "" {
		uri = "mongodb://localhost:27017"
	}
	client, err := mongo.Connect(options.Client().ApplyURI(uri))
	if err != nil {
		log.Fatal(err)
	}
	ctx := context.Background()
	defer client.Disconnect(ctx)

	hh := client.Database("hh")
	resumes := hh.Collection("resumes")
	vacancies := hh.Collection("vacancies")
	interviews := hh.Collection("interviews")

	// ── 1. Требования ─────────────────────────────────────────────────
	title("1. Требования")

	fmt.Printf("  навыки: %s\n", strings.Join(navyki, " и "))
	fmt.Printf("  город: %s или готовность к переезду\n", gorod)
	fmt.Printf("  зарплата: не больше %d\n", potolok)
	fmt.Printf("  опыт: место работы от %d месяцев\n", opytMesyacev)

	spisokNavykov := bson.A{}
	for _, n := range navyki {
		spisokNavykov = append(spisokNavykov, n)
	}
	gorodIliPereezd := bson.A{
		bson.D{{Key: "city", Value: gorod}},
		bson.D{{Key: "ready_to_move", Value: true}},
	}
	opyt := bson.D{{Key: "$elemMatch", Value: bson.D{
		{Key: "months", Value: bson.D{{Key: "$gte", Value: opytMesyacev}}}}}}

	// ── 2. Каждое требование по отдельности ───────────────────────────
	title("2. Сколько резюме проходит каждое требование")

	vsego, _ := resumes.CountDocuments(ctx, bson.D{})
	kriterii := []struct {
		nazvanie string
		filtr    bson.D
	}{
		{"навыки $all", bson.D{{Key: "skills", Value: bson.D{{Key: "$all", Value: spisokNavykov}}}}},
		{"город или переезд $or", bson.D{{Key: "$or", Value: gorodIliPereezd}}},
		{"зарплата $lte", bson.D{{Key: "salary", Value: bson.D{{Key: "$lte", Value: potolok}}}}},
		{"опыт $elemMatch", bson.D{{Key: "experience", Value: opyt}}},
		{"есть контакты $exists", bson.D{{Key: "contacts", Value: bson.D{{Key: "$exists", Value: true}}}}},
	}
	for _, k := range kriterii {
		skolko, _ := resumes.CountDocuments(ctx, k.filtr)
		fmt.Printf("  %s %s из %d\n", pad(k.nazvanie, 24), right(fmt.Sprint(skolko), 2), vsego)
	}

	// ── 3. Все требования сразу ───────────────────────────────────────
	title("3. Кандидаты, проходящие все требования")

	filtr := bson.D{
		{Key: "skills", Value: bson.D{{Key: "$all", Value: spisokNavykov}}},
		{Key: "salary", Value: bson.D{{Key: "$lte", Value: potolok}}},
		{Key: "experience", Value: opyt},
		{Key: "$or", Value: gorodIliPereezd},
	}
	fmt.Printf("  %s %s %s  навыки\n", pad("ФИО", 18), pad("город", 16), right("зарплата", 9))
	cursor, _ := resumes.Find(ctx, filtr, options.Find().SetSort(bson.D{{Key: "salary", Value: 1}}))
	var podoshli []Resume
	cursor.All(ctx, &podoshli)
	for _, r := range podoshli {
		fmt.Printf("  %s %s %s  %s\n", pad(r.Fio, 18), pad(r.City, 16),
			right(fmt.Sprint(r.Salary), 9), strings.Join(r.Skills, ", "))
	}
	fmt.Println("  подошло:", len(podoshli))

	// ── 4. Поиск по части строки ──────────────────────────────────────
	title("4. Должности, начинающиеся с Junior")

	cursor, _ = resumes.Find(ctx, bson.D{{Key: "position", Value: bson.D{
		{Key: "$regex", Value: "^junior"}, {Key: "$options", Value: "i"}}}})
	var junior []Resume
	cursor.All(ctx, &junior)
	for _, r := range junior {
		fmt.Printf("  %s %s\n", pad(r.Fio, 18), r.Position)
	}

	// ── 5. Подходящие вакансии ────────────────────────────────────────
	title("5. Вакансии под ожидание 90 000")

	ozhidanie := 90000
	vilka := bson.D{
		{Key: "salary.from", Value: bson.D{{Key: "$lte", Value: ozhidanie}}},
		{Key: "salary.to", Value: bson.D{{Key: "$gte", Value: ozhidanie}}},
	}
	cursor, _ = vacancies.Find(ctx, vilka)
	var podhodyat []Vacancy
	cursor.All(ctx, &podhodyat)
	for _, v := range podhodyat {
		fmt.Printf("  %s %s %s %d–%d\n", pad(v.Title, 26), pad(v.Company, 16), pad(v.City, 16),
			v.Salary.From, v.Salary.To)
	}
	fmt.Println("  подходит вакансий:", len(podhodyat))

	// ── 6. Собеседования за неделю ────────────────────────────────────
	title("6. Собеседования с 21 по 27 сентября")

	sep21 := time.Date(2026, 9, 21, 0, 0, 0, 0, time.UTC)
	sep28 := time.Date(2026, 9, 28, 0, 0, 0, 0, time.UTC)
	nedelya := bson.D{{Key: "when", Value: bson.D{{Key: "$gte", Value: sep21}, {Key: "$lt", Value: sep28}}}}
	zaNedelyu, _ := interviews.CountDocuments(ctx, nedelya)
	fmt.Println("  всего за неделю:", zaNedelyu)
	// К общему фильтру добавляем условие копией: append по общему срезу
	// может перезаписать данные соседних вызовов.
	sUsloviem := func(key, value string) bson.D {
		return append(append(bson.D{}, nedelya...), bson.E{Key: key, Value: value})
	}
	for _, etap := range []string{"скрининг", "техническое", "финальное"} {
		skolko, _ := interviews.CountDocuments(ctx, sUsloviem("stage", etap))
		fmt.Printf("    %s %s\n", pad(etap, 12), right(fmt.Sprint(skolko), 2))
	}
	ochno, _ := interviews.CountDocuments(ctx, sUsloviem("format", "очно"))
	video, _ := interviews.CountDocuments(ctx, sUsloviem("format", "видеозвонок"))
	fmt.Printf("  очно: %d, видеозвонком: %d\n", ochno, video)

	// ── 7. Проверка данных ────────────────────────────────────────────
	title("7. Проверка данных")

	proverki := []struct {
		nazvanie string
		col      *mongo.Collection
		filtr    bson.D
	}{
		{"резюме без даты обновления", resumes,
			bson.D{{Key: "updated", Value: bson.D{{Key: "$exists", Value: false}}}}},
		{"зарплата записана не числом", resumes,
			bson.D{{Key: "salary", Value: bson.D{{Key: "$not", Value: bson.D{{Key: "$type", Value: "number"}}}}}}},
		{"пустой список мест работы", resumes,
			bson.D{{Key: "experience", Value: bson.D{{Key: "$size", Value: 0}}}}},
		{"нет ни одного навыка", resumes,
			bson.D{{Key: "skills.0", Value: bson.D{{Key: "$exists", Value: false}}}}},
		{"перевёрнутая вилка", vacancies,
			bson.D{{Key: "$expr", Value: bson.D{{Key: "$gt", Value: bson.A{"$salary.from", "$salary.to"}}}}}},
		{"вакансия закрыта", vacancies,
			bson.D{{Key: "is_open", Value: bson.D{{Key: "$ne", Value: true}}}}},
	}
	for _, p := range proverki {
		skolko, _ := p.col.CountDocuments(ctx, p.filtr)
		pometka := ""
		if skolko > 0 {
			pometka = "  ← проверить"
		}
		fmt.Printf("  %s %s%s\n", pad(p.nazvanie, 30), right(fmt.Sprint(skolko), 2), pometka)
	}

	// ── 8. Кто не подошёл и почему ────────────────────────────────────
	title("8. Кто не прошёл отбор")

	cursor, _ = resumes.Find(ctx, bson.D{})
	var vse []Resume
	cursor.All(ctx, &vse)
	for _, r := range vse {
		var prichiny []string
		for _, nuzhen := range navyki {
			est := false
			for _, navyk := range r.Skills {
				if navyk == nuzhen {
					est = true
				}
			}
			if !est {
				prichiny = append(prichiny, "нет навыков")
				break
			}
		}
		if r.Salary > potolok {
			prichiny = append(prichiny, "зарплата выше потолка")
		}
		if r.City != gorod && !r.ReadyToMove {
			prichiny = append(prichiny, "другой город без переезда")
		}
		hvatitOpyta := false
		for _, mesto := range r.Experience {
			if mesto.Months >= opytMesyacev {
				hvatitOpyta = true
			}
		}
		if !hvatitOpyta {
			prichiny = append(prichiny, "мало опыта")
		}
		if len(prichiny) > 0 {
			fmt.Printf("  %s %s\n", pad(r.Fio, 18), strings.Join(prichiny, "; "))
		}
	}
}
