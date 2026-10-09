// Глава 3.3 · Как читать explain: COLLSCAN против IXSCAN — заготовка для примеров.
//
// Перед главой верните базы в исходное состояние (глава 1.0, §8).
// Пример из главы вставьте в фигурные скобки в конце main вместо проверочных строк
// и запустите:
//
//	cd lessons
//	go run ch3_3.go
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
	"strings"

	"go.mongodb.org/mongo-driver/v2/bson"
	"go.mongodb.org/mongo-driver/v2/mongo"
	"go.mongodb.org/mongo-driver/v2/mongo/options"
)

// Импорты нужны разным примерам главы. Строки ниже не дают Go ругаться
// на те, которыми текущий пример не пользуется.
var _ = fmt.Sprint
var _ = strings.Join
var _ = bson.D{}
var _ = mongo.ErrNoDocuments

// ── функции и типы из примеров главы ─────────────────────────────

// summary — план и счётчики explain одной строкой: стадии | вернул | ключей | документов.
func summary(coll *mongo.Collection, find bson.D) string {
	cmd := append(bson.D{{Key: "find", Value: coll.Name()}}, find...)
	var res bson.Raw
	coll.Database().RunCommand(context.Background(),
		bson.D{{Key: "explain", Value: cmd}, {Key: "verbosity", Value: "executionStats"}}).Decode(&res)
	stage := res.Lookup("queryPlanner", "winningPlan").Document()
	if inner, ok := stage.Lookup("queryPlan").DocumentOK(); ok { // MongoDB 7 кладёт план во вложенное поле
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
	stats := res.Lookup("executionStats").Document()
	return fmt.Sprintf("%s | вернул %d | ключей %d | документов %d", strings.Join(steps, " → "),
		stats.Lookup("nReturned").AsInt64(), stats.Lookup("totalKeysExamined").AsInt64(),
		stats.Lookup("totalDocsExamined").AsInt64())
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
	events := client.Database("logs").Collection("events")
	sandbox := client.Database("sandbox")
	box := sandbox.Collection("products") // песочница: здесь можно менять
	_, _, _, _, _, _ = db, products, orders, events, sandbox, box

	journal := sandbox.Collection("events") // копия журнала: на ней строятся индексы главы
	_ = journal

	{
		// ── пример из главы ──────────────────────────────────────
		// Проверочные строки: замените их примером из главы.
		if err := client.Ping(ctx, nil); err != nil {
			log.Fatal(err)
		}
		fmt.Println("Заготовка главы 3.3 подключилась к серверу. Замените проверочные строки примером из главы.")
	}
}
