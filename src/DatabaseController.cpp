#include "DatabaseController.h"
#include "LicenseRecord.h"
#include "Migration/QtDBMigration.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

// ---------------------------------------------------------------------------
// Вспомогательная функция: возвращает уникальное имя соединения для текущего
// потока. Qt требует, что каждый поток работает со своим QSqlDatabase.
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
  // Соединения Qt SQL очищаются автоматически при выходе из программы.
  // Принудительное removeDatabase() здесь небезопасно: деструктор Singleton
  // может вызваться в момент, когда потоки Drogon ещё активны и держат
  // локальные QSqlDatabase-объекты — это приводит к segfault.
}

DatabaseController &DatabaseController::instance() {
  static DatabaseController instance;
  return instance;
}

// ---------------------------------------------------------------------------
// Инициализация
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

  // Открываем ПЕРВОЕ соединение (в главном потоке) для выполнения миграций.
  // Это же соединение будет использоваться главным потоком в обычной работе.
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
// Connection Pool (per-thread)
// ---------------------------------------------------------------------------

/**
 * @brief Возвращает QSqlDatabase для ТЕКУЩЕГО потока.
 *
 * Qt строго запрещает передавать QSqlDatabase между потоками.
 * Поэтому мы создаём отдельное именованное соединение для каждого потока
 * при первом обращении и переиспользуем его в последующих запросах.
 */
QSqlDatabase DatabaseController::getConnection() const {
  const QString name = connectionNameForCurrentThread();

  if (QSqlDatabase::contains(name)) {
    QSqlDatabase db = QSqlDatabase::database(name, /*open=*/false);
    if (db.isOpen()) {
      return db;
    }
    // Соединение есть, но закрыто — попытаемся переоткрыть
    if (!db.open()) {
      qCritical() << "[DatabaseController] Failed to re-open connection:"
                  << name << db.lastError().text();
    }
    return db;
  }

  // Создаём новое соединение для этого потока
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
// Миграции
// ---------------------------------------------------------------------------

bool DatabaseController::runMigrations() {
  if (!QFile::exists(m_migrationConfig)) {
    qCritical() << "[DatabaseController] Migration config missing:"
                << m_migrationConfig;
    return false;
  }

  // Для миграций используем соединение главного потока
  QSqlDatabase db = getConnection();
  m_migrator = std::make_unique<QtDBMigration>(m_migrationConfig, db);
  return m_migrator->migrate();
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------

bool DatabaseController::saveLicense(const LicenseRecord &record) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical() << "[DatabaseController] saveLicense: DB connection not open.";
    return false;
  }

  QSqlQuery query(db);
  const QString sql =
      "INSERT INTO licenses "
      "(company_name, hardware_id, issue_date, expired_date, signature, "
      "modules) "
      "VALUES (:company, :hwid, :issue, :expired, :signature, :modules)";

  if (!query.prepare(sql)) {
    qCritical() << "[DatabaseController] Failed to prepare saveLicense:"
                << query.lastError().text();
    return false;
  }

  query.bindValue(":company", record.companyName);
  query.bindValue(":hwid", record.hardwareId);
  query.bindValue(":issue", record.issueDate);
  query.bindValue(":expired", record.expiredDate);
  query.bindValue(":signature", record.signature);
  query.bindValue(":modules", record.modules);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] saveLicense exec failed for"
                << record.companyName << ":" << query.lastError().text();
    return false;
  }
  return true;
}

QVector<LicenseRecord> DatabaseController::loadAllLicenses(int limit,
                                                           int offset) const {
  QVector<LicenseRecord> licenses;
  QSqlDatabase db = getConnection();

  QString sql = "SELECT id, company_name, hardware_id, issue_date, "
                "expired_date, modules, generated_at, signature "
                "FROM licenses ORDER BY generated_at DESC";

  if (limit > 0) {
    sql += QString(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);
  }

  QSqlQuery query(db);
  if (!query.exec(sql)) {
    qCritical() << "[DatabaseController] loadAllLicenses failed:"
                << query.lastError().text();
    return licenses;
  }

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
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical()
        << "[DatabaseController] deleteLicense: DB connection not open.";
    return false;
  }

  QSqlQuery query(db);
  query.prepare("DELETE FROM licenses WHERE id = :id");
  query.bindValue(":id", id);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] deleteLicense failed:"
                << query.lastError().text();
    return false;
  }
  return query.numRowsAffected() > 0;
}

bool DatabaseController::logAction(const QString &username,
                                   const QString &action,
                                   const QString &details) {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical() << "[DatabaseController] logAction: DB connection not open.";
    return false;
  }

  QSqlQuery query(db);
  query.prepare("INSERT INTO logs (username, action, details) "
                "VALUES (:username, :action, :details)");
  query.bindValue(":username", username);
  query.bindValue(":action", action);
  query.bindValue(":details", details);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] logAction failed:"
                << query.lastError().text();
    return false;
  }
  return true;
}

bool DatabaseController::validateUser(const QString &username,
                                      const QString &password) const {
  QSqlDatabase db = getConnection();
  if (!db.isOpen()) {
    qCritical() << "[DatabaseController] validateUser: DB connection not open.";
    return false;
  }

  QSqlQuery query(db);
  query.prepare("SELECT password_hash FROM users WHERE username = :username");
  query.bindValue(":username", username);

  if (!query.exec()) {
    qCritical() << "[DatabaseController] validateUser query failed:"
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
