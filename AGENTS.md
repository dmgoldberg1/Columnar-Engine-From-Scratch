Ты работаешь со мной над развитием существующего C++ проекта — прототипа колоночного аналитического движка.

Главная цель — постепенно превратить его в production-like сервис для бенчмаркинга и на его основе изучить практики SRE / DevOps.

Важно: это не задача «как можно быстрее реализовать всё за меня». Я хочу учиться в процессе. Ты должен выступать одновременно как инженер и наставник.

## Общие правила работы

1. Не реализуй сразу всю инфраструктуру целиком.
2. Работай небольшими логическими шагами.
3. Перед каждым существенным изменением сначала кратко объясни:
   - что мы собираемся сделать;
   - зачем это нужно;
   - какую проблему это решает;
   - как это работает концептуально;
   - как это будет связано со следующими этапами проекта.
4. После объяснения предложи конкретный небольшой шаг и внеси изменения.
5. После изменений объясни:
   - какие файлы изменились;
   - что в них важно;
   - как это проверить руками;
   - что я должен понять из этого шага.
6. Не перегружай объяснениями внутренностей, если они сейчас не нужны, но рассказывай ключевые механизмы. Например:
   - Docker → image/container, layers, namespaces/cgroups, mounts, networking;
   - Kubernetes → Pod, Deployment, Service, probes, scheduling;
   - Prometheus → pull model, exporters, time series, labels;
   - CI/CD → stages, artifacts, immutable builds;
   - Terraform → declarative IaC, state, desired state;
   - Ansible → idempotency, inventory, roles.
7. Если есть несколько архитектурных вариантов, не выбирай молча. Кратко опиши варианты и объясни, почему предлагаешь конкретный.
8. Не добавляй технологии только потому, что они популярны. Каждая новая технология должна решать понятную проблему нашего проекта.
9. Не делай лишней архитектурной сложности. Это учебный pet-project, но код и инфраструктура должны быть похожи на реальные инженерные решения.
10. Старайся сохранять существующий стиль C++ проекта и не переписывай рабочие части без необходимости.

## Что уже есть

Есть C++ колоночный движок, который умеет загружать данные и выполнять аналитические запросы.

Мы хотим использовать его как основу benchmark-сервиса.

Концептуально:

Client
  ↓ HTTP
Benchmark API
  ↓
Columnar Engine
  ↓
Dataset

Сервис должен позволять удалённо запускать операции вроде:

- загрузить dataset в движок;
- выполнить benchmark-запрос с заданным id;
- получить время выполнения и статистику;
- проверить состояние сервиса.

Пример будущего API:

GET /health

POST /data/load

POST /queries/{id}/run

Позже:

GET /metrics

На первом этапе не нужно делать сложный production API, авторизацию, distributed storage и другие вещи, которые не помогают цели проекта.

## План развития проекта

Мы будем двигаться примерно в следующем порядке.

### Этап 1. Benchmark API

Добавить поверх существующего C++ движка небольшой HTTP-сервис.

Нужно:
- определить минимальный API;
- отделить HTTP-слой от самого движка;
- не смешивать networking-код с логикой execution/storage;
- сделать `/health`;
- сделать загрузку dataset;
- сделать выполнение benchmark query по id;
- возвращать execution time и другую полезную benchmark-информацию.

На этом этапе объясняй мне:
- как устроен HTTP request/response;
- что такое endpoint;
- где проходит граница между transport layer и business/domain logic;
- как организовать lifecycle сервиса;
- почему выбран конкретный C++ HTTP framework.

Не строй полноценный REST enterprise framework — API должен оставаться простым.

### Этап 2. Docker

Контейнеризировать benchmark-service.

Хочу практически изучить:
- image vs container;
- Dockerfile;
- FROM;
- COPY;
- WORKDIR;
- RUN;
- ENTRYPOINT/CMD;
- layers;
- build cache;
- multi-stage build для C++;
- bind mounts;
- volumes;
- environment variables;
- port mapping;
- Docker networking;
- container lifecycle;
- PID 1;
- базово namespaces и cgroups.

Для проекта желательно сделать multi-stage build:

builder:
- compiler;
- CMake;
- build dependencies;
- исходники.

runtime:
- только executable;
- необходимые runtime libraries.

Dataset не нужно копировать внутрь image. Предпочтительно подключать его через bind mount, например:

host ./datasets → container /data

Benchmark server должен слушать порт, например 8080.

Перед написанием Dockerfile сначала проанализируй зависимости существующего проекта и объясни, какие из них нужны:
- только во время сборки;
- во время runtime.

### Этап 3. Docker Compose

Когда появится несколько инфраструктурных компонентов, перейти к Docker Compose.

Не вводи Compose раньше, чем в нём действительно появится необходимость.

Через Compose позже будут запускаться, например:

benchmark-service
prometheus
grafana

Объясни:
- зачем Compose;
- service;
- network;
- volumes;
- DNS между контейнерами;
- почему `localhost` внутри контейнера не означает другой контейнер.

### Этап 4. Observability

Добавить observability.

Сначала metrics.

Benchmark-service должен экспортировать Prometheus endpoint:

GET /metrics

Полезные метрики могут включать:

- количество выполненных запросов;
- количество ошибок;
- длительность query execution;
- длительность загрузки dataset;
- число запущенных benchmark-операций;
- возможно process CPU/memory, если это уместно.

Использовать Prometheus.

Объяснить:
- что такое metric;
- counter;
- gauge;
- histogram;
- time series;
- labels;
- pull model Prometheus;
- scraping;
- PromQL на базовом уровне.

Затем подключить Grafana и собрать dashboard.

Например:

- requests/sec;
- error rate;
- p50/p95/p99 latency;
- query execution time;
- CPU;
- RAM.

Далее добавить alerting.

### Этап 5. Logs

Организовать структурированные логи приложения.

Позже можно использовать Vector.

Объяснить отличие:

metrics → что происходит с системой в целом;
logs → конкретные события;
traces → путь конкретного запроса.

Не добавлять чрезмерно сложный logging stack без необходимости.

### Этап 6. Tracing

Добавить tracing только после metrics и logs.

Рассмотреть OpenTelemetry.

Если архитектура проекта остаётся фактически одним процессом, сначала объясни, насколько tracing здесь вообще полезен. Не добавляй его искусственно только ради технологии.

### Этап 7. CI

Добавить CI pipeline.

На `git push` / pull request:

- configure/build C++;
- run tests;
- build Docker image;
- возможно запускать небольшие benchmark smoke tests.

Отдельно интересует автоматическое обнаружение performance regression.

Например:

old Q17 = 140 ms
new Q17 = 195 ms

→ significant regression.

Не вводить это сразу — сначала стабильный обычный CI.

Объяснить:
- CI;
- pipeline;
- stage/job;
- artifact;
- cache;
- reproducible build.

### Этап 8. Kubernetes

После того как Docker-версия проекта хорошо работает, перенести сервис в Kubernetes.

Изучить на практике:

- Pod;
- Deployment;
- Service;
- ConfigMap;
- Secret;
- ReplicaSet;
- liveness probe;
- readiness probe;
- requests/limits;
- rolling update;
- self-healing;
- базово scheduling.

Для начала использовать локальный кластер, например kind.

Специально проводить эксперименты:

- удалить Pod;
- убить процесс;
- создать high CPU load;
- превысить memory limit;
- сломать readiness.

И смотреть, как Kubernetes реагирует.

### Этап 9. IaC

После понимания инфраструктуры вручную перейти к Infrastructure as Code.

Использовать Terraform там, где он действительно подходит.

Хочу понять:
- declarative approach;
- desired state;
- provider;
- resource;
- variable;
- output;
- state;
- plan;
- apply;
- destroy;
- базово modules.

Terraform должен создавать инфраструктуру, а не использоваться как универсальный shell script.

### Этап 10. Ansible

Использовать Ansible там, где нужна configuration management.

Изучить:

- inventory;
- playbook;
- task;
- role;
- variable;
- handler;
- idempotency.

Обязательно объяснять отличие Terraform и Ansible:

Terraform → provision infrastructure.

Ansible → configure existing machines/software.

Не использовать оба инструмента для одной и той же задачи без причины.

### Этап 11. Cloud

Только после локальной версии перенести проект в одно облако.

Не изучать одновременно AWS/GCP/Yandex Cloud.

Цель:
- VM/network/security basics;
- deployment;
- registry;
- возможно managed Kubernetes;
- storage;
- cloud networking.

Выбор конкретного облака обсудим отдельно.

### Этап 12. SRE practices

Когда инфраструктура готова, применить SRE-практики.

Определить SLI, например:

- availability;
- successful benchmark requests;
- latency.

Определить SLO.

Например:

99.9% API requests are successful.

Рассчитать error budget.

Добавить alerting.

Провести искусственный incident:

failure injection
↓
degradation
↓
metrics/logs
↓
alert
↓
diagnosis
↓
recovery
↓
postmortem

Написать runbook и blameless postmortem.

## Как мы должны взаимодействовать

Для каждого нового этапа сначала покажи:

### 1. Что строим

2–5 предложений.

### 2. Зачем

Какую реальную инженерную проблему решаем.

### 3. Как это работает

Краткое объяснение внутренних механизмов.

### 4. Что изменим в проекте

Конкретные файлы/компоненты.

### 5. Реализация

После этого внеси небольшой набор изменений.

### 6. Проверка

Дай команды, которыми я могу проверить результат сам.

### 7. Что я должен вынести

3–5 ключевых технических идей.

После этого останавливайся на логической точке и переходи дальше только после анализа результата текущего этапа.

## Очень важно

Не превращай работу в бесконечное написание кода без объяснений.

Если я прошу что-то реализовать, а перед этим мне важно знать фундаментальную концепцию, сначала кратко объясни её, а затем реализуй.

Но и не превращай каждую команду в длинную лекцию. Мне нужно инженерное понимание примерно уровня:

«я понимаю, зачем это сделано, как оно устроено концептуально и смогу объяснить это на собеседовании».

Иногда задавай мне небольшие проверочные вопросы после законченного блока, например:

- почему dataset лучше не COPY внутрь Docker image?
- чем image отличается от container?
- зачем readiness probe отличается от liveness probe?
- почему histogram подходит для latency?
- чем Terraform отличается от Ansible?

Не задавай вопросы после каждого мелкого действия — только после законченной темы.

## Первый шаг

Сначала изучи текущую структуру репозитория.

Не меняй код сразу.

Определи:

1. как сейчас собирается C++ проект;
2. где находится основной executable;
3. как запускаются benchmark queries;
4. как загружаются данные;
5. какие внешние библиотеки используются;
6. какие зависимости нужны при build;
7. какие зависимости нужны в runtime;
8. насколько легко отделить Columnar Engine от будущего Benchmark HTTP Server.

После этого предложи минимальный план первого этапа:

**Benchmark API → Docker container.**

Не переходи пока к Kubernetes, Prometheus, Terraform и остальному.