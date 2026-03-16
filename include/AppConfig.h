#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

/**
 * @brief Singleton-класс для загрузки конфигурации приложения из JSON-файла.
 *
 * Использует Qt-инфраструктуру (QFile, QJsonDocument) для чтения
 * параметров сервера и базы данных из config.json.
 *
 * Пример использования:
 *   AppConfig::instance().load("config.json");
 *   QString host = AppConfig::instance().dbHost();
 */
class AppConfig {
public:
  static AppConfig &instance();

  /**
   * @brief Загружает конфигурацию из указанного файла.
   * @param filePath Путь к JSON-файлу конфигурации.
   * @return true, если файл успешно загружен и разобран; false при ошибке.
   */
  bool load(const QString &filePath);

  // --- Database ---
  QString dbHost() const;
  int dbPort() const;
  QString dbName() const;
  QString dbUser() const;
  QString dbPassword() const;

  // --- Server ---
  QString serverHost() const;
  int serverPort() const;

  /**
   * @brief Возвращает путь к конфигу миграций.
   *        Читается из поля "migrationConfig" или используется дефолт.
   */
  QString migrationConfig() const;

  /**
   * @brief Путь к директории со статическими файлами (HTML, JS, CSS).
   */
  QString documentRoot() const;

  /**
   * @brief Проверяет, была ли конфигурация успешно загружена.
   */
  bool isLoaded() const;

private:
  AppConfig();
  ~AppConfig() = default;

  // Запрещаем копирование
  AppConfig(const AppConfig &) = delete;
  AppConfig &operator=(const AppConfig &) = delete;

  bool m_loaded = false;

  // Database
  QString m_dbHost = QStringLiteral("127.0.0.1");
  int m_dbPort = 5432;
  QString m_dbName = QStringLiteral("licenses_db");
  QString m_dbUser = QStringLiteral("postgres");
  QString m_dbPassword = QStringLiteral("postgres");

  // Server
  QString m_serverHost = QStringLiteral("0.0.0.0");
  int m_serverPort = 8080;

  // Migration
  QString m_migrationConfig = QStringLiteral("migration.json");
  QString m_documentRoot = QStringLiteral("../public");
};

#endif // APPCONFIG_H
