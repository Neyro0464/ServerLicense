#ifndef DATABASECONTROLLER_H
#define DATABASECONTROLLER_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>
#include <memory>

struct LicenseRecord;
class QtDBMigration;

class DatabaseController : public QObject {
  Q_OBJECT

public:
  static DatabaseController &instance();

  bool initialize(const QString &dbHost = "127.0.0.1", int dbPort = 5432,
                  const QString &dbName = "licenses_db",
                  const QString &dbUser = "postgres",
                  const QString &dbPass = "postgres",
                  const QString &migrationConfig = "migration.json");

  void setDatabasePath(const QString &path);
  void setMigrationConfig(const QString &configPath);

  bool saveLicense(const LicenseRecord &record);
  QVector<LicenseRecord> loadAllLicenses(int limit = 0, int offset = 0) const;

  bool deleteLicense(int id);
  bool logAction(const QString &username, const QString &action,
                 const QString &details);

  bool validateUser(const QString &username, const QString &password) const;

private:
  explicit DatabaseController(QObject *parent = nullptr);
  ~DatabaseController() override;

  bool openDatabase();
  bool runMigrations();
  void closeDatabase();

  QSqlDatabase m_db;
  QString m_dbHost;
  int m_dbPort;
  QString m_dbName;
  QString m_dbUser;
  QString m_dbPass;
  QString m_migrationConfig;
  std::unique_ptr<QtDBMigration> m_migrator;
};

#endif // DATABASECONTROLLER_H
