// Глава 1.4 · Чтение: find, find_one и курсор — заготовка для примеров.
//
// Глава только читает данные — сбрасывать базы не нужно.
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//	cd lessons
//	go run ch1_4.go
//
// Если пример начинается с func или type, его место — над main, у отметки.
// Следующий пример вставляйте вместо предыдущего: каждый пример запускается
// один раз и по порядку — именно так получены выводы в справочнике.
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

// Импорты нужны разным примерам главы. Строки ниже не дают Go ругаться
// на те, которыми текущий пример не пользуется.
var _ = errors.New
var _ = fmt.Sprint
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

	{
		// ── пример из главы ──────────────────────────────────────
		// Проверочные строки: замените их примером из главы.
		if err := client.Ping(ctx, nil); err != nil {
			log.Fatal(err)
		}
		fmt.Println("Заготовка главы 1.4 подключилась к серверу. Замените проверочные строки примером из главы.")
	}
}
