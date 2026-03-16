#ifndef DATABASECONTROLLER_H
#define DATABASECONTROLLER_H

#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <memory>

struct LicenseRecord;
class QtDBMigration;

/**
 * @brief Singleton-контроллер базы данных.
 *
 * Потокобезопасность:
 *   Вместо хранения единственного QSqlDatabase m_db (опасно для многопоточного
 *   Drogon), используется паттерн Connection Pool на основе thread-local
 *   именованных соединений Qt. Метод getConnection() возвращает соединение,
 *   уникальное для текущего потока.
 */
class DatabaseController {
public:
  static DatabaseController &instance();

  bool initialize(const QString &dbHost = "127.0.0.1", int dbPort = 5432,
                  const QString &dbName = "licenses_db",
                  const QString &dbUser = "postgres",
                  const QString &dbPass = "postgres",
                  const QString &migrationConfig = "migration.json");

  void setMigrationConfig(const QString &configPath);

  bool saveLicense(const LicenseRecord &record);
  QVector<LicenseRecord> loadAllLicenses(int limit = 0, int offset = 0) const;

  bool deleteLicense(int id);
  bool logAction(const QString &username, const QString &action,
                 const QString &details);

  bool validateUser(const QString &username, const QString &password) const;

private:
  explicit DatabaseController();
  ~DatabaseController();

  /**
   * @brief Возвращает QSqlDatabase для текущего потока.
   *
   * Если соединение для этого потока ещё не создано — открывает новое.
   * Это обеспечивает thread-safety: Qt требует, чтобы каждый поток
   * использовал своё собственное QSqlDatabase-соединение.
   */
  QSqlDatabase getConnection() const;

  bool runMigrations();

  QString m_dbHost;
  int m_dbPort;
  QString m_dbName;
  QString m_dbUser;
  QString m_dbPass;
  QString m_migrationConfig;

  std::unique_ptr<QtDBMigration> m_migrator;
};

#endif // DATABASECONTROLLER_H
