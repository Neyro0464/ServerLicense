# Руководство по развертыванию License Server (Docker)

Данное руководство предназначено для системного администратора и описывает процесс установки и запуска сервера лицензий в локальной сети компании на ОС **Debian**.

## 1. Системные требования
- **ОС**: Debian 11/12 (или новее).
- **ПО**: Docker v20.10+ и Docker Compose v2.0+.

### Установка Docker (если не установлен)
```bash
sudo apt update
sudo apt install -y ca-certificates curl gnupg
sudo install -m 0755 -d /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/debian/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

echo \
  "deb [arch=\"$(dpkg --print-architecture)\" signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/debian \
  $(. /etc/os-release && echo \"$VERSION_CODENAME\") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt update
sudo apt install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
```

---

## 2. Подготовка к запуску

1. Скопируйте папку с проектом на сервер.
2. Перейдите в директорию проекта:
   ```bash
   cd /path/to/ServerLicense
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

## 3. Запуск приложения

Запустите сборку и запуск контейнеров в фоновом режиме:
```bash
docker compose up -d --build
```

После выполнения приложение будет доступно по адресу: `http://localhost:8080` (или IP-адрес сервера в сети).

---

## 4. Управление контейнерами

- **Просмотр статуса**: `docker compose ps`
- **Просмотр логов приложения**: `docker compose logs -f app`
- **Остановка сервера**: `docker compose down`
- **Перезапуск**: `docker compose restart app`

---

## 5. Система резервного копирования (Backup)

Система содержит контейнер `backup`, который автоматически создает резервные копии базы данных каждые 24 часа.

- **Где хранятся копии**: В папке `./backups` в корне проекта.
- **Очистка**: Скрипт автоматически удаляет старые копии (старше 7 дней).
- **Ручной запуск бекапа**:
  ```bash
  docker compose exec backup /scripts/backup.sh
  ```

---

## 6. База данных и миграции

При первом запуске база данных будет создана автоматически. Приложение при старте проверяет наличие миграций в файле `migration.json` и применяет их.

Для прямого доступа к БД (в случае необходимости):
```bash
docker compose exec db psql -U postgres -d licenses_db
```

---

## 7. Обновление приложения

Для обновления сервера (при получении нового кода):
1. Замените исходные файлы.
2. Пересоберите образ и перезапустите контейнеры:
   ```bash
   docker compose up -d --build
   ```

> [!IMPORTANT]
> Папка `Bin/` содержит критическую библиотеку `libLicenseLib.so`. Убедитесь, что она поставляется вместе с обновлением.
