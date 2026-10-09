// Глава 3.5 · География: 2dsphere, $near, $geoWithin, $geoIntersects — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//	cd lessons
//	go run ch3_5.go
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

	pickup := sandbox.Collection("pickup") // пункты выдачи: точки на карте
	zones := sandbox.Collection("zones")   // зоны доставки: многоугольники

	// Покупатель в центре Ярославля: долгота, затем широта.
	here := bson.D{{Key: "type", Value: "Point"}, {Key: "coordinates", Value: bson.A{39.8875, 57.6300}}}
	_, _, _ = pickup, zones, here

	{
		// ── пример из главы ──────────────────────────────────────
		// Проверочные строки: замените их примером из главы.
		if err := client.Ping(ctx, nil); err != nil {
			log.Fatal(err)
		}
		fmt.Println("Заготовка главы 3.5 подключилась к серверу. Замените проверочные строки примером из главы.")
	}
}
