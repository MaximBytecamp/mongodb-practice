# Заливка учебных данных в MongoDB — вариант для Windows PowerShell.
#
#   powershell -ExecutionPolicy Bypass -File stend\load.ps1
#   powershell -ExecutionPolicy Bypass -File stend\load.ps1 -Uri mongodb://localhost:27018
#
# Нужен mongoimport из MongoDB Command Line Database Tools.
# Скрипт полностью пересоздаёт учебные коллекции (--drop): повторный запуск
# безопасен и не двоит документы.

param(
  [string]$Uri = $(if ($env:MONGO_URI) { $env:MONGO_URI } else { "mongodb://localhost:27017" })
)

$ErrorActionPreference = "Stop"
$seed = Join-Path $PSScriptRoot "seed"

if (-not (Get-Command mongoimport -ErrorAction SilentlyContinue)) {
  Write-Host "mongoimport не найден. Установите MongoDB Command Line Database Tools"
  Write-Host "и добавьте папку bin в PATH — или загрузите базы через Docker: docker compose run --rm reset"
  exit 1
}

Get-ChildItem -Path $seed -Filter "*.json" | Sort-Object Name | ForEach-Object {
  $name = $_.BaseName                      # например shop.orders
  $db, $collection = $name.Split(".", 2)
  & mongoimport --uri $Uri --db $db --collection $collection --file $_.FullName --jsonArray --drop --quiet
  if ($LASTEXITCODE -ne 0) { throw "mongoimport не смог загрузить $($_.Name)" }
  "{0,-22} <- {1}" -f $name, $_.Name
}

""
"Готово. Проверка в Compass: база shop, коллекция orders — 120 документов."
