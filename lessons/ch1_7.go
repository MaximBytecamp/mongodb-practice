// Глава 1.7 · Обновление: $set, $inc, $unset — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//	cd lessons
//	go run ch1_7.go
//
// Если пример начинается с func или type, его место — над main, у отметки.
// Следующий пример вставляйте вместо предыдущего: каждый пример запускается
// один раз и по порядку — именно так получены выводы в справочнике.
package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"time"

	"go.mongodb.org/mongo-driver/v2/bson"
	"go.mongodb.org/mongo-driver/v2/mongo"
	"go.mongodb.org/mongo-driver/v2/mongo/options"
)

// Импорты нужны разным примерам главы. Строки ниже не дают Go ругаться
// на те, которыми текущий пример не пользуется.
var _ = fmt.Sprint
var _ = time.Now
var _ = bson.D{}
var _ = mongo.ErrNoDocuments

// ── функции и типы из примеров главы ─────────────────────────────

// Product — поля товара, которые читают примеры. Тип объявлен в главе 1.4.
type Product struct {
	ID       string `bson:"_id"`
	SKU      string `bson:"sku"`
	Title    string `bson:"title"`
	Category string `bson:"category"`
	Price    int    `bson:"price"`
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

	db := client.Database("shop")
	products := db.Collection("products")
	orders := db.Collection("orders")
	box := client.Database("sandbox").Collection("products") // песочница: здесь можно менять
	_, _, _, _ = db, products, orders, box

	// Товары, которые добавлялись в песочницу в главе 1.3. Если песочницу
	// сбрасывали, заготовка добавит их снова — обновлять будет что.
	if n, _ := box.CountDocuments(ctx, bson.D{{Key: "_id", Value: "p-101"}}); n == 0 {
		box.InsertMany(ctx, []any{
			bson.D{{Key: "_id", Value: "p-101"}, {Key: "title", Value: "Подставка для ноутбука"}, {Key: "category", Value: "аксессуары"}, {Key: "price", Value: 2490}},
			bson.D{{Key: "_id", Value: "p-102"}, {Key: "title", Value: "Чехол для планшета"}, {Key: "category", Value: "аксессуары"}, {Key: "price", Value: 1890}},
			bson.D{{Key: "sku", Value: "SKU-AC-022"}, {Key: "title", Value: "Коврик для мыши XL"}, {Key: "brand", Value: "OEM"}, {Key: "category", Value: "аксессуары"}, {Key: "price", Value: 1290}},
		})
	}

	ordersBox := client.Database("sandbox").Collection("orders") // копия заказов: их можно менять
	if n, _ := ordersBox.CountDocuments(ctx, bson.D{}); n == 0 {
		var all []bson.D
		cursor, _ := orders.Find(ctx, bson.D{})
		cursor.All(ctx, &all)
		docs := make([]any, len(all))
		for i := range all {
			docs[i] = all[i]
		}
		ordersBox.InsertMany(ctx, docs)
	}

	stats := client.Database("sandbox").Collection("product_stats") // счётчики просмотров для §6
	stats.Drop(ctx)                                                 // каждый запуск заготовки считает с нуля
	today := time.Now().Format("2006-01-02")
	_, _, _ = ordersBox, stats, today

	{
		// ── пример из главы ──────────────────────────────────────
		// Проверочные строки: замените их примером из главы.
		if err := client.Ping(ctx, nil); err != nil {
			log.Fatal(err)
		}
		fmt.Println("Заготовка главы 1.7 подключилась к серверу. Замените проверочные строки примером из главы.")
	}
}
