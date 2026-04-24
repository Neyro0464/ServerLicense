#include "database/DatabaseController.h"
#include "models/LicenseRecord.h"
#include "Migration/QtDBMigration.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

// ---------------------------------------------------------------------------
// Returns a unique connection name for the current thread.
// ---------------------------------------------------------------------------
static QString connectionNameForCurrentThread() {
  return QStringLiteral("LicenseDB_") +
         QString::number(
             reinterpret_cast<quintptr>(QThread::currentThreadId()));
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

DatabaseController::DatabaseController()
    : m_dbHost("127.0.0.1"), m_dbPort(5432), m_dbName("licenses_db"),
      m_dbUser("postgres"), m_dbPass("postgres"),
      m_migrationConfig("migration.json") {}

DatabaseController::~DatabaseController() {
  // Qt SQL connections are cleaned up automatically on program exit.
  // Forcing removeDatabase() here is unsafe: the Singleton destructor
  // might be called while Drogon threads are still active and holding
  // local QSqlDatabase objects — this leads to a segfault.
}

DatabaseController &DatabaseController::instance() {
  static DatabaseController instance;
  return instance;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool DatabaseController::initialize(const QString &dbHost, int dbPort,
                                    const QString &dbName,
                                    const QString &dbUser,
                                    const QString &dbPass,
                                    const QString &migrationConfig) {
  m_dbHost = dbHost;
  m_dbPort = dbPort;
  m_dbName = dbName;
  m_dbUser = dbUser;
  m_dbPass = dbPass;

  if (!migrationConfig.isEmpty()) {
    m_migrationConfig = migrationConfig;
  }

  // Open the FIRST connection (in the main thread) to run migrations.
  // This same connection will be used by the main thread during normal operation.
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical()
        << "[DatabaseController] Failed to open initial database connection.";
    return false;
  }

  if (!runMigrations()) {
    qCritical() << "[DatabaseController] Database migrations failed.";
    return false;
  }

  return true;
}

void DatabaseController::setMigrationConfig(const QString &configPath) {
  m_migrationConfig = configPath;
}

// ---------------------------------------------------------------------------
// Connection Pool
// ---------------------------------------------------------------------------

/**
 * @brief Returns QSqlDatabase for the CURRENT thread.
 *
 * Qt strictly forbids passing QSqlDatabase between threads.
 * Therefore, we create a separate named connection for each thread
 * on the first access and reuse it in subsequent requests. (Connection Pool)
 */
QSqlDatabase DatabaseController::getConnection() const {
  const QString name = connectionNameForCurrentThread();

  if (QSqlDatabase::contains(name)) {
    QSqlDatabase db = QSqlDatabase::database(name, false);
    if (db.isOpen()) {
      return db;
    }
    // Connection exists but is closed — try to reopen it
    if (!db.open()) {
      qCritical() << "[DatabaseController] Failed to re-open connection:"
                  << name << db.lastError().text();
    }
    return db;
  }

  // Create a new connection for this thread
  QSqlDatabase db = QSqlDatabase::addDatabase("QPSQL", name);
  db.setHostName(m_dbHost);
  db.setPort(m_dbPort);
  db.setDatabaseName(m_dbName);
  db.setUserName(m_dbUser);
  db.setPassword(m_dbPass);

  if (!db.open()) {
    qCritical() << "[DatabaseController] Cannot open DB connection for thread"
                << name << ":" << db.lastError().text();
  } else {
    qDebug() << "[DatabaseController] Opened new DB connection for thread:"
             << name;
  }

  return db;
}

// ---------------------------------------------------------------------------
// Migrations
// ---------------------------------------------------------------------------

bool DatabaseController::runMigrations() {
  if (!QFile::exists(m_migrationConfig)) {
    qCritical() << "[DatabaseController] Migration config missing:"
                << m_migrationConfig;
    return false;
  }

  // Use the main thread connection for migrations
  QSqlDatabase db = getConnection();
  m_migrator = std::make_unique<QtDBMigration>(m_migrationConfig, db);
  return m_migrator->migrate();
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------

bool DatabaseController::saveLicense(const LicenseRecord &record,
                                     const QString &employeeId) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical() << "[DatabaseController] saveLicense: DB connection not open.";
    return false;
  }

  db.transaction();

  // 1. Insert into licenses
  QSqlQuery query(db);
  query.prepare("INSERT INTO licenses "
                "(license_key, company_name, issue_date, expired_date, hwid) "
                "VALUES (:key, :company, :issue, :expired, :hwid)");
  query.bindValue(":key", record.signature);
  query.bindValue(":company", record.companyName);
  query.bindValue(":issue", record.issueDate);
  query.bindValue(":expired", record.expiredDate);
  query.bindValue(":hwid", record.hardwareId);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] saveLicense exec failed for licenses:"
                << query.lastError().text();
    db.rollback();
    return false;
  }

  // 2. Insert into license_modules
  QStringList moduleList = record.modules.split(",", Qt::SkipEmptyParts);
  for (QString mod : moduleList) {
    mod = mod.trimmed();
    if (mod.isEmpty())
      continue;

    // Ensure module exists first (or we could assume it exists, but let's be
    // safe)
    QSqlQuery modQuery(db);
    modQuery.prepare("INSERT INTO modules (module_name, description) VALUES "
                     "(:name, '') ON CONFLICT DO NOTHING");
    modQuery.bindValue(":name", mod);
    modQuery.exec();

    QSqlQuery lmQuery(db);
    lmQuery.prepare("INSERT INTO license_modules (license_key, module_name) "
                    "VALUES (:key, :mod)");
    lmQuery.bindValue(":key", record.signature);
    lmQuery.bindValue(":mod", mod);
    if (!lmQuery.exec()) {
      qCritical()
          << "[DatabaseController] saveLicense exec failed for license_modules:"
          << lmQuery.lastError().text();
      db.rollback();
      return false;
    }
  }

  // 3. Insert into generations
  QSqlQuery genQuery(db);
  genQuery.prepare("INSERT INTO generations (employee_id, generated_key) "
                   "VALUES (:emp, :key)");
  genQuery.bindValue(":emp", employeeId);
  genQuery.bindValue(":key", record.signature);
  if (!genQuery.exec()) {
    qCritical()
        << "[DatabaseController] saveLicense exec failed for generations:"
        << genQuery.lastError().text();
    db.rollback();
    return false;
  }

  db.commit();
  return true;
}

QVector<LicenseRecord>
DatabaseController::loadAllLicenses(int limit, int offset,
                                    const QString &employeeId) const {
  QVector<LicenseRecord> licenses;
  QSqlDatabase db = getConnection();

  QString sql = "SELECT l.license_key, l.company_name, l.hwid, l.issue_date, "
                "l.expired_date, g.generation_date, "
                "(SELECT string_agg(module_name, ', ') FROM license_modules "
                "WHERE license_key = l.license_key) as modules "
                "FROM licenses l "
                "LEFT JOIN generations g ON l.license_key = g.generated_key ";

  if (!employeeId.isEmpty()) {
    sql += "WHERE g.employee_id = :empId ";
  }

  sql += "ORDER BY g.generation_date DESC";

  if (limit > 0) {
    sql += QString(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);
  }

  QSqlQuery query(db);
  query.prepare(sql);

  if (!employeeId.isEmpty()) {
    query.bindValue(":empId", employeeId);
  }

  if (!query.exec()) {
    qCritical() << "[DatabaseController] loadAllLicenses failed:"
                << query.lastError().text();
    return licenses;
  }

  while (query.next()) {
    LicenseRecord rec;
    rec.signature = query.value(0).toString();
    rec.companyName = query.value(1).toString();
    rec.hardwareId = query.value(2).toString();
    rec.issueDate = query.value(3).toDate();
    rec.expiredDate = query.value(4).toDate();
    rec.generatedAt = query.value(5).toDateTime();
    rec.modules = query.value(6).toString();
    licenses.append(rec);
  }
  return licenses;
}

bool DatabaseController::deleteLicense(const QString &licenseKey) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical()
        << "[DatabaseController] deleteLicense: DB connection not open.";
    return false;
  }

  // Thanks to ON DELETE CASCADE on generations and license_modules,
  // deleting from licenses will delete related records.
  QSqlQuery query(db);
  query.prepare("DELETE FROM licenses WHERE license_key = :key");
  query.bindValue(":key", licenseKey);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] deleteLicense failed:"
                << query.lastError().text();
    return false;
  }
  return query.numRowsAffected() > 0;
}

bool DatabaseController::addCompany(const CompanyRecord &company) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen())
    return false;
  QSqlQuery query(db);
  query.prepare("INSERT INTO companies (company_name, city, contacts) VALUES "
                "(:name, :city, :contacts) "
                "ON CONFLICT (company_name) DO UPDATE SET city = "
                "EXCLUDED.city, contacts = EXCLUDED.contacts");
  query.bindValue(":name", company.companyName);
  query.bindValue(":city", company.city);
  query.bindValue(":contacts", company.contacts);
  return query.exec();
}

QVector<CompanyRecord> DatabaseController::getAllCompanies() const {
  QVector<CompanyRecord> companies;
  QSqlDatabase db = getConnection();
  QSqlQuery query(db);
  if (!query.exec("SELECT company_name, date_added, city, contacts FROM "
                  "companies ORDER BY company_name")) {
    return companies;
  }
  while (query.next()) {
    CompanyRecord r;
    r.companyName = query.value(0).toString();
    r.dateAdded = query.value(1).toDateTime();
    r.city = query.value(2).toString();
    r.contacts = query.value(3).toString();
    companies.append(r);
  }
  return companies;
}

bool DatabaseController::createEmployee(
    const QString &login, const QString &password, const QString &role,
    const QString &firstName, const QString &lastName,
    const QString &middleName, const QString &adminUsername,
    const QString &adminEmpId) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical()
        << "[DatabaseController] createEmployee: DB connection not open.";
    return false;
  }

  QByteArray hashData =
      QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
  QString inputHash = QString(hashData.toHex());

  if (!db.transaction()) {
    qCritical() << "[DatabaseController] Failed to start transaction.";
    return false;
  }

  QSqlQuery accountQuery(db);
  accountQuery.prepare(
      "INSERT INTO accounts (login, password_hash, status_name) "
      "VALUES (:login, :pass, :role) RETURNING account_id");
  accountQuery.bindValue(":login", login);
  accountQuery.bindValue(":pass", inputHash);
  accountQuery.bindValue(":role", role);

  if (!accountQuery.exec() || !accountQuery.next()) {
    qCritical() << "[DatabaseController] createEmployee (accounts) failed:"
                << accountQuery.lastError().text();
    db.rollback();
    return false;
  }

  int accountId = accountQuery.value(0).toInt();

  QSqlQuery empQuery(db);
  empQuery.prepare("INSERT INTO employees (last_name, first_name, "
                   "middle_name, account_id) "
                   "VALUES (:lName, :fName, :mName, :accId) RETURNING employee_id");
  empQuery.bindValue(":lName", lastName);
  empQuery.bindValue(":fName", firstName);
  empQuery.bindValue(":mName", middleName);
  empQuery.bindValue(":accId", accountId);

  if (!empQuery.exec() || !empQuery.next()) {
    qCritical() << "[DatabaseController] createEmployee (employees) failed:"
                << empQuery.lastError().text();
    db.rollback();
    return false;
  }

  QString generatedEmpId = empQuery.value(0).toString();

  db.commit();

  logAction(adminUsername, adminEmpId, "CREATE_EMPLOYEE",
            "Created employee: " + generatedEmpId + " login: " + login);

  return true;
}

bool DatabaseController::logAction(const QString &username,
                                   const QString &employeeId,
                                   const QString &action,
                                   const QString &details) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical() << "[DatabaseController] logAction: DB connection not open.";
    return false;
  }

  // 1. Write to database table logs
  QSqlQuery query(db);
  query.prepare("INSERT INTO logs (username, employee_id, action, details) "
                "VALUES (:username, :emp, :action, :details)");
  query.bindValue(":username", username);
  query.bindValue(":emp", employeeId);
  query.bindValue(":action", action);
  query.bindValue(":details", details);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] logAction failed to insert to DB:"
                << query.lastError().text();
    // Do not return false, proceed to log to file
  }

  // 2. Fetch Employee name for file log
  QString eName = "Unknown";
  QSqlQuery empQuery(db);
  empQuery.prepare(
      "SELECT last_name, first_name FROM employees WHERE employee_id = :emp");
  empQuery.bindValue(":emp", employeeId);
  if (empQuery.exec() && empQuery.next()) {
    eName = empQuery.value(1).toString() + " " + empQuery.value(0).toString();
  }

  // 3. Write to database.log file
  QFile file("logs/database.log");
  if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")
        << " - ACTION: " << action << " | Details: " << details
        << " | Account: " << username << " | Employee: " << eName
        << " (ID: " << employeeId << ")\n";
  }

  return true;
}

AuthResult DatabaseController::validateUser(const QString &username,
                                            const QString &password) const {
  AuthResult result;
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical() << "[DatabaseController] validateUser: DB connection not open.";
    return result;
  }

  QSqlQuery query(db);
  // Join accounts and employees to get employee_id and role
  query.prepare("SELECT a.password_hash, a.status_name, e.employee_id "
                "FROM accounts a "
                "LEFT JOIN employees e ON a.account_id = e.account_id "
                "WHERE a.login = :username");
  query.bindValue(":username", username);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] validateUser query failed:"
                << query.lastError().text();
    return result;
  }

  if (query.next()) {
    QString storedHash = query.value(0).toString();
    QByteArray hashData =
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    QString inputHash = QString(hashData.toHex());
    if (storedHash.compare(inputHash, Qt::CaseInsensitive) == 0) {
      result.success = true;
      result.role = query.value(1).toString();
      result.employeeId = query.value(2).toString();
    }
  }

  return result;
}

bool DatabaseController::updateEmployee(const QString &employeeId, const QString &login, 
                                        const QString &password, const QString &role, 
                                        const QString &firstName, const QString &lastName, 
                                        const QString &middleName) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  db.transaction();

  // Find account_id for this employee
  QSqlQuery getAcc(db);
  getAcc.prepare("SELECT account_id FROM employees WHERE employee_id = :empId");
  getAcc.bindValue(":empId", employeeId);
  if (!getAcc.exec() || !getAcc.next()) {
    db.rollback();
    return false;
  }
  int accountId = getAcc.value(0).toInt();

  // Update employee details
  QSqlQuery empQuery(db);
  empQuery.prepare("UPDATE employees SET first_name = :fName, last_name = :lName, middle_name = :mName WHERE employee_id = :empId");
  empQuery.bindValue(":fName", firstName);
  empQuery.bindValue(":lName", lastName);
  empQuery.bindValue(":mName", middleName);
  empQuery.bindValue(":empId", employeeId);
  if (!empQuery.exec()) {
    db.rollback();
    return false;
  }

  // Update account (login, role, and conditionally password)
  QSqlQuery accQuery(db);
  if (password.isEmpty()) {
    accQuery.prepare("UPDATE accounts SET login = :login, status_name = :role WHERE account_id = :accId");
  } else {
    accQuery.prepare("UPDATE accounts SET login = :login, status_name = :role, password_hash = :pass WHERE account_id = :accId");
    QByteArray hashData = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    accQuery.bindValue(":pass", QString(hashData.toHex()));
  }
  accQuery.bindValue(":login", login);
  accQuery.bindValue(":role", role);
  accQuery.bindValue(":accId", accountId);

  if (!accQuery.exec()) {
    db.rollback();
    return false;
  }

  db.commit();
  return true;
}

bool DatabaseController::deleteEmployee(const QString &employeeId) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  db.transaction();

  QSqlQuery getAcc(db);
  getAcc.prepare("SELECT account_id FROM employees WHERE employee_id = :empId");
  getAcc.bindValue(":empId", employeeId);
  if (!getAcc.exec() || !getAcc.next()) {
    db.rollback();
    return false;
  }
  int accountId = getAcc.value(0).toInt();

  QSqlQuery empQuery(db);
  empQuery.prepare("DELETE FROM employees WHERE employee_id = :empId");
  empQuery.bindValue(":empId", employeeId);
  if (!empQuery.exec()) {
    db.rollback();
    return false;
  }

  QSqlQuery accQuery(db);
  accQuery.prepare("DELETE FROM accounts WHERE account_id = :accId");
  accQuery.bindValue(":accId", accountId);
  if (!accQuery.exec()) {
    db.rollback();
    return false;
  }

  db.commit();
  return true;
}

QVector<EmployeeRecord> DatabaseController::getAllEmployees() const {
  QVector<EmployeeRecord> result;
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return result;

  QSqlQuery query(db);
  if (!query.exec("SELECT e.employee_id, a.login, a.status_name, e.first_name, e.last_name, e.middle_name "
                  "FROM employees e JOIN accounts a ON e.account_id = a.account_id "
                  "ORDER BY e.employee_id")) {
    return result;
  }

  while (query.next()) {
    EmployeeRecord rec;
    rec.employeeId = query.value(0).toString();
    rec.login = query.value(1).toString();
    rec.role = query.value(2).toString();
    rec.firstName = query.value(3).toString();
    rec.lastName = query.value(4).toString();
    rec.middleName = query.value(5).toString();
    result.append(rec);
  }
  return result;
}

bool DatabaseController::updateCompany(const QString &companyName, const CompanyRecord &company) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  QSqlQuery query(db);
  query.prepare("UPDATE companies SET company_name = :newName, city = :city, contacts = :contacts WHERE company_name = :oldName");
  query.bindValue(":newName", company.companyName);
  query.bindValue(":city", company.city);
  query.bindValue(":contacts", company.contacts);
  query.bindValue(":oldName", companyName);

  return query.exec();
}

bool DatabaseController::deleteCompany(const QString &companyName) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  QSqlQuery query(db);
  query.prepare("DELETE FROM companies WHERE company_name = :name");
  query.bindValue(":name", companyName);

  return query.exec();
}

QVector<ModuleRecord> DatabaseController::getAllModules() const {
  QVector<ModuleRecord> modules;
  QSqlDatabase db = getConnection();
  if (!db.isOpen())
    return modules;

  QSqlQuery query(db);
  if (!query.exec("SELECT module_name, COALESCE(display_label, module_name), "
                  "COALESCE(parent_module, ''), COALESCE(sort_order, 0), "
                  "COALESCE(is_selectable, true), COALESCE(requires_device, false), "
                  "COALESCE(dependency_group, ''), COALESCE(required_with, '{}'), "
                  "COALESCE(is_active, true), COALESCE(description, '') "
                  "FROM modules WHERE is_active = true ORDER BY COALESCE(sort_order, 0), module_name")) {
    qCritical() << "[DatabaseController] getAllModules failed:"
                << query.lastError().text();
    return modules;
  }

  while (query.next()) {
    ModuleRecord rec;
    rec.moduleName   = query.value(0).toString();
    rec.displayLabel = query.value(1).toString();
    rec.parentModule = query.value(2).toString();
    rec.sortOrder    = query.value(3).toInt();
    rec.isSelectable = query.value(4).toBool();
    rec.requiresDevice = query.value(5).toBool();
    rec.dependencyGroup = query.value(6).toString();

    // Parse TEXT[] array from PostgreSQL
    QString reqWithStr = query.value(7).toString();
    if (!reqWithStr.isEmpty() && reqWithStr != "{}") {
      // Remove braces and split by comma
      reqWithStr = reqWithStr.mid(1, reqWithStr.length() - 2); // Remove { }
      rec.requiredWith = reqWithStr.split(',', Qt::SkipEmptyParts);
      // Trim each element
      for (QString &s : rec.requiredWith) {
        s = s.trimmed();
      }
    }

    rec.isActive = query.value(8).toBool();
    rec.description = query.value(9).toString();
    modules.append(rec);
  }
  return modules;
}

QVector<ConfigurationRecord> DatabaseController::getAllConfigurations() const {
  QVector<ConfigurationRecord> configs;
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return configs;

  QSqlQuery query(db);
  if (!query.exec("SELECT sc.config_id, sc.config_name, COALESCE(sc.description, ''), "
                  "COALESCE(string_agg(cm.module_name, ','), '') "
                  "FROM system_configurations sc "
                  "LEFT JOIN configuration_modules cm ON sc.config_id = cm.config_id "
                  "GROUP BY sc.config_id, sc.config_name, sc.description "
                  "ORDER BY sc.config_id")) {
    qCritical() << "[DatabaseController] getAllConfigurations failed:"
                << query.lastError().text();
    return configs;
  }

  while (query.next()) {
    ConfigurationRecord rec;
    rec.configId    = query.value(0).toInt();
    rec.configName  = query.value(1).toString();
    rec.description = query.value(2).toString();
    QString mods    = query.value(3).toString();
    if (!mods.isEmpty()) {
      for (const QString &m : mods.split(',', Qt::SkipEmptyParts))
        rec.modules.append(m.trimmed());
    }
    configs.append(rec);
  }
  return configs;
}

bool DatabaseController::addConfiguration(const QString &name, const QString &description, const QStringList &modules) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  db.transaction();

  QSqlQuery insQuery(db);
  insQuery.prepare("INSERT INTO system_configurations (config_name, description) "
                   "VALUES (:name, :desc) RETURNING config_id");
  insQuery.bindValue(":name", name);
  insQuery.bindValue(":desc", description);
  if (!insQuery.exec() || !insQuery.next()) {
    qCritical() << "[DatabaseController] addConfiguration insert failed:"
                << insQuery.lastError().text();
    db.rollback();
    return false;
  }
  int configId = insQuery.value(0).toInt();

  for (const QString &mod : modules) {
    QSqlQuery lnk(db);
    lnk.prepare("INSERT INTO configuration_modules (config_id, module_name) VALUES (:cid, :mod) ON CONFLICT DO NOTHING");
    lnk.bindValue(":cid", configId);
    lnk.bindValue(":mod", mod.trimmed());
    if (!lnk.exec()) {
      db.rollback();
      return false;
    }
  }

  db.commit();
  return true;
}

bool DatabaseController::updateConfiguration(int configId, const QString &name, const QString &description, const QStringList &modules) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  db.transaction();

  QSqlQuery updQuery(db);
  updQuery.prepare("UPDATE system_configurations SET config_name = :name, description = :desc WHERE config_id = :id");
  updQuery.bindValue(":name", name);
  updQuery.bindValue(":desc", description);
  updQuery.bindValue(":id",   configId);
  if (!updQuery.exec()) {
    db.rollback();
    return false;
  }

  // Replace module links
  QSqlQuery delLinks(db);
  delLinks.prepare("DELETE FROM configuration_modules WHERE config_id = :id");
  delLinks.bindValue(":id", configId);
  if (!delLinks.exec()) {
    db.rollback();
    return false;
  }

  for (const QString &mod : modules) {
    QSqlQuery lnk(db);
    lnk.prepare("INSERT INTO configuration_modules (config_id, module_name) VALUES (:cid, :mod) ON CONFLICT DO NOTHING");
    lnk.bindValue(":cid", configId);
    lnk.bindValue(":mod", mod.trimmed());
    if (!lnk.exec()) {
      db.rollback();
      return false;
    }
  }

  db.commit();
  return true;
}

bool DatabaseController::deleteConfiguration(int configId) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  // CASCADE will remove configuration_modules rows automatically
  QSqlQuery query(db);
  query.prepare("DELETE FROM system_configurations WHERE config_id = :id");
  query.bindValue(":id", configId);
  return query.exec() && query.numRowsAffected() > 0;
}

void DatabaseController::cleanupOldLogs(int days) {
  if (days <= 0) return; // Automatic cleanup disabled if <= 0

  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return;

  QSqlQuery query(db);
  query.prepare("DELETE FROM logs WHERE created_at < NOW() - INTERVAL '" + QString::number(days) + " days'");
  
  if (!query.exec()) {
    qCritical() << "[DatabaseController] cleanupOldLogs failed:" << query.lastError().text();
  } else {
    int removed = query.numRowsAffected();
    if (removed > 0) {
      qDebug() << "[DatabaseController] Cleaned up" << removed << "old log entries.";
    }
  }
}

bool DatabaseController::addModule(const QString &moduleName, const QString &displayLabel,
                                    const QString &description, const QString &parentModule,
                                    int sortOrder, bool isSelectable, bool requiresDevice,
                                    const QString &dependencyGroup, const QStringList &requiredWith) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  // Check if module with this name already exists (including soft-deleted)
  QSqlQuery checkExisting(db);
  checkExisting.prepare("SELECT is_active FROM modules WHERE module_name = :name");
  checkExisting.bindValue(":name", moduleName);
  if (checkExisting.exec() && checkExisting.next()) {
    bool isActive = checkExisting.value(0).toBool();
    if (isActive) {
      qCritical() << "[DatabaseController] addModule: module already exists:" << moduleName;
      return false;
    } else {
      // Module exists but is soft-deleted, reactivate it instead
      qDebug() << "[DatabaseController] addModule: reactivating soft-deleted module:" << moduleName;
      QSqlQuery reactivate(db);
      reactivate.prepare("UPDATE modules SET display_label = :label, description = :desc, "
                        "parent_module = :parent, sort_order = :sort, is_selectable = :selectable, "
                        "requires_device = :device, dependency_group = :depgroup, required_with = :reqwith, "
                        "is_active = true WHERE module_name = :name");
      reactivate.bindValue(":name", moduleName);
      reactivate.bindValue(":label", displayLabel.isEmpty() ? moduleName : displayLabel);
      reactivate.bindValue(":desc", description);
      reactivate.bindValue(":parent", parentModule.isEmpty() ? QVariant(QVariant::String) : parentModule);
      reactivate.bindValue(":sort", sortOrder);
      reactivate.bindValue(":selectable", isSelectable);
      reactivate.bindValue(":device", requiresDevice);
      reactivate.bindValue(":depgroup", dependencyGroup.isEmpty() ? QVariant(QVariant::String) : dependencyGroup);

      QString reqWithArray = "{";
      for (int i = 0; i < requiredWith.size(); ++i) {
        if (i > 0) reqWithArray += ",";
        reqWithArray += requiredWith[i].trimmed();
      }
      reqWithArray += "}";
      reactivate.bindValue(":reqwith", reqWithArray);

      return reactivate.exec();
    }
  }

  // Validate parent module exists if specified
  if (!parentModule.isEmpty()) {
    QSqlQuery checkParent(db);
    checkParent.prepare("SELECT 1 FROM modules WHERE module_name = :parent AND is_active = true");
    checkParent.bindValue(":parent", parentModule);
    if (!checkParent.exec() || !checkParent.next()) {
      qCritical() << "[DatabaseController] addModule: parent module does not exist:" << parentModule;
      return false;
    }
  }

  // Convert QStringList to PostgreSQL array format
  QString reqWithArray = "{";
  for (int i = 0; i < requiredWith.size(); ++i) {
    if (i > 0) reqWithArray += ",";
    reqWithArray += requiredWith[i].trimmed();
  }
  reqWithArray += "}";

  QSqlQuery query(db);
  query.prepare("INSERT INTO modules (module_name, display_label, description, parent_module, "
                "sort_order, is_selectable, requires_device, dependency_group, required_with, is_active) "
                "VALUES (:name, :label, :desc, :parent, :sort, :selectable, :device, :depgroup, :reqwith, true)");
  query.bindValue(":name", moduleName);
  query.bindValue(":label", displayLabel.isEmpty() ? moduleName : displayLabel);
  query.bindValue(":desc", description);
  query.bindValue(":parent", parentModule.isEmpty() ? QVariant(QVariant::String) : parentModule);
  query.bindValue(":sort", sortOrder);
  query.bindValue(":selectable", isSelectable);
  query.bindValue(":device", requiresDevice);
  query.bindValue(":depgroup", dependencyGroup.isEmpty() ? QVariant(QVariant::String) : dependencyGroup);
  query.bindValue(":reqwith", reqWithArray);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] addModule failed:" << query.lastError().text();
    return false;
  }

  return true;
}

bool DatabaseController::updateModule(const QString &moduleName, const QString &displayLabel,
                                       const QString &description, const QString &parentModule,
                                       int sortOrder, bool isSelectable, bool requiresDevice,
                                       const QString &dependencyGroup, const QStringList &requiredWith,
                                       bool isActive) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  // Validate parent module exists if specified and not self-referencing
  if (!parentModule.isEmpty() && parentModule != moduleName) {
    QSqlQuery checkParent(db);
    checkParent.prepare("SELECT 1 FROM modules WHERE module_name = :parent AND is_active = true");
    checkParent.bindValue(":parent", parentModule);
    if (!checkParent.exec() || !checkParent.next()) {
      qCritical() << "[DatabaseController] updateModule: parent module does not exist:" << parentModule;
      return false;
    }
  }

  // Convert QStringList to PostgreSQL array format
  QString reqWithArray = "{";
  for (int i = 0; i < requiredWith.size(); ++i) {
    if (i > 0) reqWithArray += ",";
    reqWithArray += requiredWith[i].trimmed();
  }
  reqWithArray += "}";

  QSqlQuery query(db);
  query.prepare("UPDATE modules SET display_label = :label, description = :desc, "
                "parent_module = :parent, sort_order = :sort, is_selectable = :selectable, "
                "requires_device = :device, dependency_group = :depgroup, required_with = :reqwith, "
                "is_active = :active WHERE module_name = :name");
  query.bindValue(":name", moduleName);
  query.bindValue(":label", displayLabel.isEmpty() ? moduleName : displayLabel);
  query.bindValue(":desc", description);
  query.bindValue(":parent", parentModule.isEmpty() || parentModule == moduleName ? QVariant(QVariant::String) : parentModule);
  query.bindValue(":sort", sortOrder);
  query.bindValue(":selectable", isSelectable);
  query.bindValue(":device", requiresDevice);
  query.bindValue(":depgroup", dependencyGroup.isEmpty() ? QVariant(QVariant::String) : dependencyGroup);
  query.bindValue(":reqwith", reqWithArray);
  query.bindValue(":active", isActive);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] updateModule failed:" << query.lastError().text();
    return false;
  }

  return query.numRowsAffected() > 0;
}

bool DatabaseController::deleteModule(const QString &moduleName) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) return false;

  db.transaction();

  // Check if module is used in any licenses
  QSqlQuery checkUsage(db);
  checkUsage.prepare("SELECT COUNT(*) FROM license_modules WHERE module_name = :name");
  checkUsage.bindValue(":name", moduleName);
  if (checkUsage.exec() && checkUsage.next() && checkUsage.value(0).toInt() > 0) {
    qWarning() << "[DatabaseController] deleteModule: module is used in licenses, performing soft delete:" << moduleName;
    // Soft delete instead of hard delete
    QSqlQuery softDel(db);
    softDel.prepare("UPDATE modules SET is_active = false WHERE module_name = :name");
    softDel.bindValue(":name", moduleName);
    bool success = softDel.exec() && softDel.numRowsAffected() > 0;
    if (success) {
      db.commit();
    } else {
      db.rollback();
    }
    return success;
  }

  // Check if module has active children
  QSqlQuery checkChildren(db);
  checkChildren.prepare("SELECT COUNT(*) FROM modules WHERE parent_module = :name AND is_active = true");
  checkChildren.bindValue(":name", moduleName);
  if (checkChildren.exec() && checkChildren.next() && checkChildren.value(0).toInt() > 0) {
    qWarning() << "[DatabaseController] deleteModule: module has active children, cannot delete:" << moduleName;
    db.rollback();
    return false;
  }

  // Before deleting, set parent_module to NULL for any inactive children
  QSqlQuery clearParent(db);
  clearParent.prepare("UPDATE modules SET parent_module = NULL WHERE parent_module = :name");
  clearParent.bindValue(":name", moduleName);
  clearParent.exec();

  // Hard delete if not used in licenses and has no active children
  QSqlQuery query(db);
  query.prepare("DELETE FROM modules WHERE module_name = :name");
  query.bindValue(":name", moduleName);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] deleteModule failed:" << query.lastError().text();
    db.rollback();
    return false;
  }

  bool success = query.numRowsAffected() > 0;
  if (success) {
    db.commit();
  } else {
    db.rollback();
  }
  return success;
}

