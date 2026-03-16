#include <gtest/gtest.h>

#include "AppConfig.h"

#include <QTemporaryFile>
#include <QTextStream>

// ---------------------------------------------------------------------------
// Вспомогательная функция: создаёт временный JSON-файл с заданным содержимым
// и возвращает его путь.
// ---------------------------------------------------------------------------
static QString writeTempConfig(const QString &json) {
  QTemporaryFile *f = new QTemporaryFile();
  f->setAutoRemove(false); // удалим вручную после теста
  f->open();
  QTextStream out(f);
  out << json;
  f->close();
  return f->fileName();
}

// ---------------------------------------------------------------------------
// AppConfig — загрузка корректного конфига
// ---------------------------------------------------------------------------
TEST(AppConfigTest, LoadsValidConfig) {
  const QString json = R"({
        "database": {
            "host": "192.168.1.10",
            "port": 5433,
            "name": "test_db",
            "user": "test_user",
            "password": "test_pass"
        },
        "server": {
            "host": "127.0.0.1",
            "port": 9090
        },
        "migrationConfig": "custom_migration.json"
    })";

  // Для каждого теста нам нужен "чистый" экземпляр — но Singleton сохраняет
  // состояние между тестами. Поэтому просто перезагружаем его с новым файлом.
  QString path = writeTempConfig(json);
  AppConfig &cfg = AppConfig::instance();
  bool loaded = cfg.load(path);
  QFile::remove(path);

  EXPECT_TRUE(loaded);
  EXPECT_TRUE(cfg.isLoaded());
  EXPECT_EQ(cfg.dbHost(), QStringLiteral("192.168.1.10"));
  EXPECT_EQ(cfg.dbPort(), 5433);
  EXPECT_EQ(cfg.dbName(), QStringLiteral("test_db"));
  EXPECT_EQ(cfg.dbUser(), QStringLiteral("test_user"));
  EXPECT_EQ(cfg.dbPassword(), QStringLiteral("test_pass"));
  EXPECT_EQ(cfg.serverHost(), QStringLiteral("127.0.0.1"));
  EXPECT_EQ(cfg.serverPort(), 9090);
  EXPECT_EQ(cfg.migrationConfig(), QStringLiteral("custom_migration.json"));
}

// ---------------------------------------------------------------------------
// AppConfig — несуществующий файл: не крашимся, используем дефолты
// ---------------------------------------------------------------------------
TEST(AppConfigTest, FallsBackToDefaultsOnMissingFile) {
  AppConfig &cfg = AppConfig::instance();
  bool loaded = cfg.load("/tmp/nonexistent_config_file_12345.json");

  EXPECT_FALSE(loaded);
  EXPECT_FALSE(cfg.isLoaded());

  // После неудачи значения должны оставаться дефолтными
  EXPECT_FALSE(cfg.dbHost().isEmpty());
  EXPECT_GT(cfg.dbPort(), 0);
  EXPECT_GT(cfg.serverPort(), 0);
}

// ---------------------------------------------------------------------------
// AppConfig — невалидный JSON: не крашимся
// ---------------------------------------------------------------------------
TEST(AppConfigTest, HandlesInvalidJson) {
  const QString badJson = QStringLiteral("{ this is not valid json }}}");
  QString path = writeTempConfig(badJson);
  AppConfig &cfg = AppConfig::instance();
  bool loaded = cfg.load(path);
  QFile::remove(path);

  EXPECT_FALSE(loaded);
  EXPECT_FALSE(cfg.isLoaded());
}

// ---------------------------------------------------------------------------
// AppConfig — частичный конфиг: пропущен блок "server", но не падает
// ---------------------------------------------------------------------------
TEST(AppConfigTest, HandlesPartialConfig) {
  const QString json = R"({
        "database": {
            "host": "db.example.com",
            "port": 5432,
            "name": "prod",
            "user": "admin",
            "password": "secret"
        }
    })";

  QString path = writeTempConfig(json);
  AppConfig &cfg = AppConfig::instance();
  bool loaded = cfg.load(path);
  QFile::remove(path);

  EXPECT_TRUE(loaded);
  EXPECT_EQ(cfg.dbHost(), QStringLiteral("db.example.com"));
  // Серверный порт остаётся дефолтным
  EXPECT_GT(cfg.serverPort(), 0);
}
