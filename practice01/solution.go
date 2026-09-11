// Практическая работа 01 — эталонное решение на Go.
//
// Те же две задачи, что и в solution.py:
//  1. Витрина каталога: страница списка товаров, как её отдаёт бэкенд.
//  2. Приёмка поставки: обновить остатки, завести новинки, снять с продажи.
//
// Запуск:
//
//	go run solution.go
//	MONGO_URI=mongodb://host:27017 go run solution.go
//
// Перед запуском данные должны быть залиты: bash ../stend/load.sh
package main

import (
	"context"
	"errors"
	"fmt"
	"log"
	"os"

	"go.mongodb.org/mongo-driver/v2/bson"
	"go.mongodb.org/mongo-driver/v2/mongo"
	"go.mongodb.org/mongo-driver/v2/mongo/options"
)

// Product — только те поля, которые нужны витрине.
// Остальное с сервера не запрашивается и в структуру не попадает.
type Product struct {
	SKU    string  `bson:"sku"`
	Title  string  `bson:"title"`
	Price  int32   `bson:"price"`
	Rating float64 `bson:"rating"`
}

var (
	ctx      = context.Background()
	products *mongo.Collection // только читаем
	box      *mongo.Collection // здесь меняем
)

// ── Задача 1. Витрина каталога ───────────────────────────────────

// catalogPage возвращает одну страницу каталога и общее число товаров
// под фильтром: без второго числа на странице не нарисовать «2 из 5».
func catalogPage(category string, page, perPage int64, sortField string, desc bool) ([]Product, int64, error) {
	query := bson.D{}
	if category != "" {
		query = append(query, bson.E{Key: "category", Value: category})
	}

	total, err := products.CountDocuments(ctx, query)
	if err != nil {
		return nil, 0, err
	}

	direction := 1
	if desc {
		direction = -1
	}

	cursor, err := products.Find(ctx, query, options.Find().
		SetProjection(bson.D{
			{Key: "_id", Value: 0}, {Key: "sku", Value: 1},
			{Key: "title", Value: 1}, {Key: "price", Value: 1}, {Key: "rating", Value: 1},
		}).
		// Второй ключ сортировки делает порядок однозначным: без него товары
		// с одинаковой ценой прыгают между страницами.
		SetSort(bson.D{{Key: sortField, Value: direction}, {Key: "sku", Value: 1}}).
		SetSkip((page - 1) * perPage).
		SetLimit(perPage))
	if err != nil {
		return nil, total, err
	}

	var items []Product
	return items, total, cursor.All(ctx, &items)
}

func showCatalog() {
	line := "=============================================================="
	fmt.Println(line)
	fmt.Println("ЗАДАЧА 1 · витрина каталога")
	fmt.Println(line)

	items, total, err := catalogPage("ноутбуки", 1, 3, "price", true)
	if err != nil {
		log.Fatal(err)
	}
	pages := (total + 2) / 3
	fmt.Printf("\nНоутбуки, страница 1 из %d (всего %d):\n", pages, total)
	for _, item := range items {
		fmt.Printf("  %7d ₽  %-32s рейтинг %g\n", item.Price, item.Title, item.Rating)
	}

	items, _, _ = catalogPage("ноутбуки", 2, 3, "price", true)
	fmt.Println("\nСтраница 2:")
	for _, item := range items {
		fmt.Printf("  %7d ₽  %-32s рейтинг %g\n", item.Price, item.Title, item.Rating)
	}

	items, total, _ = catalogPage("", 1, 5, "price", true)
	fmt.Printf("\nВесь каталог, топ-5 по цене (всего %d):\n", total)
	for _, item := range items {
		fmt.Printf("  %7d ₽  %s\n", item.Price, item.Title)
	}

	// Пустая страница — обычное дело: запросили дальше, чем есть данных.
	items, total, _ = catalogPage("ноутбуки", 9, 3, "price", true)
	fmt.Printf("\nСтраница 9 из существующих %d: найдено %d товаров, total по-прежнему %d\n",
		pages, len(items), total)
}

// ── Задача 2. Приёмка поставки ───────────────────────────────────

// arrived — что приехало: артикул → сколько штук.
var arrived = []struct {
	SKU string
	Qty int32
}{
	{"SKU-NB-001", 5},
	{"SKU-PH-006", 12},
	{"SKU-PR-010", 3},
}

// newItems — новые позиции каталога. Свой _id делает повторный запуск
// безопасным: вторая попытка упрётся в E11000, а не создаст дубли.
var newItems = []any{
	bson.D{{Key: "_id", Value: "p-101"}, {Key: "sku", Value: "SKU-AC-101"},
		{Key: "title", Value: "Подставка для ноутбука"}, {Key: "brand", Value: "OEM"},
		{Key: "category", Value: "аксессуары"}, {Key: "price", Value: 2490}, {Key: "reviews", Value: 0}},
	bson.D{{Key: "_id", Value: "p-102"}, {Key: "sku", Value: "SKU-AC-102"},
		{Key: "title", Value: "Чехол для планшета"}, {Key: "brand", Value: "OEM"},
		{Key: "category", Value: "аксессуары"}, {Key: "price", Value: 1890}, {Key: "reviews", Value: 0}},
}

var discontinued = []string{"SKU-CP-018"} // снято с продажи

func receiveSupply() {
	line := "=============================================================="
	fmt.Println("\n" + line)
	fmt.Println("ЗАДАЧА 2 · приёмка поставки")
	fmt.Println(line)

	report := [][2]any{}

	// 1. Остатки: $inc прибавляет на сервере, не читая значение на клиент.
	//    Если поля qty_total нет — оно появится с этим значением.
	var updated int64
	for _, item := range arrived {
		res, err := box.UpdateOne(ctx,
			bson.D{{Key: "sku", Value: item.SKU}},
			bson.D{{Key: "$inc", Value: bson.D{{Key: "qty_total", Value: item.Qty}}}})
		if err != nil {
			log.Fatal(err)
		}
		if res.MatchedCount == 0 {
			fmt.Printf("  ВНИМАНИЕ: артикул %s не найден в каталоге\n", item.SKU)
		}
		updated += res.ModifiedCount
	}
	report = append(report, [2]any{"остатки обновлены", updated})

	// 2. Новинки: ordered=false — одна уже существующая позиция
	//    не должна останавливать приёмку остальных.
	many, err := box.InsertMany(ctx, newItems, options.InsertMany().SetOrdered(false))
	switch {
	case err == nil:
		report = append(report, [2]any{"новых позиций", len(many.InsertedIDs)})
	default:
		var bulkErr mongo.BulkWriteException
		if !errors.As(err, &bulkErr) {
			log.Fatal(err)
		}
		already := len(bulkErr.WriteErrors)
		fmt.Printf("  %d позиций уже были в каталоге — пропущены\n", already)
		report = append(report, [2]any{"новых позиций", len(newItems) - already})
	}

	// 3. Снятие с продажи: сначала считаем, потом удаляем.
	query := bson.D{{Key: "sku", Value: bson.D{{Key: "$in", Value: discontinued}}}}
	toRemove, _ := box.CountDocuments(ctx, query)
	fmt.Printf("\n  под снятие с продажи попало: %d\n", toRemove)

	cursor, _ := box.Find(ctx, query, options.Find().
		SetProjection(bson.D{{Key: "_id", Value: 1}, {Key: "title", Value: 1}, {Key: "price", Value: 1}}))
	var doomed []bson.M
	cursor.All(ctx, &doomed)
	for _, doc := range doomed {
		fmt.Printf("    %v\n", doc)
	}

	deleted, _ := box.DeleteMany(ctx, query)
	report = append(report, [2]any{"снято с продажи", deleted.DeletedCount})

	// 4. Бесплатная доставка для всей категории — одним запросом.
	query = bson.D{{Key: "category", Value: "аксессуары"}}
	count, _ := box.CountDocuments(ctx, query)
	fmt.Printf("\n  бесплатная доставка коснётся: %d товаров\n", count)

	shipped, _ := box.UpdateMany(ctx, query,
		bson.D{{Key: "$set", Value: bson.D{{Key: "free_shipping", Value: true}}}})
	report = append(report, [2]any{"бесплатная доставка", shipped.ModifiedCount})

	total, _ := box.CountDocuments(ctx, bson.D{})
	report = append(report, [2]any{"итого в каталоге", total})

	fmt.Println("\nОтчёт о приёмке:")
	for _, row := range report {
		fmt.Printf("  %-24s %v\n", row[0], row[1])
	}
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
	defer client.Disconnect(ctx)

	products = client.Database("shop").Collection("products")
	box = client.Database("sandbox").Collection("products")

	showCatalog()
	receiveSupply()
}
