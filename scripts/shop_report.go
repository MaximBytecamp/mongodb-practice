// Отчёт по магазину: что умеет MongoDB из модуля 1.
//
// Скрипт проходит все операции модуля 1 на базе shop и в песочнице sandbox
// и печатает результат так, как его показывают в консоли: счётчики, таблицы,
// карточку документа, страницы и итоги записи.
//
//	cd scripts
//	go run shop_report.go
//
//	docker compose run --rm go scripts/shop_report.go    # в контейнере
package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"sort"
	"strings"
	"unicode/utf8"

	"go.mongodb.org/mongo-driver/v2/bson"
	"go.mongodb.org/mongo-driver/v2/mongo"
	"go.mongodb.org/mongo-driver/v2/mongo/options"
)

// Product — поля товара, которые читает отчёт.
type Product struct {
	ID       string `bson:"_id"`
	Title    string `bson:"title"`
	Category string `bson:"category"`
	Price    int    `bson:"price"`
}

// Order — заказ вместе с вложенными документами и массивом позиций.
type Order struct {
	Number   string `bson:"number"`
	Status   string `bson:"status"`
	Total    int    `bson:"total"`
	Delivery struct {
		City string `bson:"city"`
		Type string `bson:"type"`
		Days int    `bson:"days"`
	} `bson:"delivery"`
	Payment struct {
		Method string `bson:"method"`
		Paid   bool   `bson:"paid"`
	} `bson:"payment"`
	Items []struct {
		Title string `bson:"title"`
		Qty   int    `bson:"qty"`
		Price int    `bson:"price"`
	} `bson:"items"`
}

// pad дополняет строку пробелами по числу символов: в кириллице байт больше,
// чем букв, и обычное %-32s сдвигает столбцы.
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

	shop := client.Database("shop")
	products := shop.Collection("products")
	orders := shop.Collection("orders")
	box := client.Database("sandbox").Collection("products")

	// ── 1. Что лежит на сервере ───────────────────────────────────────
	title("1. Связь и база shop")

	if err := client.Ping(ctx, nil); err != nil {
		log.Fatal(err)
	}
	bases, _ := client.ListDatabaseNames(ctx, bson.D{})
	sort.Strings(bases)
	fmt.Println("базы на сервере:", strings.Join(bases, ", "))
	names, _ := shop.ListCollectionNames(ctx, bson.D{})
	sort.Strings(names)
	fmt.Println("коллекции shop:", strings.Join(names, ", "))
	vsegoTovarov, _ := products.CountDocuments(ctx, bson.D{})
	vsegoZakazov, _ := orders.CountDocuments(ctx, bson.D{})
	vsegoKlientov, _ := shop.Collection("customers").CountDocuments(ctx, bson.D{})
	fmt.Printf("товаров %d, заказов %d, покупателей %d\n", vsegoTovarov, vsegoZakazov, vsegoKlientov)

	// ── 2. Карточка одного документа ──────────────────────────────────
	title("2. Один товар целиком")

	var tovar bson.D
	products.FindOne(ctx, bson.D{{Key: "_id", Value: "p-001"}}).Decode(&tovar)
	for _, field := range tovar {
		fmt.Printf("  %s %v\n", pad(field.Key, 10), field.Value)
	}

	// ── 3. Таблица: пять самых дорогих товаров ────────────────────────
	title("3. Пять самых дорогих товаров")

	fmt.Printf("  %s %s %s\n", pad("название", 32), pad("категория", 14), right("цена", 8))
	cursor, _ := products.Find(ctx, bson.D{},
		options.Find().
			SetProjection(bson.D{{Key: "_id", Value: 0}, {Key: "title", Value: 1},
				{Key: "category", Value: 1}, {Key: "price", Value: 1}}).
			SetSort(bson.D{{Key: "price", Value: -1}}).
			SetLimit(5))
	var dorogie []Product
	cursor.All(ctx, &dorogie)
	for _, p := range dorogie {
		fmt.Printf("  %s %s %s\n", pad(p.Title, 32), pad(p.Category, 14), right(fmt.Sprint(p.Price), 8))
	}

	// ── 4. Сортировка по двум полям ───────────────────────────────────
	title("4. Каталог: категории по алфавиту, внутри — от дорогих к дешёвым")

	cursor, _ = products.Find(ctx, bson.D{},
		options.Find().
			SetProjection(bson.D{{Key: "_id", Value: 0}, {Key: "category", Value: 1},
				{Key: "title", Value: 1}, {Key: "price", Value: 1}}).
			SetSort(bson.D{{Key: "category", Value: 1}, {Key: "price", Value: -1}}).
			SetLimit(8))
	var katalog []Product
	cursor.All(ctx, &katalog)
	for _, p := range katalog {
		fmt.Printf("  %s %s %s\n", pad(p.Category, 14), pad(p.Title, 32), right(fmt.Sprint(p.Price), 8))
	}

	// ── 5. Счётчики по категориям ─────────────────────────────────────
	title("5. Товары по категориям")

	var spisok []string
	products.Distinct(ctx, "category", bson.D{}).Decode(&spisok)
	sort.Strings(spisok)
	for _, category := range spisok {
		skolko, _ := products.CountDocuments(ctx, bson.D{{Key: "category", Value: category}})
		fmt.Printf("  %s %s шт\n", pad(category, 14), right(fmt.Sprint(skolko), 2))
	}

	// ── 6. Страницы заказов ───────────────────────────────────────────
	title("6. Заказы страницами по 5")

	for page := int64(1); page <= 2; page++ {
		fmt.Printf("  страница %d\n", page)
		cursor, _ = orders.Find(ctx, bson.D{{Key: "status", Value: "доставлен"}},
			options.Find().
				SetProjection(bson.D{{Key: "_id", Value: 0}, {Key: "number", Value: 1}, {Key: "total", Value: 1}}).
				SetSort(bson.D{{Key: "number", Value: 1}}).
				SetSkip((page - 1) * 5).
				SetLimit(5))
		var stranica []Order
		cursor.All(ctx, &stranica)
		for _, o := range stranica {
			fmt.Printf("    %s  %s\n", o.Number, right(fmt.Sprint(o.Total), 7))
		}
	}

	// ── 7. Вложенные поля и массивы в выводе ──────────────────────────
	title("7. Заказ с доставкой и позициями")

	var zakaz Order
	orders.FindOne(ctx, bson.D{{Key: "_id", Value: "o-0001"}}).Decode(&zakaz)
	fmt.Printf("  номер %s, статус %s, сумма %d\n", zakaz.Number, zakaz.Status, zakaz.Total)
	fmt.Printf("  доставка: %s, %s, %d дн.\n", zakaz.Delivery.City, zakaz.Delivery.Type, zakaz.Delivery.Days)
	fmt.Printf("  оплата: %s, проведена: %t\n", zakaz.Payment.Method, zakaz.Payment.Paid)
	for _, position := range zakaz.Items {
		fmt.Printf("    %d × %s %s\n", position.Qty, pad(position.Title, 34), right(fmt.Sprint(position.Price), 7))
	}

	// ── 8. Запись: вставка, обновление, удаление ──────────────────────
	title("8. Песочница: вставка, обновление, удаление")

	for _, key := range []string{"p-301", "p-302"} {
		box.DeleteOne(ctx, bson.D{{Key: "_id", Value: key}})
	}

	added, _ := box.InsertMany(ctx, []any{
		bson.D{{Key: "_id", Value: "p-301"}, {Key: "title", Value: "Док-станция USB-C"},
			{Key: "category", Value: "аксессуары"}, {Key: "price", Value: 6490}, {Key: "reviews", Value: 0}},
		bson.D{{Key: "_id", Value: "p-302"}, {Key: "title", Value: "Кабель HDMI 2 м"},
			{Key: "category", Value: "аксессуары"}, {Key: "price", Value: 890}, {Key: "reviews", Value: 0}},
	})
	kluchi := make([]string, 0, len(added.InsertedIDs))
	for _, id := range added.InsertedIDs {
		kluchi = append(kluchi, fmt.Sprint(id))
	}
	fmt.Printf("  вставлено: %d ключи: %s\n", len(added.InsertedIDs), strings.Join(kluchi, ", "))

	changed, _ := box.UpdateOne(ctx, bson.D{{Key: "_id", Value: "p-301"}},
		bson.D{{Key: "$set", Value: bson.D{{Key: "price", Value: 5990}}},
			{Key: "$inc", Value: bson.D{{Key: "reviews", Value: 1}}}})
	fmt.Printf("  обновление одного: найдено %d, изменено %d\n", changed.MatchedCount, changed.ModifiedCount)

	many, _ := box.UpdateMany(ctx, bson.D{{Key: "category", Value: "аксессуары"}},
		bson.D{{Key: "$set", Value: bson.D{{Key: "sale", Value: true}}}})
	fmt.Printf("  пометка распродажи: найдено %d, изменено %d\n", many.MatchedCount, many.ModifiedCount)

	var after bson.D
	box.FindOne(ctx, bson.D{{Key: "_id", Value: "p-301"}},
		options.FindOne().SetProjection(bson.D{{Key: "_id", Value: 0}, {Key: "title", Value: 1},
			{Key: "price", Value: 1}, {Key: "reviews", Value: 1}, {Key: "sale", Value: 1}})).Decode(&after)
	out, _ := bson.MarshalExtJSON(after, false, false)
	fmt.Println("  что стало:", string(out))

	gone, _ := box.DeleteOne(ctx, bson.D{{Key: "_id", Value: "p-302"}})
	vPesochnice, _ := box.CountDocuments(ctx, bson.D{})
	fmt.Printf("  удалено: %d · товаров в песочнице: %d\n", gone.DeletedCount, vPesochnice)

	// ── 9. Итог ───────────────────────────────────────────────────────
	title("9. Итог")

	fmt.Printf("  прочитано из shop: %d товаров и %d заказов\n", vsegoTovarov, vsegoZakazov)
	fmt.Println("  изменено в песочнице: 1 вставка пачкой, 2 обновления, 1 удаление")
}
