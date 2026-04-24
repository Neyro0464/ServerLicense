#ifndef DATABASECONTROLLER_H
#define DATABASECONTROLLER_H

#include "models/CompanyRecord.h"
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

struct LicenseRecord;
class QtDBMigration;

struct ModuleRecord {
  QString moduleName;
  QString displayLabel;  // Russian name (display_label)
  QString parentModule;  // parent_module_name or empty if root
  int     sortOrder = 0;
};

struct ConfigurationRecord {
  int     configId = 0;
  QString configName;
  QString description;
  QStringList modules; // list of module_name values
};

struct EmployeeRecord {
  QString employeeId;
  QString login;
  QString role;
  QString firstName;
  QString lastName;
  QString middleName;
};

struct AuthResult {
  bool success = false;
  QString role;
  QString employeeId;
};

/**
 * @brief Singleton Database Controller.
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

  bool saveLicense(const LicenseRecord &record, const QString &employeeId);
  QVector<LicenseRecord> loadAllLicenses(int limit = 0, int offset = 0,
                                         const QString &employeeId = "") const;

  bool deleteLicense(const QString &licenseKey);

  bool createEmployee(const QString &login, const QString &password,
                      const QString &role, const QString &firstName, 
                      const QString &lastName, const QString &middleName, 
                      const QString &adminUsername, const QString &adminEmpId);

  bool updateEmployee(const QString &employeeId, const QString &login, 
                      const QString &password, const QString &role, 
                      const QString &firstName, const QString &lastName, 
                      const QString &middleName);
  bool deleteEmployee(const QString &employeeId);
  QVector<EmployeeRecord> getAllEmployees() const;

  bool logAction(const QString &username, const QString &employeeId,
                 const QString &action, const QString &details);

  AuthResult validateUser(const QString &username,
                          const QString &password) const;

  bool addCompany(const CompanyRecord &company);
  QVector<CompanyRecord> getAllCompanies() const;
  bool updateCompany(const QString &companyName, const CompanyRecord &company);
  bool deleteCompany(const QString &companyName);

  // Modules (with hierarchy info)
  QVector<ModuleRecord> getAllModules() const;

  // System Configurations
  QVector<ConfigurationRecord> getAllConfigurations() const;
  bool addConfiguration(const QString &name, const QString &description, const QStringList &modules);
  bool updateConfiguration(int configId, const QString &name, const QString &description, const QStringList &modules);
  bool deleteConfiguration(int configId);

  void cleanupOldLogs(int days);

private:
  explicit DatabaseController();
  ~DatabaseController();

  /**
   * @brief Returns QSqlDatabase for the current thread.
   *
   * If a connection for this thread doesn't exist, it opens a new one.
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
