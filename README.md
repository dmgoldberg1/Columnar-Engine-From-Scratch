# Columnar Engine From Scratch

Колоночный аналитический движок на C++23, реализованный с нуля. На движке
реализован набор аналитических запросов
[ClickBench](https://github.com/ClickHouse/ClickBench).

Дополнительно движок обёрнут в benchmark-сервис с HTTP API для удалённого
выполнения запросов и мониторингом через Prometheus и Grafana. Проект можно
собрать локально, запустить в Docker или развернуть на виртуальной машине в
Yandex Cloud.

## Что реализовано

- собственный колоночный формат хранения данных;
- операторы фильтрации, агрегации, `GROUP BY`, `ORDER BY` и `LIMIT`;
- сценарии запросов ClickBench для проверки движка;
- HTTP API для удалённого запуска запросов ClickBench и измерения времени;
- метрики Prometheus и визуализация в Grafana;
- сборка и запуск через CMake, Docker Compose, Terraform и Ansible;
- CI/CD в GitHub Actions со сборкой, тестами и публикацией Docker-образа.

## Локальная сборка через CMake

Требуются:

- CMake 3.22 или новее;
- Ninja;
- компилятор с поддержкой C++23;
- доступ в интернет для первой загрузки зависимостей.

Клонировать репозиторий и собрать движок вместе с HTTP-сервисом:

~~~bash
git clone https://github.com/dmgoldberg1/Columnar-Engine-From-Scratch.git
cd Columnar-Engine-From-Scratch

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DCOLUMNAR_ENABLE_NATIVE_OPTIMIZATIONS=OFF

cmake --build build --target benchmark-service --parallel 2
~~~

Подготовить тестовый датасет и запустить сервис:

~~~bash
mkdir -p datasets
cp hits_sample.csv datasets/hits.csv

BENCHMARK_DATA_DIR="$PWD/datasets" ./build/benchmark-service
~~~

Сервис будет доступен на http://127.0.0.1:8080. Для остановки нажать `Ctrl+C`.

Запуск тестов:

~~~bash
cmake -S . -B build-tests -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCOLUMNAR_ENABLE_NATIVE_OPTIMIZATIONS=OFF

cmake --build build-tests --parallel 2
ctest --test-dir build-tests --output-on-failure --timeout 300
~~~

## Сборка и запуск через Docker

Требуются Docker Engine и Docker Compose. Команды выполняются из корня
репозитория.

~~~bash
mkdir -p datasets
cp hits_sample.csv datasets/hits.csv

BENCHMARK_UID="$(id -u)" \
BENCHMARK_GID="$(id -g)" \
GRAFANA_ADMIN_PASSWORD=admin \
docker compose up --build --detach
~~~

Проверить запущенные контейнеры:

~~~bash
docker compose ps
~~~

После запуска доступны:

- Benchmark API: http://127.0.0.1:8080
- Prometheus: http://127.0.0.1:9090
- Grafana: http://127.0.0.1:3000

Логин Grafana — admin, пароль в примере — admin.

Остановить контейнеры:

~~~bash
docker compose down
~~~

## Benchmark API

API возвращает ответы в формате JSON и принимает идентификаторы запросов от 1
до 43. Query 40 пока является заглушкой и возвращает статус `not_implemented`.
Ответ выполненного запроса содержит его идентификатор, время работы движка и
полученные строки результата.

| Метод и путь | Назначение |
|---|---|
| `GET /health` | проверить состояние сервиса и наличие загруженного датасета |
| `POST /data/load` | подготовить CSV из каталога данных для выполнения запросов |
| `POST /queries/{id}/run` | выполнить Query 1–43 и получить результат со временем выполнения |
| `GET /metrics` | получить метрики в формате Prometheus |

Пример полного цикла работы с API:

~~~bash
# Проверить состояние
curl --fail --silent --show-error http://127.0.0.1:8080/health

# Загрузить datasets/hits.csv
curl --fail --silent --show-error \
  --request POST \
  --header 'Content-Type: application/json' \
  --data '{"source":"hits.csv"}' \
  http://127.0.0.1:8080/data/load

# Выполнить ClickBench Query 1
curl --fail --silent --show-error \
  --request POST \
  http://127.0.0.1:8080/queries/1/run

# Выполнить любой другой запрос, например Query 18
curl --fail --silent --show-error \
  --request POST \
  http://127.0.0.1:8080/queries/18/run

# Получить Prometheus-метрики
curl --fail --silent --show-error http://127.0.0.1:8080/metrics
~~~

## Развёртывание в Yandex Cloud

Terraform создаёт виртуальную машину и сеть в Yandex Cloud. Ansible подключается
к созданной VM по SSH, устанавливает Docker, запускает опубликованный образ
приложения и выполняет smoke-тест.

Требуются Yandex Cloud CLI (`yc`), Terraform, Ansible и SSH-клиент.

### 1. Создать VM через Terraform

~~~bash
cd infra/terraform/yandex

# Выполнить только при первом запуске.
cp terraform.tfvars.example terraform.tfvars
~~~

В `terraform.tfvars` указать свой публичный IP с маской `/32` и путь к публичному
SSH-ключу. Затем выполнить:

~~~bash
export YC_TOKEN="$(yc iam create-token)"
export YC_CLOUD_ID="$(yc config get cloud-id)"
export YC_FOLDER_ID="$(yc config get folder-id)"

terraform init
terraform plan
terraform apply
terraform output
~~~

Запомнить `vm_public_ip` из вывода Terraform.

### 2. Развернуть приложение через Ansible

Сначала один раз проверить обычное SSH-подключение:

~~~bash
ssh -o IdentitiesOnly=yes \
  -i ~/.ssh/id_ed25519 \
  ubuntu@<VM_PUBLIC_IP>
~~~

После подключения выйти с VM и продолжить на локальном компьютере:

~~~bash
cd ../../../infra/ansible

# Выполнить только при первом запуске.
cp inventory.example.ini inventory.ini
~~~

В `inventory.ini` заменить тестовый IP на `vm_public_ip` из Terraform. Затем:

~~~bash
ansible-galaxy collection install --requirements-file requirements.yml
ansible benchmark -m ansible.builtin.ping

ansible-playbook playbooks/site.yml
~~~

После успешного выполнения будут запущены benchmark-service, Prometheus и
Grafana, а Ansible автоматически выполнит smoke-тест. По умолчанию скачивается
готовый публичный Docker-образ с тегом `latest`, поэтому новому пользователю не
нужно собирать или публиковать собственный образ.

Чтобы развернуть конкретную версию, можно передать полный SHA коммита, ранее
опубликованного GitHub Actions:

~~~bash
BENCHMARK_RELEASE=<COMMIT_SHA> ansible-playbook playbooks/site.yml
~~~

### 3. Открыть удалённые интерфейсы

Порты сервисов опубликованы только внутри VM. Для доступа с локального
компьютера открыть SSH-туннель:

~~~bash
ssh -N -o IdentitiesOnly=yes \
  -i ~/.ssh/id_ed25519 \
  -L 18080:127.0.0.1:8080 \
  -L 19090:127.0.0.1:9090 \
  -L 13000:127.0.0.1:3000 \
  ubuntu@<VM_PUBLIC_IP>
~~~

После этого доступны:

- Benchmark API: http://127.0.0.1:18080
- Prometheus: http://127.0.0.1:19090
- Grafana: http://127.0.0.1:13000

Для завершения работы остановить VM через Yandex Cloud. После повторного
запуска публичный IP может измениться — тогда его нужно обновить в
inventory.ini.

Команда `terraform destroy` полностью удаляет созданную VM и данные на её диске.
