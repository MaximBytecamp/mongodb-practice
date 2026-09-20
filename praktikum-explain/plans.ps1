# Снимает все шесть планов выполнения для практикума «Онлайн-разбор запроса».
#
#   powershell -ExecutionPolicy Bypass -File plans.ps1
#   powershell -ExecutionPolicy Bypass -File plans.ps1 etalon
#
# Скрипт создаёт индексы, снимает план и удаляет их за собой: коллекция
# остаётся в том же виде, в каком была до запуска. Данные не меняются.

param([string]$Out = ".")

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$compose = Join-Path (Split-Path -Parent $here) "compose.yaml"

# Где выполнять команды: локальный mongosh или контейнер стенда.
if (Get-Command mongosh -ErrorAction SilentlyContinue) {
  $uri = if ($env:MONGO_URI) { $env:MONGO_URI } else { "mongodb://localhost:27017" }
  function Invoke-Mongo($script) { mongosh "$uri/logs" --quiet --eval $script }
} else {
  $id = docker compose -f $compose ps -q mongo 2>$null
  if (-not $id) {
    Write-Error "Нужен mongosh или запущенный стенд: docker compose up -d"
  }
  function Invoke-Mongo($script) { docker compose -f $compose exec -T mongo mongosh logs --quiet --eval $script }
}

# Запрос 1: медленные ошибки платежей, свежие сверху.
$q1 = 'db.events.find({ service: "payments", level: "error", duration_ms: { $gt: 500 } }).sort({ ts: -1 })'
# Запрос 2: десять самых долгих ошибок.
$q2 = 'db.events.find({ level: "error" }).sort({ duration_ms: -1 }).limit(10)'

function Save-Plan($file, $query, $index) {
  $make = ""
  $drop = ""
  if ($index) {
    # Имя индексу не задаём: в плане должно стоять то же имя, которое получит
    # студент после обычного createIndex. Удаляем по описанию ключей.
    $make = "db.events.createIndex($index);"
    $drop = "db.events.dropIndex($index);"
  }
  # Весь запуск обёрнут в функцию: mongosh печатает только строку плана.
  $script = "(() => { $make const plan = EJSON.stringify($query.explain('executionStats')); $drop return plan; })()"
  $plan = Invoke-Mongo $script
  Set-Content -Path (Join-Path $Out $file) -Value $plan -Encoding utf8

  $stats = ($plan | ConvertFrom-Json).executionStats
  $stages = @()
  $stage = $stats.executionStages
  while ($stage) { $stages += $stage.stage; $stage = $stage.inputStage }
  "{0,-22} возвращено {1}, документов {2}, ключей {3} — {4}" -f `
    $file, $stats.nReturned, $stats.totalDocsExamined, $stats.totalKeysExamined, ($stages -join " <- ")
}

"Запрос 1 — ошибки payments дольше 500 мс, сортировка по времени"
Save-Plan "plan-1.json"     $q1 $null
Save-Plan "plan-2.json"     $q1 '{ service: 1, level: 1, ts: -1, duration_ms: 1 }'
Save-Plan "plan-3a.json"    $q1 '{ duration_ms: 1, service: 1, level: 1, ts: -1 }'
Save-Plan "plan-3b.json"    $q1 '{ service: 1, level: 1, duration_ms: 1, ts: -1 }'

""
"Запрос 2 — десять самых долгих ошибок"
Save-Plan "plan-4-bez.json" $q2 $null
Save-Plan "plan-4.json"     $q2 '{ level: 1, duration_ms: -1 }'

""
"Индексы коллекции после работы скрипта:"
Invoke-Mongo 'db.events.getIndexes().map(i => i.name).join(", ")'
