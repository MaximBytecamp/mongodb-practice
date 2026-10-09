// Аудит журнала сервиса: что умеют правила, индексы и explain из модуля 3.
//
// Скрипт работает с копией журнала logs.events в песочнице: проверяет события
// правилом $jsonSchema и включает его как валидатор, снимает планы трёх
// запросов дежурного, строит под них индексы по правилу ESR и сравнивает
// планы до и после. База logs остаётся без изменений.
//
//	cd scripts
//	go run logs_audit.go
//
//	docker compose run --rm go scripts/logs_audit.go    # в контейнере
package main

import (
	"context"
	"errors"
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

func pad(text string, width int) string {
	if n := utf8.RuneCountInString(text); n < width {
		return text + strings.Repeat(" ", width-n)
	}
	return text
}

func title(text string) {
	fmt.Println("\n" + text)
	fmt.Println(strings.Repeat("─", utf8.RuneCountInString(text)))
}

// summary — план и счётчики explain: стадии сверху вниз, вернул, ключей, документов.
func summary(sandbox *mongo.Database, find bson.D) (string, int64, int64, int64) {
	cmd := append(bson.D{{Key: "find", Value: "audit"}}, find...)
	var info bson.Raw
	sandbox.RunCommand(context.Background(),
		bson.D{{Key: "explain", Value: cmd}, {Key: "verbosity", Value: "executionStats"}}).Decode(&info)
	stage := info.Lookup("queryPlanner", "winningPlan").Document()
	if inner, ok := stage.Lookup("queryPlan").DocumentOK(); ok {
		stage = inner
	}
	steps := []string{}
	for {
		step := stage.Lookup("stage").StringValue()
		if name, ok := stage.Lookup("indexName").StringValueOK(); ok {
			step += " " + name
		}
		steps = append(steps, step)
		next, ok := stage.Lookup("inputStage").DocumentOK()
		if !ok {
			break
		}
		stage = next
	}
	stats := info.Lookup("executionStats").Document()
	return strings.Join(steps, " → "), stats.Lookup("nReturned").AsInt64(),
		stats.Lookup("totalKeysExamined").AsInt64(), stats.Lookup("totalDocsExamined").AsInt64()
}

// Запрос дежурного: название и части команды find.
type query struct {
	name string
	find bson.D
}

var queries = []query{
	{"ошибки платежей", bson.D{
		{Key: "filter", Value: bson.D{{Key: "service", Value: "payments"}, {Key: "status", Value: bson.D{{Key: "$gte", Value: 500}}}}},
		{Key: "sort", Value: bson.D{{Key: "ts", Value: -1}}},
		{Key: "limit", Value: 5}}},
	{"медленный поиск", bson.D{
		{Key: "filter", Value: bson.D{{Key: "service", Value: "search"}, {Key: "duration_ms", Value: bson.D{{Key: "$gte", Value: 1000}}}}},
		{Key: "sort", Value: bson.D{{Key: "duration_ms", Value: -1}}},
		{Key: "limit", Value: 5}}},
	{"события покупателя", bson.D{
		{Key: "filter", Value: bson.D{{Key: "user_id", Value: "c-015"}}},
		{Key: "sort", Value: bson.D{{Key: "ts", Value: -1}}}}},
}

func report(sandbox *mongo.Database) {
	fmt.Printf("  %s %6s %7s %11s\n", pad("запрос", 20), "вернул", "ключей", "документов")
	for _, q := range queries {
		plan, returned, keys, docs := summary(sandbox, q.find)
		fmt.Printf("  %s %6d %7d %11d\n", pad(q.name, 20), returned, keys, docs)
		fmt.Println("      " + plan)
	}
}

// Причины отказа из errInfo: недостающие поля и нарушенные условия полей.
type schemaRule struct {
	OperatorName           string   `bson:"operatorName"`
	MissingProperties      []string `bson:"missingProperties"`
	PropertiesNotSatisfied []struct {
		PropertyName string `bson:"propertyName"`
		Details      []struct {
			OperatorName string `bson:"operatorName"`
		} `bson:"details"`
	} `bson:"propertiesNotSatisfied"`
}

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

	events := client.Database("logs").Collection("events")
	sandbox := client.Database("sandbox")
	audit := sandbox.Collection("audit")

	// ── 1. Копия журнала ──────────────────────────────────────────────────
	title("1. Копия журнала в песочнице")

	audit.Drop(ctx)
	var all []bson.D
	cursor, _ := events.Find(ctx, bson.D{})
	cursor.All(ctx, &all)
	docs := make([]any, len(all))
	for i := range all {
		docs[i] = all[i]
	}
	audit.InsertMany(ctx, docs)
	n, _ := audit.CountDocuments(ctx, bson.D{})
	fmt.Printf("  событий в копии: %d\n", n)
	specs, _ := audit.Indexes().ListSpecifications(ctx)
	fmt.Printf("  индексов: %d\n", len(specs))

	// ── 2. Правило для событий ────────────────────────────────────────────
	title("2. Правило для событий")

	rule := bson.D{
		{Key: "bsonType", Value: "object"},
		{Key: "required", Value: bson.A{"ts", "service", "level", "route", "status", "duration_ms"}},
		{Key: "properties", Value: bson.D{
			{Key: "ts", Value: bson.D{{Key: "bsonType", Value: "date"}}},
			{Key: "service", Value: bson.D{{Key: "enum", Value: bson.A{"api-gateway", "notifier", "orders", "payments", "search"}}}},
			{Key: "level", Value: bson.D{{Key: "enum", Value: bson.A{"info", "warn", "error"}}}},
			{Key: "route", Value: bson.D{{Key: "bsonType", Value: "string"}}},
			{Key: "status", Value: bson.D{{Key: "bsonType", Value: "int"}, {Key: "minimum", Value: 100}, {Key: "maximum", Value: 599}}},
			{Key: "duration_ms", Value: bson.D{{Key: "bsonType", Value: "number"}, {Key: "minimum", Value: 0}}},
		}},
	}
	fits, _ := audit.CountDocuments(ctx, bson.D{{Key: "$jsonSchema", Value: rule}})
	misfits, _ := audit.CountDocuments(ctx, bson.D{{Key: "$nor", Value: bson.A{bson.D{{Key: "$jsonSchema", Value: rule}}}}})
	fmt.Printf("  соответствуют правилу:    %d\n", fits)
	fmt.Printf("  не соответствуют правилу: %d\n", misfits)

	sandbox.RunCommand(ctx, bson.D{
		{Key: "collMod", Value: "audit"},
		{Key: "validator", Value: bson.D{{Key: "$jsonSchema", Value: rule}}},
		{Key: "validationLevel", Value: "strict"},
		{Key: "validationAction", Value: "error"}})
	fmt.Println("  правило включено: strict, error")

	// ── 3. Проверка правила на записи ─────────────────────────────────────
	title("3. Неверные события")

	event := func(key string, value any) bson.D {
		doc := bson.D{
			{Key: "ts", Value: time.Date(2026, 9, 2, 18, 30, 0, 0, time.UTC)},
			{Key: "service", Value: "payments"},
			{Key: "level", Value: "error"},
			{Key: "route", Value: "/api/pay"},
			{Key: "status", Value: 500},
			{Key: "duration_ms", Value: 300}}
		out := bson.D{}
		for _, field := range doc {
			if field.Key != key {
				out = append(out, field)
			} else if value != nil {
				out = append(out, bson.E{Key: key, Value: value})
			}
		}
		return out
	}
	wrong := []struct {
		label string
		doc   bson.D
	}{
		{"статус 700", event("status", 700)},
		{"уровень fatal", event("level", "fatal")},
		{"без времени", event("ts", nil)},
	}
	for _, w := range wrong {
		_, err := audit.InsertOne(ctx, w.doc)
		var we mongo.WriteException
		if !errors.As(err, &we) {
			fmt.Printf("  %s записано\n", pad(w.label, 14))
			continue
		}
		var info struct {
			Details struct {
				Rules []schemaRule `bson:"schemaRulesNotSatisfied"`
			} `bson:"details"`
		}
		bson.Unmarshal(we.WriteErrors[0].Details, &info)
		reasons := []string{}
		for _, r := range info.Details.Rules {
			if r.OperatorName == "required" {
				for _, name := range r.MissingProperties {
					reasons = append(reasons, "нет "+name)
				}
			}
			if r.OperatorName == "properties" {
				for _, prop := range r.PropertiesNotSatisfied {
					for _, d := range prop.Details {
						reasons = append(reasons, prop.PropertyName+": "+d.OperatorName)
					}
				}
			}
		}
		fmt.Printf("  %s отклонено, код %d: %s\n", pad(w.label, 14), we.WriteErrors[0].Code, strings.Join(reasons, ", "))
	}
	n, _ = audit.CountDocuments(ctx, bson.D{})
	fmt.Printf("  событий в копии: %d\n", n)

	// ── 4. Запросы дежурного без индексов ─────────────────────────────────
	title("4. Запросы дежурного без индексов")
	report(sandbox)

	// ── 5. Индексы по правилу ESR ─────────────────────────────────────────
	title("5. Индексы: равенство, сортировка, диапазон")

	for _, keys := range []bson.D{
		{{Key: "service", Value: 1}, {Key: "ts", Value: -1}, {Key: "status", Value: 1}},
		{{Key: "service", Value: 1}, {Key: "duration_ms", Value: -1}},
		{{Key: "user_id", Value: 1}, {Key: "ts", Value: -1}},
	} {
		name, _ := audit.Indexes().CreateOne(ctx, mongo.IndexModel{Keys: keys})
		fmt.Println("  создан " + name)
	}

	// ── 6. Те же запросы с индексами ──────────────────────────────────────
	title("6. Запросы дежурного с индексами")
	report(sandbox)

	// ── 7. Пять последних ошибок платежей ─────────────────────────────────
	title("7. Пять последних ошибок платежей")

	filtr := queries[0].find[0].Value // фильтр первого запроса: первое поле его команды find
	last, _ := audit.Find(ctx, filtr, options.Find().SetSort(bson.D{{Key: "ts", Value: -1}}).SetLimit(5))
	for last.Next(ctx) {
		doc := last.Current
		ts := doc.Lookup("ts").Time().UTC()
		fmt.Printf("  %s  %s %d  %5d мс\n", ts.Format("02.01 15:04:05"), pad(doc.Lookup("route").StringValue(), 18),
			doc.Lookup("status").AsInt64(), doc.Lookup("duration_ms").AsInt64())
	}
}
