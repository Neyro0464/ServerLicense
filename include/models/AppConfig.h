#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>

/**
 * @brief Singleton class to load application config from a JSON file.
 *
 * Uses Qt (QFile, QJsonDocument) to read
 * server and db parameters from config.json.
 *
 * Example usage:
 *   AppConfig::instance().load("config.json");
 *   QString host = AppConfig::instance().dbHost();
 */
class AppConfig {
public:
  static AppConfig &instance();

  /**
   * @brief Loads configuration from the specified file.
   * @param filePath Path to the JSON config file.
   * @return true if loaded and parsed successfully; false on error.
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
   * @brief Returns the path to the migration config.
   *        Read from "migrationConfig" or uses default.
   */
  QString migrationConfig() const;

  /**
   * @brief Path to the directory with static files (HTML, JS, CSS).
   */
  QString documentRoot() const;

  /**
   * @brief Retention period for logs in days.
   */
  int logsRetentionDays() const;

  /**
   * @brief Checks if the configuration was loaded successfully.
   */
  bool isLoaded() const;

private:
  AppConfig();
  ~AppConfig() = default;

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
  int m_logsRetentionDays = 30;
};

#endif // APPCONFIG_H
