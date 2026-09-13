// Глава 1.8 · Удаление — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте внутрь фигурных скобок в конце main и запустите:
//
//	cd lessons
//	go run ch1_8.go
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

	{
		// ── пример из главы ──────────────────────────────────────

	}
}
