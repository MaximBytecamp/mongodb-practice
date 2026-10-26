// Витрина продаж за квартал: конвейеры агрегации из модуля 4.
//
// Скрипт строит отчёт по заказам магазина за второй квартал 2026 года
// (апрель — июнь): статусы, выручка по месяцам и городам, категории и товары,
// чеки по суммам, самый дорогой заказ каждого города, оплата и доставка.
// Базы только читаются.
//
//	cd scripts
//	go run sales_showcase.go
//
//	docker compose run --rm go scripts/sales_showcase.go    # в контейнере
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

// Число из ответа сервера: суммы приходят как int32 или int64, среднее после $round — как double.
func number(value bson.RawValue) int64 {
	switch value.Type {
	case bson.TypeInt32:
		return int64(value.Int32())
	case bson.TypeDouble:
		return int64(value.Double())
	}
	return value.Int64()
}

func date(year int, month time.Month, day int) time.Time {
	return time.Date(year, month, day, 0, 0, 0, 0, time.UTC)
}

// Все документы результата конвейера.
func run(orders *mongo.Collection, pipeline mongo.Pipeline) []bson.Raw {
	ctx := context.Background()
	cursor, err := orders.Aggregate(ctx, pipeline)
	if err != nil {
		log.Fatal(err)
	}
	var docs []bson.Raw
	for cursor.Next(ctx) {
		docs = append(docs, append(bson.Raw{}, cursor.Current...))
	}
	return docs
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
	defer client.Disconnect(context.Background())
	orders := client.Database("shop").Collection("orders")

	quarter := bson.D{{Key: "created", Value: bson.D{{Key: "$gte", Value: date(2026, 4, 1)}, {Key: "$lt", Value: date(2026, 7, 1)}}}}
	delivered := bson.D{{Key: "$match", Value: append(append(bson.D{}, quarter...), bson.E{Key: "status", Value: "доставлен"})}}

	// ── 1. Заказы квартала ────────────────────────────────────────────────
	title("1. Заказы квартала")

	for _, doc := range run(orders, mongo.Pipeline{
		{{Key: "$match", Value: quarter}},
		{{Key: "$sortByCount", Value: "$status"}},
	}) {
		fmt.Printf("  %s %4d\n", pad(doc.Lookup("_id").StringValue(), 12), number(doc.Lookup("count")))
	}

	total := run(orders, mongo.Pipeline{
		delivered,
		{{Key: "$group", Value: bson.D{
			{Key: "_id", Value: nil},
			{Key: "orders", Value: bson.D{{Key: "$sum", Value: 1}}},
			{Key: "revenue", Value: bson.D{{Key: "$sum", Value: "$total"}}},
			{Key: "avg", Value: bson.D{{Key: "$avg", Value: "$total"}}}}}},
		{{Key: "$project", Value: bson.D{
			{Key: "orders", Value: 1}, {Key: "revenue", Value: 1},
			{Key: "avg", Value: bson.D{{Key: "$round", Value: bson.A{"$avg", 0}}}}}}},
	})[0]
	fmt.Printf("  доставлено: %d, выручка %d, средний чек %d\n",
		number(total.Lookup("orders")), number(total.Lookup("revenue")), number(total.Lookup("avg")))

	// ── 2. Выручка по месяцам ─────────────────────────────────────────────
	title("2. Выручка по месяцам")

	months := run(orders, mongo.Pipeline{
		delivered,
		{{Key: "$bucket", Value: bson.D{
			{Key: "groupBy", Value: "$created"},
			{Key: "boundaries", Value: bson.A{date(2026, 4, 1), date(2026, 5, 1), date(2026, 6, 1), date(2026, 7, 1)}},
			{Key: "output", Value: bson.D{
				{Key: "orders", Value: bson.D{{Key: "$sum", Value: 1}}},
				{Key: "revenue", Value: bson.D{{Key: "$sum", Value: "$total"}}}}}}}},
	})
	for i, name := range []string{"апрель", "май", "июнь"} {
		fmt.Printf("  %s %4d %10d\n", pad(name, 12), number(months[i].Lookup("orders")), number(months[i].Lookup("revenue")))
	}

	// ── 3. Города ─────────────────────────────────────────────────────────
	title("3. Города")

	fmt.Printf("  %s %7s %10s %12s\n", pad("город", 16), "заказов", "выручка", "средний чек")
	for _, doc := range run(orders, mongo.Pipeline{
		delivered,
		{{Key: "$group", Value: bson.D{
			{Key: "_id", Value: "$delivery.city"},
			{Key: "orders", Value: bson.D{{Key: "$sum", Value: 1}}},
			{Key: "revenue", Value: bson.D{{Key: "$sum", Value: "$total"}}},
			{Key: "avg", Value: bson.D{{Key: "$avg", Value: "$total"}}}}}},
		{{Key: "$project", Value: bson.D{
			{Key: "orders", Value: 1}, {Key: "revenue", Value: 1},
			{Key: "avg", Value: bson.D{{Key: "$round", Value: bson.A{"$avg", 0}}}}}}},
		{{Key: "$sort", Value: bson.D{{Key: "revenue", Value: -1}}}},
	}) {
		fmt.Printf("  %s %7d %10d %12d\n", pad(doc.Lookup("_id").StringValue(), 16),
			number(doc.Lookup("orders")), number(doc.Lookup("revenue")), number(doc.Lookup("avg")))
	}

	// ── 4. Категории ──────────────────────────────────────────────────────
	title("4. Категории")

	fmt.Printf("  %s %5s %10s\n", pad("категория", 16), "штук", "выручка")
	for _, doc := range run(orders, mongo.Pipeline{
		delivered,
		{{Key: "$unwind", Value: "$items"}},
		{{Key: "$group", Value: bson.D{
			{Key: "_id", Value: "$items.category"},
			{Key: "qty", Value: bson.D{{Key: "$sum", Value: "$items.qty"}}},
			{Key: "revenue", Value: bson.D{{Key: "$sum", Value: bson.D{{Key: "$multiply", Value: bson.A{"$items.qty", "$items.price"}}}}}}}}},
		{{Key: "$sort", Value: bson.D{{Key: "revenue", Value: -1}}}},
	}) {
		fmt.Printf("  %s %5d %10d\n", pad(doc.Lookup("_id").StringValue(), 16),
			number(doc.Lookup("qty")), number(doc.Lookup("revenue")))
	}

	// ── 5. Пять самых продаваемых товаров ─────────────────────────────────
	title("5. Пять самых продаваемых товаров")

	for i, doc := range run(orders, mongo.Pipeline{
		delivered,
		{{Key: "$unwind", Value: "$items"}},
		{{Key: "$group", Value: bson.D{
			{Key: "_id", Value: "$items.title"},
			{Key: "qty", Value: bson.D{{Key: "$sum", Value: "$items.qty"}}}}}},
		{{Key: "$sort", Value: bson.D{{Key: "qty", Value: -1}, {Key: "_id", Value: 1}}}},
		{{Key: "$limit", Value: 5}},
	}) {
		fmt.Printf("  %d. %s %3d шт.\n", i+1, pad(doc.Lookup("_id").StringValue(), 32), number(doc.Lookup("qty")))
	}

	// ── 6. Чеки по суммам ─────────────────────────────────────────────────
	title("6. Чеки по суммам")

	labels := map[int64]string{0: "до 10 000", 10000: "10 000 — 50 000", 50000: "50 000 — 200 000", 200000: "от 200 000"}
	for _, doc := range run(orders, mongo.Pipeline{
		delivered,
		{{Key: "$bucket", Value: bson.D{
			{Key: "groupBy", Value: "$total"},
			{Key: "boundaries", Value: bson.A{0, 10000, 50000, 200000, 500000}},
			{Key: "output", Value: bson.D{{Key: "orders", Value: bson.D{{Key: "$sum", Value: 1}}}}}}}},
	}) {
		fmt.Printf("  %s %4d\n", pad(labels[number(doc.Lookup("_id"))], 18), number(doc.Lookup("orders")))
	}

	// ── 7. Самый дорогой заказ города ─────────────────────────────────────
	title("7. Самый дорогой заказ города")

	for _, doc := range run(orders, mongo.Pipeline{
		delivered,
		{{Key: "$sort", Value: bson.D{{Key: "total", Value: -1}}}},
		{{Key: "$group", Value: bson.D{
			{Key: "_id", Value: "$delivery.city"},
			{Key: "number", Value: bson.D{{Key: "$first", Value: "$number"}}},
			{Key: "total", Value: bson.D{{Key: "$first", Value: "$total"}}}}}},
		{{Key: "$sort", Value: bson.D{{Key: "total", Value: -1}}}},
	}) {
		fmt.Printf("  %s %s  %8d\n", pad(doc.Lookup("_id").StringValue(), 16),
			doc.Lookup("number").StringValue(), number(doc.Lookup("total")))
	}

	// ── 8. Оплата и доставка ──────────────────────────────────────────────
	title("8. Оплата и доставка")

	facets := run(orders, mongo.Pipeline{
		delivered,
		{{Key: "$facet", Value: bson.D{
			{Key: "payment", Value: bson.A{bson.D{{Key: "$sortByCount", Value: "$payment.method"}}}},
			{Key: "delivery", Value: bson.A{bson.D{{Key: "$sortByCount", Value: "$delivery.type"}}}},
			{Key: "promo", Value: bson.A{
				bson.D{{Key: "$match", Value: bson.D{{Key: "promo", Value: bson.D{{Key: "$exists", Value: true}}}}}},
				bson.D{{Key: "$count", Value: "orders"}}}}}}},
	})[0]
	line := func(field string) string {
		values, _ := facets.Lookup(field).Array().Values()
		parts := []string{}
		for _, value := range values {
			group := value.Document()
			parts = append(parts, fmt.Sprintf("%s %d", group.Lookup("_id").StringValue(), number(group.Lookup("count"))))
		}
		return strings.Join(parts, ", ")
	}
	fmt.Println("  оплата:   " + line("payment"))
	fmt.Println("  доставка: " + line("delivery"))
	promo, _ := facets.Lookup("promo").Array().Values()
	withPromo := int64(0)
	if len(promo) > 0 {
		withPromo = number(promo[0].Document().Lookup("orders"))
	}
	fmt.Printf("  с кодом скидки: %d\n", withPromo)
}
