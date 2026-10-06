// Глава 3.1 · Правила на уровне базы: $jsonSchema — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//	cd lessons
//	go run ch3_1.go
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
	events := client.Database("logs").Collection("events")
	sandbox := client.Database("sandbox")
	box := sandbox.Collection("products") // песочница: здесь можно менять
	_, _, _, _, _, _ = db, products, orders, events, sandbox, box

	catalog := sandbox.Collection("catalog") // коллекция без правила, §1

	// Правило товара (§2 главы): обязательные поля и типы значений.
	schema := bson.D{
		{Key: "bsonType", Value: "object"},
		{Key: "required", Value: bson.A{"sku", "title", "category", "price"}},
		{Key: "properties", Value: bson.D{
			{Key: "sku", Value: bson.D{{Key: "bsonType", Value: "string"}}},
			{Key: "title", Value: bson.D{{Key: "bsonType", Value: "string"}}},
			{Key: "category", Value: bson.D{{Key: "bsonType", Value: "string"}}},
			{Key: "price", Value: bson.D{{Key: "bsonType", Value: "number"}, {Key: "minimum", Value: 1}}},
		}},
	}
	_, _ = catalog, schema

	{
		// ── пример из главы ──────────────────────────────────────
		// Проверочные строки: замените их примером из главы.
		if err := client.Ping(ctx, nil); err != nil {
			log.Fatal(err)
		}
		fmt.Println("Заготовка главы 3.1 подключилась к серверу. Замените проверочные строки примером из главы.")
	}
}
