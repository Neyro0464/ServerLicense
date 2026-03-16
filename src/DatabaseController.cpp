#include "DatabaseController.h"
#include "LicenseRecord.h"
#include "Migration/QtDBMigration.h"

#include <QCryptographicHash>

#include <QDebug>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>

DatabaseController::DatabaseController(QObject *parent)
    : QObject(parent), m_dbHost("127.0.0.1"), m_dbPort(5432),
      m_dbName("licenses_db"), m_dbUser("postgres"), m_dbPass("postgres"),
      m_migrationConfig("migration.json") {}

DatabaseController::~DatabaseController() { closeDatabase(); }

DatabaseController &DatabaseController::instance() {
  static DatabaseController instance;
  return instance;
}

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

  if (!openDatabase()) {
    qCritical() << "Failed to open database:" << m_db.lastError().text();
    return false;
  }

  if (!runMigrations()) {
    qCritical() << "Database migrations failed. Check migration.json and logs.";
    closeDatabase();
    return false;
  }

  return true;
}

void DatabaseController::setDatabasePath(const QString &path) {
  // Keeping for compatibility or updating dynamically could be done here.
  Q_UNUSED(path);
}

void DatabaseController::setMigrationConfig(const QString &configPath) {
  m_migrationConfig = configPath;
  if (m_db.isOpen()) {
    runMigrations();
  }
}

bool DatabaseController::openDatabase() {
  m_db = QSqlDatabase::addDatabase("QPSQL");
  m_db.setHostName(m_dbHost);
  m_db.setPort(m_dbPort);
  m_db.setDatabaseName(m_dbName);
  m_db.setUserName(m_dbUser);
  m_db.setPassword(m_dbPass);

  if (!m_db.open()) {
    qCritical() << "Cannot open database:" << m_db.lastError().text();
    return false;
  }

  return true;
}

bool DatabaseController::runMigrations() {
  if (!QFile::exists(m_migrationConfig)) {
    qCritical() << "Migration config missing:" << m_migrationConfig;
    return false;
  }

  m_migrator = std::make_unique<QtDBMigration>(m_migrationConfig, m_db);

  if (!m_migrator->migrate()) {
    return false;
  }
  return true;
}

void DatabaseController::closeDatabase() {
  if (m_db.isOpen()) {
    m_db.close();
  }
}

bool DatabaseController::saveLicense(const LicenseRecord &record) {

  if (!m_db.isOpen()) {
    qCritical() << "Attempted to save license to a closed database";
    return false;
  }

  QSqlQuery query(m_db);

  const QString sql =
      "INSERT INTO licenses "
      "(company_name, hardware_id, issue_date, expired_date, signature, "
      "modules) "
      "VALUES (:company, :hwid, :issue, :expired, :signature, :modules)";

  if (!query.prepare(sql)) {
    qCritical() << "Failed to prepare save query:" << query.lastError().text();
    return false;
  }

  query.bindValue(":company", record.companyName);
  query.bindValue(":hwid", record.hardwareId);
  query.bindValue(":issue", record.issueDate);
  query.bindValue(":expired", record.expiredDate);
  query.bindValue(":signature", record.signature);
  query.bindValue(":modules", record.modules);

  if (!query.exec()) {
    qCritical() << "Exec failed for company:" << record.companyName
                << "Error:" << query.lastError().text();
    return false;
  }
  return true;
}

QVector<LicenseRecord> DatabaseController::loadAllLicenses(int limit,
                                                           int offset) const {
  QVector<LicenseRecord> licenses;

  QString sql = "SELECT id, company_name, hardware_id, issue_date, "
                "expired_date, modules, generated_at, signature "
                "FROM licenses ORDER BY generated_at DESC";

  if (limit > 0) {
    sql += QString(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);
  }

  QSqlQuery query(sql, m_db);

  while (query.next()) {
    LicenseRecord rec;
    rec.id = query.value(0).toInt();
    rec.companyName = query.value(1).toString();
    rec.hardwareId = query.value(2).toString();
    rec.issueDate = query.value(3).toDate();
    rec.expiredDate = query.value(4).toDate();
    rec.modules = query.value(5).toString();
    rec.generatedAt = query.value(6).toDateTime();
    rec.signature = query.value(7).toString();
    licenses.append(rec);
  }
  return licenses;
}

bool DatabaseController::deleteLicense(int id) {
  if (!m_db.isOpen()) {
    qCritical() << "Attempted to delete license with a closed database";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("DELETE FROM licenses WHERE id = :id");
  query.bindValue(":id", id);

  if (!query.exec()) {
    qCritical() << "Failed to delete license:" << query.lastError().text();
    return false;
  }
  return query.numRowsAffected() > 0;
}

bool DatabaseController::logAction(const QString &username,
                                   const QString &action,
                                   const QString &details) {
  if (!m_db.isOpen()) {
    qCritical() << "Attempted to log action to a closed database";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("INSERT INTO logs (username, action, details) VALUES "
                "(:username, :action, :details)");
  query.bindValue(":username", username);
  query.bindValue(":action", action);
  query.bindValue(":details", details);

  if (!query.exec()) {
    qCritical() << "Failed to log action:" << query.lastError().text();
    return false;
  }
  return true;
}

bool DatabaseController::validateUser(const QString &username,
                                      const QString &password) const {
  if (!m_db.isOpen()) {
    qCritical() << "Attempted to validate user with a closed database";
    return false;
  }

  QSqlQuery query(m_db);
  query.prepare("SELECT password_hash FROM users WHERE username = :username");
  query.bindValue(":username", username);

  if (!query.exec()) {
    qCritical() << "Failed to execute validate user query:"
                << query.lastError().text();
    return false;
  }

  if (query.next()) {
    QString storedHash = query.value(0).toString();
    QByteArray hashData =
        QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    QString inputHash = QString(hashData.toHex());
    return storedHash.compare(inputHash, Qt::CaseInsensitive) == 0;
  }

  return false;
}
