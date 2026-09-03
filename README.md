# Columnar Engine From Scratch

Учебный колоночный аналитический движок, реализованный с нуля на C++. Поверх
движка работает небольшой HTTP-сервис: он загружает датасет ClickBench,
запускает реализованные benchmark-запросы и возвращает результат вместе со
временем выполнения.

Проект развивается как production-like pet-проект для практического изучения
C++, Docker и SRE/DevOps-подходов. Текущий этап включает Benchmark API, его
запуск в Docker и базовый endpoint с метриками в формате Prometheus. Сбор и
визуализация метрик будут добавляться следующими этапами.

## Возможности

- собственный бинарный колоночный формат хранения данных;
- батчевая обработка и исполнение запросов по pull-модели;
- чтение только необходимых колонок (projection pushdown);
- пропуск батчей по статистикам `min/max` (block skipping);
- фильтрация, агрегации, `GROUP BY`, `ORDER BY` и `ORDER BY ... LIMIT`;
- специализированные алгоритмы кодирования и сжатия: Bit Packing, Delta
  Encoding, Dictionary Encoding и Delta-Length Byte Array Encoding;
- HTTP API для проверки состояния, загрузки датасета и запуска запроса;
- endpoint `GET /metrics` с текущим состоянием сервиса;
- многоэтапная сборка Docker-образа;
- подключение датасета с хоста через bind mount;
- корректная остановка сервиса по `SIGINT` и `SIGTERM`;
- Docker healthcheck и настраиваемые ограничения CPU и памяти.

Сейчас через HTTP API реализован ClickBench Query 1 — подсчёт строк по колонке
`WatchID`.

## Архитектура

```text
HTTP client (curl)
        |
        | HTTP request / response
        v
benchmark-service
  Transport layer: BenchmarkHttpServer
        |--------------------> Prometheus metrics
        |
        | вызов C++ методов
        v
  Application layer: BenchmarkService
        |
        v
  Columnar Engine
  Scan -> Filter/Aggregation -> result
        |
        v
  Dataset: active.egg
```

Границы компонентов:

- `src/server` — транспортный слой: принимает HTTP-запросы, проверяет их и
  формирует HTTP-ответы;
- `src/benchmark` — прикладная логика: загружает датасет, выбирает benchmark по
  идентификатору и измеряет время выполнения;
- остальные компоненты в `src` образуют колоночный движок: типы, формат файла,
  чтение, запись и операторы исполнения запросов.

CMake отражает это разделение четырьмя нашими целями:

```text
benchmark-service (executable)
        |------------------------------|
        v                              v
benchmark_application          benchmark_metrics
        |                              |
        v                              v
columnar_engine                 prometheus-cpp::core
   (static library)                (static library)
```

`executable` — запускаемый файл. `Static library` — библиотека, код которой
добавляется в запускаемый файл во время линковки.

### Как датасет попадает в контейнер

Датасет намеренно не копируется в Docker-образ:

```text
host: ./datasets/hits.csv
          |
          | bind mount
          v
container: /data/hits.csv
          |
          | POST /data/load {"source":"hits.csv"}
          v
container: /data/active.egg
```

Bind mount — подключение существующего каталога хоста внутрь контейнера. В
данном случае `./datasets` на хосте и `/data` в контейнере показывают на одни и
те же файлы. Поэтому созданный `active.egg` остаётся на хосте после удаления
контейнера.

Поле `source` содержит относительный путь внутри `/data`. Сам CSV-файл через
HTTP не передаётся: он должен заранее находиться на машине, где запущен
контейнер. При загрузке сервис сначала создаёт и проверяет временный колоночный
файл, а затем атомарно заменяет им `active.egg`. Ошибка загрузки не повреждает
ранее активированный датасет.

## Требования

Для запуска в контейнере достаточно:

- Git;
- Docker Engine.

Компилятор, CMake и Ninja устанавливаются только на стадии сборки Docker-образа
и не попадают в итоговый runtime-образ. `Runtime` означает среду, в которой уже
собранная программа выполняется.

Для сборки без Docker нужны:

- CMake 3.22 или новее;
- компилятор с поддержкой C++23;
- Ninja;
- доступ в интернет при первой конфигурации CMake для получения
  `cpp-httplib`, `nlohmann/json`, `prometheus-cpp` и, при сборке тестов,
  GoogleTest. `prometheus-cpp` используется для хранения и сериализации
  метрик.

## Быстрый запуск в Docker

Все команды выполняются из корня репозитория.

### 1. Собрать image

```bash
docker build --tag columnar-benchmark-service:dev .
```

Image (образ) — неизменяемый шаблон с программой и её runtime-зависимостями.
Container (контейнер) — запущенный экземпляр этого образа.

### 2. Подготовить каталог с датасетом

В репозитории есть небольшой CSV для smoke-теста:

```bash
mkdir -p datasets
cp hits_sample.csv datasets/hits.csv
```

Smoke-тест — короткая проверка основных функций системы без полного набора
тестовых сценариев и большого датасета.

### 3. Запустить контейнер

```bash
./script/run_docker.sh
```

Скрипт по умолчанию:

- запускает image `columnar-benchmark-service:dev`;
- подключает `./datasets` к `/data` контейнера;
- перенаправляет `127.0.0.1:8080` хоста на порт `8080` контейнера;
- запускает контейнер от UID/GID текущего пользователя;
- работает на переднем плане и удаляет остановленный контейнер благодаря
  параметру `--rm`.

`127.0.0.1` — loopback-адрес, доступный только с той же машины. Нажатие
`Ctrl+C` отправляет сервису сигнал остановки и завершает контейнер.

Ограничения ресурсов можно передать через переменные окружения:

```bash
BENCHMARK_CPU_LIMIT=1.5 \
BENCHMARK_MEMORY_LIMIT=256m \
./script/run_docker.sh
```

Переменная окружения — именованное значение, передаваемое процессу при
запуске. Здесь `1.5` разрешает использовать вычислительное время, эквивалентное
полутора ядрам, а `256m` ограничивает память контейнера 256 MiB.

Другие параметры скрипта:

| Переменная | Значение по умолчанию | Назначение |
|---|---|---|
| `BENCHMARK_IMAGE` | `columnar-benchmark-service:dev` | имя Docker-образа |
| `BENCHMARK_CONTAINER_NAME` | `columnar-benchmark` | имя контейнера |
| `BENCHMARK_PUBLISH_ADDRESS` | `127.0.0.1` | интерфейс хоста для публикации порта |
| `BENCHMARK_HOST_PORT` | `8080` | порт на хосте |
| `BENCHMARK_DATASET_DIR` | `./datasets` | каталог датасета на хосте |
| `BENCHMARK_CPU_LIMIT` | без ограничения | ограничение CPU |
| `BENCHMARK_MEMORY_LIMIT` | без ограничения | ограничение памяти |

## Проверка API

Контейнер должен продолжать работать в первом терминале. Следующие команды
выполняются во втором терминале.

### Проверить состояние сервиса

```bash
curl --fail --silent --show-error http://127.0.0.1:8080/health
```

До загрузки датасета ответ выглядит так:

```json
{"dataset_loaded":false,"status":"ok"}
```

### Загрузить датасет

```bash
curl --fail --silent --show-error \
  --request POST \
  --header 'Content-Type: application/json' \
  --data '{"source":"hits.csv"}' \
  http://127.0.0.1:8080/data/load
```

Сервис прочитает `/data/hits.csv`, преобразует его в собственный колоночный
формат и сохранит `/data/active.egg`. Ответ содержит длительность загрузки и
размеры исходного и колоночного файлов.

### Выполнить ClickBench Query 1

```bash
curl --fail --silent --show-error \
  --request POST \
  http://127.0.0.1:8080/queries/1/run
```

Пример структуры ответа:

```json
{
  "execution_time_ms": 0.42,
  "query_id": 1,
  "result": {
    "columns": ["count"],
    "rows": [["1000"]]
  }
}
```

Конкретное время зависит от машины и текущей нагрузки.

### Посмотреть Four Golden Signals

```bash
curl --fail --silent --show-error http://127.0.0.1:8080/metrics
```

Four Golden Signals («четыре золотых сигнала») — четыре категории, по которым
оценивается состояние сервиса:

| Сигнал | Метрика | Смысл |
|---|---|---|
| Latency | `columnar_benchmark_http_request_duration_seconds` | распределение времени обработки запросов |
| Traffic | `columnar_benchmark_http_requests_total` | накопительное количество запросов |
| Errors | метка `outcome` у `http_requests_total` | успешные запросы, клиентские и серверные ошибки |
| Saturation | `http_requests_in_progress` и `http_worker_limit` | занятые обработчики относительно их предела |

Сокращённый пример ответа:

```text
# TYPE columnar_benchmark_http_requests_total counter
columnar_benchmark_http_requests_total{outcome="success",route="/health"} 1
columnar_benchmark_http_requests_total{outcome="client_error",route="/queries/:id/run"} 1

# TYPE columnar_benchmark_http_requests_in_progress gauge
columnar_benchmark_http_requests_in_progress 0

# TYPE columnar_benchmark_http_worker_limit gauge
columnar_benchmark_http_worker_limit 60

# TYPE columnar_benchmark_http_request_duration_seconds histogram
columnar_benchmark_http_request_duration_seconds_bucket{route="/health",le="0.001"} 1
columnar_benchmark_http_request_duration_seconds_count{route="/health"} 1
columnar_benchmark_http_request_duration_seconds_sum{route="/health"} 0.000056
```

Значения `outcome` ограничены тремя вариантами: `success`, `client_error` и
`server_error`. Путь запроса также нормализуется до шаблона, например
`/queries/:id/run`; неизвестные пути получают значение `unmatched`. Благодаря
этому произвольные URL и query ID не создают неограниченное количество временных
рядов.

На текущем этапе `outcome` определяется по HTTP-коду. Метки разделены, поэтому
будущий SLO сможет учитывать серверные `5xx` отдельно от ошибочных клиентских
`4xx`. Формально успешный `2xx` с неверным результатом можно обнаружить только
отдельной проверкой корректности, а слишком медленный запрос — сравнением
histogram с порогом latency SLO; эти правила появятся после определения SLO.

Histogram состоит из накопительных buckets («корзин»), общего количества
измерений `_count` и их суммы `_sum`. Позже Prometheus рассчитает по buckets
p50, p95 и p99. Метрики самого `GET /metrics` в HTTP-статистику не входят,
поэтому опросы Prometheus не будут искусственно увеличивать Traffic.

Текущая Saturation относится к пулу HTTP-обработчиков. CPU, RAM и disk I/O
контейнера будут собираться отдельно из Linux cgroups; приложение не пытается
самостоятельно подменить инфраструктурные измерения.

## Prometheus и Docker Compose

`GET /metrics` показывает текущие значения, но benchmark-сервис сам не хранит
их историю. Prometheus каждые 5 секунд опрашивает endpoint и сохраняет полученные
значения с отметкой времени во встроенной базе временных рядов.

Docker Compose читает [`compose.yaml`](compose.yaml) и совместно запускает два
сервиса:

- `benchmark-service` собирается из текущего `Dockerfile`;
- `prometheus` запускается из готового image `prom/prometheus:v3.13.1`.

Compose создаёт для них общую Docker-сеть и внутренний DNS. Поэтому Prometheus
обращается к приложению по адресу `benchmark-service:8080`. Адрес
`localhost:8080` внутри контейнера Prometheus указывал бы на сам контейнер
Prometheus, а не на benchmark-сервис.

Настройки сбора находятся в
[`monitoring/prometheus/prometheus.yml`](monitoring/prometheus/prometheus.yml):

- `scrape_interval: 5s` — собирать метрики каждые 5 секунд;
- `scrape_timeout: 2s` — считать попытку неуспешной, если ответ не пришёл за 2
  секунды;
- `job_name` — имя наблюдаемой группы целей;
- `targets` — адреса сервисов, с которых нужно собирать метрики.

История хранится в именованном Docker volume `prometheus-data`. Volume
сохраняется после обычного `docker compose down`; параметр `--volumes` удалит
его вместе с накопленной историей.

### Запуск стенда

Сначала подготовить тестовый датасет:

```bash
mkdir -p datasets
cp hits_sample.csv datasets/hits.csv
```

Затем собрать benchmark-сервис и запустить оба контейнера:

```bash
BENCHMARK_UID="$(id -u)" BENCHMARK_GID="$(id -g)" \
  docker compose up --build
```

UID и GID — числовые идентификаторы текущего Linux-пользователя и его группы.
Они нужны, чтобы benchmark-сервис мог создавать `active.egg` в подключённом
каталоге `./datasets` без файлов, принадлежащих `root`.

После запуска доступны:

- Benchmark API: `http://127.0.0.1:8080`;
- интерфейс Prometheus: `http://127.0.0.1:9090`;
- состояние сбора: `http://127.0.0.1:9090/targets`.

На странице Query в Prometheus можно выполнить PromQL-запрос. Например,
скорость HTTP-запросов за последнюю минуту:

```promql
sum(rate(columnar_benchmark_http_requests_total[1m]))
```

Остановить стенд, сохранив историю Prometheus:

```bash
docker compose down
```

Остановить стенд и удалить его volume с историей:

```bash
docker compose down --volumes
```

## HTTP API

| Метод и путь | Назначение | Успешный код |
|---|---|---|
| `GET /health` | проверить работу процесса и наличие активного датасета | `200` |
| `GET /metrics` | получить метрики в текстовом формате Prometheus | `200` |
| `POST /data/load` | преобразовать CSV из каталога данных в `active.egg` | `200` |
| `POST /queries/{id}/run` | выполнить benchmark-запрос по идентификатору | `200` |

API возвращает ошибки в едином JSON-формате:

```json
{
  "error": {
    "code": "dataset_not_loaded",
    "message": "Load a dataset before running queries."
  }
}
```

## Запуск без Docker

Собрать только HTTP-сервис в Release-конфигурации:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCOLUMNAR_ENABLE_NATIVE_OPTIMIZATIONS=OFF
cmake --build build --target benchmark-service --parallel
```

Запустить сервис:

```bash
mkdir -p datasets
cp hits_sample.csv datasets/hits.csv
BENCHMARK_DATA_DIR=./datasets ./build/benchmark-service
```

Параметры процесса:

| Переменная | Значение по умолчанию | Назначение |
|---|---|---|
| `BENCHMARK_HOST` | `0.0.0.0` | сетевые интерфейсы, на которых слушает сервис |
| `BENCHMARK_PORT` | `8080` | TCP-порт сервиса |
| `BENCHMARK_DATA_DIR` | `./data` | разрешённый каталог с датасетом |

`0.0.0.0` означает «слушать все сетевые интерфейсы текущего сетевого
пространства». В контейнере это нужно, чтобы перенаправление порта Docker могло
достичь процесса.

## Запуск на удалённой VM

VM (virtual machine, виртуальная машина) должна иметь установленный Git и
Docker. На VM:

```bash
git clone https://github.com/dmgoldberg1/Columnar-Engine-From-Scratch.git
cd Columnar-Engine-From-Scratch
docker build --tag columnar-benchmark-service:dev .
mkdir -p datasets
cp hits_sample.csv datasets/hits.csv
```

Для запуска контейнера в фоновом режиме:

```bash
DATASET_DIR="$(pwd)/datasets"
docker run --detach \
  --name columnar-benchmark \
  --user "$(id -u):$(id -g)" \
  --publish 127.0.0.1:8080:8080 \
  --mount "type=bind,source=$DATASET_DIR,target=/data" \
  --cpus 1.5 \
  --memory 256m \
  columnar-benchmark-service:dev
```

`--detach` оставляет контейнер работающим в фоне после завершения команды.
Порт публикуется только на loopback-интерфейсе VM и не открывается напрямую в
интернет.

Проверить контейнер с самой VM:

```bash
curl --fail --silent --show-error http://127.0.0.1:8080/health
docker logs columnar-benchmark
docker inspect --format '{{.State.Health.Status}}' columnar-benchmark
```

### Доступ к VM через SSH-туннель

На локальном компьютере задайте имя пользователя и публичный IP своей VM:

```bash
VM_USER="your-vm-user"
VM_PUBLIC_IP="your-vm-public-ip"
ssh -N -L 18080:127.0.0.1:8080 "${VM_USER}@${VM_PUBLIC_IP}"
```

SSH-туннель — зашифрованное перенаправление TCP-соединения. Пока команда
работает, запрос к локальному порту `18080` проходит через SSH на локальный порт
`8080` виртуальной машины:

```text
local computer: 127.0.0.1:18080
        |
        | encrypted SSH connection to VM port 22
        v
VM: 127.0.0.1:8080
        |
        | Docker port mapping
        v
container: 8080 -> benchmark-service
```

Во втором локальном терминале можно выполнить тот же smoke-тест:

```bash
curl --fail --silent --show-error http://127.0.0.1:18080/health

curl --fail --silent --show-error \
  --request POST \
  --header 'Content-Type: application/json' \
  --data '{"source":"hits.csv"}' \
  http://127.0.0.1:18080/data/load

curl --fail --silent --show-error \
  --request POST \
  http://127.0.0.1:18080/queries/1/run
```

Остановить и удалить контейнер на VM:

```bash
docker stop columnar-benchmark
docker rm columnar-benchmark
```

Каталог `datasets` и созданный `active.egg` при этом сохранятся на VM, потому
что это файлы хоста, подключённые через bind mount.

## Производительность

Движок тестировался на ClickBench и сравнивался с DuckDB. На селективных
запросах ускорение благодаря block skipping достигало 3.8 раза. Эти результаты
относятся к экспериментам с движком; HTTP API отделяет сетевую обработку от
измеряемого времени выполнения запроса.

## Технологии

- C++23 и STL;
- CMake и Ninja;
- cpp-httplib;
- nlohmann/json;
- prometheus-cpp;
- GoogleTest;
- Docker.

## Следующие этапы

После проверки накопления временных рядов в Prometheus следующим этапом станет
Grafana: она будет запрашивать данные из Prometheus и отображать их на постоянном
dashboard с графиками Four Golden Signals.
