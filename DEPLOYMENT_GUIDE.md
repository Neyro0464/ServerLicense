# Руководство по развертыванию License Server (Docker)

Данное руководство предназначено для системного администратора и описывает процесс установки и запуска сервера лицензий в локальной сети компании на ОС **Ubuntu**.

## 1. Системные требования
- **ОС**: Ubuntu 22.04 LTS (рекомендуется) / 24.04 LTS.
- **ПО**: Docker v20.10+ и Docker Compose v2.0+.
- **Образ контейнера**: `ubuntu:22.04` (используется в `Dockerfile`).

### Установка Docker (если не установлен)
```bash
# Удаление старых версий (если есть)
sudo apt-get remove docker docker-engine docker.io containerd runc

# Установка зависимостей
sudo apt-get update
sudo apt-get install -y ca-certificates curl gnupg lsb-release

# Добавление официального GPG ключа Docker
sudo mkdir -m 0755 -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# Настройка репозитория
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Установка Docker
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```
---

## 2. Подготовка к запуску

1. Скопируйте папку с проектом на сервер.
2. Перейдите в директорию проекта:
   ```bash
   cd /путь/к/проекту/ServerLicense
   ```
3. Создайте файл конфигурации среды `.env` (используйте файл `.env.example` как шаблон):
   ```bash
   cp .env.example .env
   ```
4. Отредактируйте `.env`, установив надежный пароль для базы данных:
   ```env
   POSTGRES_PASSWORD=ваш_надежный_пароль
   ```

---

## 3. Настройка Docker-файлов перед деплоем

Перед первым запуском необходимо проверить и при необходимости скорректировать два файла: `docker-compose.yml` и `Dockerfile`.

### 3.1 Файл `.env` — конфигурация секретов

Все чувствительные параметры вынесены в файл `.env`, который **не попадает в репозиторий** (добавлен в `.gitignore`).

**Обязательно измените следующие значения в `.env`:**

```env
# Имя базы данных (можно оставить по умолчанию)
POSTGRES_DB=licenses_db

# Пользователь БД (можно оставить по умолчанию)
POSTGRES_USER=postgres

# ⚠️ ОБЯЗАТЕЛЬНО смените на надёжный пароль!
POSTGRES_PASSWORD=ваш_надёжный_пароль_здесь

# Порт, на котором сервер будет доступен снаружи контейнера
SERVER_PORT=8080
```

> [!CAUTION]
> Никогда не оставляйте пароль `postgres` в продакшене. Это стандартный пароль по умолчанию, который первым проверяют при атаках.

---

### 3.2 Файл `docker-compose.yml` — что может потребовать изменений

#### Порт сервера (строка 9)

```yaml
ports:
  - "${SERVER_PORT:-8080}:8080"
```

Левая часть (`SERVER_PORT`) — внешний порт на хост-машине. Если порт `8080` уже занят другим сервисом, смените его в `.env`:

```env
SERVER_PORT=9090
```

Сервер станет доступен по адресу `http://<IP>:9090`.

#### Версия образа PostgreSQL (строка 24)

```yaml
db:
  image: postgres:15
```

При необходимости смените версию PostgreSQL. Приложение тестировалось с версией **15**. Использование другой версии может потребовать проверки совместимости.

#### Частота резервного копирования (строка 61)

```yaml
sleep 86400;   # 86400 секунд = 24 часа
```

Измените значение `86400` для другой периодичности бэкапов:
- `3600` — каждый час
- `43200` — каждые 12 часов
- `604800` — раз в неделю

#### Срок хранения бэкапов (строка 60)

```yaml
find /backups -type f -name '*.sql' -mtime +7 -delete;
```

Значение `+7` означает удаление файлов старше 7 дней. Замените на нужное количество дней.

---

### 3.3 Файл `Dockerfile` — что может потребовать изменений

#### Базовый образ сборки (строки 2, 38)

```dockerfile
# Stage 1
FROM ubuntu:22.04 AS builder

# Stage 2
FROM ubuntu:22.04
```

Оба этапа сборки используют **Ubuntu 22.04 LTS**. Если потребуется сменить версию Ubuntu, измените тег в обеих строках одновременно. Переход на Ubuntu 24.04 возможен, но может потребовать проверки совместимости версий пакетов Qt6 и libjsoncpp.

> [!NOTE]
> В обеих стадиях задана переменная `ENV DEBIAN_FRONTEND=noninteractive`. Она предотвращает зависание `apt-get` на интерактивных диалогах (например, выбор часового пояса при установке `tzdata`).

#### Имя скомпилированного бинарного файла (строка 57)

```dockerfile
COPY --from=builder /app/build/LicenseServer /app/LicenseServer
```

> [!IMPORTANT]
> Имя `LicenseServer` должно совпадать с именем цели сборки, заданной в `CMakeLists.txt`. Если в `CMakeLists.txt` задано другое имя — измените его здесь.

Проверить текущее имя цели:
```bash
grep "add_executable" CMakeLists.txt
```

#### Рабочая директория и путь к `libLicenseLib.so` (строки 51, 70)

```dockerfile
WORKDIR /app
ENV LD_LIBRARY_PATH=/app/Bin:$LD_LIBRARY_PATH
```

Приложение запускается из директории `/app`, а библиотека `libLicenseLib.so` ищется в `/app/Bin`. Если структура папок изменилась — скорректируйте оба значения.

#### Версия libjsoncpp (строка 45)

```dockerfile
libjsoncpp25
```

Версия библиотеки (`25`) привязана к конкретной версии Debian. При смене базового образа эта версия может отличаться. Для поиска актуальной версии выполните:

```bash
apt-cache search libjsoncpp
```

---

## 4. Запуск приложения

Запустите сборку и запуск контейнеров в фоновом режиме:
```bash
docker compose up -d --build
```

После выполнения приложение будет доступно по адресу: `http://localhost:8080` (или IP-адрес сервера в сети).

---

## 5. Управление контейнерами

- **Просмотр статуса**: `docker compose ps`
- **Просмотр логов приложения**: `docker compose logs -f app`
- **Остановка сервера**: `docker compose down`
- **Перезапуск**: `docker compose restart app`

---

## 6. Система резервного копирования (Backup)

Система содержит контейнер `backup`, который автоматически создает резервные копии базы данных каждые 24 часа.

- **Где хранятся копии**: В папке `./backups` в корне проекта.
- **Очистка**: Скрипт автоматически удаляет старые копии (старше 7 дней).
- **Ручной запуск бекапа**:
  ```bash
  docker compose exec backup /scripts/backup.sh
  ```

---

## 7. База данных и миграции

При первом запуске база данных будет создана автоматически. Приложение при старте проверяет наличие миграций в файле `migration.json` и применяет их.

Для прямого доступа к БД (в случае необходимости):
```bash
docker compose exec db psql -U postgres -d licenses_db
```

---

## 8. Обновление приложения

Для обновления сервера (при получении нового кода):
1. Замените исходные файлы.
2. Пересоберите образ и перезапустите контейнеры:
   ```bash
   docker compose up -d --build
   ```

> [!IMPORTANT]
> Папка `Bin/` содержит критическую библиотеку `libLicenseLib.so`. Убедитесь, что она поставляется вместе с обновлением.

---

## 9. Проверка работы сервера

После запуска выполните следующие проверки:

### 9.1 Статус контейнеров

```bash
docker compose ps
```

Все три сервиса (`app`, `db`, `backup`) должны иметь статус **running**.

### 9.2 Проверка доступности HTTP

```bash
curl -v http://localhost:8080
```

Сервер должен вернуть HTTP-ответ (код 200 или другой ожидаемый от приложения).

### 9.3 Проверка подключения к базе данных

```bash
docker compose exec db pg_isready -U postgres -d licenses_db
```

Ожидаемый ответ: `localhost:5432 - accepting connections`

### 9.4 Просмотр логов приложения

```bash
# Логи в реальном времени
docker compose logs -f app

# Последние 50 строк
docker compose logs --tail=50 app
```

### 9.5 Проверка наличия бэкапов

```bash
# Список созданных бэкапов (каталог ./backups в директории проекта)
ls -lh ./backups/
```

> [!NOTE]
> Первый бэкап создаётся через 24 часа после запуска контейнера `backup`.
> Для немедленного ручного запуска используйте:
> ```bash
> docker compose exec backup /scripts/backup.sh
> ```

### 9.6 Проверка логов в случае ошибок

```bash
# Все контейнеры сразу
docker compose logs

# Только БД
docker compose logs db

# Только backup
docker compose logs backup
```
