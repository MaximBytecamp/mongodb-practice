// Проверка подключения: что лежит на сервере.
//
//	docker compose run --rm go hello/hello.go    // из контейнера
//	cd hello && go run hello.go                  // с компьютера, если стоит Go
package main

import (
	"context"
	"fmt"
	"log"
	"os"
	"sort"
	"time"

	"go.mongodb.org/mongo-driver/v2/bson"
	"go.mongodb.org/mongo-driver/v2/mongo"
	"go.mongodb.org/mongo-driver/v2/mongo/options"
)

func main() {
	// В контейнере сервер называется mongo, на компьютере — localhost
	uri := os.Getenv("MONGO_URI")
	if uri == "" {
		uri = "mongodb://localhost:27017"
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	client, err := mongo.Connect(options.Client().ApplyURI(uri).
		SetServerSelectionTimeout(5 * time.Second))
	if err != nil {
		log.Fatal(err)
	}
	defer client.Disconnect(ctx)

	var info bson.M
	if err := client.Database("admin").RunCommand(ctx, bson.D{{Key: "buildInfo", Value: 1}}).Decode(&info); err != nil {
		log.Fatal(err)
	}
	fmt.Println("Адрес:", uri)
	fmt.Println("Сервер:", info["version"])

	names, err := client.ListDatabaseNames(ctx, bson.D{})
	if err != nil {
		log.Fatal(err)
	}
	fmt.Println("Базы данных:", names)

	db := client.Database("shop")
	collections, _ := db.ListCollectionNames(ctx, bson.D{})
	sort.Strings(collections)
	fmt.Println("Коллекции shop:", collections)

	products, _ := db.Collection("products").CountDocuments(ctx, bson.D{})
	orders, _ := db.Collection("orders").CountDocuments(ctx, bson.D{})
	fmt.Println("Товаров:", products)
	fmt.Println("Заказов:", orders)
}
