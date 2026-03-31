#include "models/AppConfig.h"

#include <cstdlib>
#include <QDebug>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

AppConfig::AppConfig() = default;

AppConfig &AppConfig::instance() {
  static AppConfig instance;
  return instance;
}

bool AppConfig::load(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qCritical() << "[AppConfig] Cannot open config file:" << filePath
                << "- Using default values.";
    // Not treating this as a fatal error, proceeding with defaults
    m_loaded = false;
    return false;
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();

  if (parseError.error != QJsonParseError::NoError) {
    qCritical() << "[AppConfig] JSON parse error:" << parseError.errorString()
                << "at offset" << parseError.offset;
    m_loaded = false;
    return false;
  }

  if (!doc.isObject()) {
    qCritical() << "[AppConfig] Root element of config is not a JSON object.";
    m_loaded = false;
    return false;
  }

  QJsonObject root = doc.object();

  // --- Database ---
  if (root.contains(QStringLiteral("database")) &&
      root["database"].isObject()) {
    QJsonObject db = root["database"].toObject();
    m_dbHost = db.value(QStringLiteral("host")).toString(m_dbHost);
    m_dbPort = db.value(QStringLiteral("port")).toInt(m_dbPort);
    m_dbName = db.value(QStringLiteral("name")).toString(m_dbName);
    m_dbUser = db.value(QStringLiteral("user")).toString(m_dbUser);
    m_dbPassword = db.value(QStringLiteral("password")).toString(m_dbPassword);
  } else {
    qWarning() << "[AppConfig] Section 'database' is missing. Using defaults.";
  }

  // --- Server ---
  if (root.contains(QStringLiteral("server")) && root["server"].isObject()) {
    QJsonObject srv = root["server"].toObject();
    m_serverHost = srv.value(QStringLiteral("host")).toString(m_serverHost);
    m_serverPort = srv.value(QStringLiteral("port")).toInt(m_serverPort);
  } else {
    qWarning() << "[AppConfig] Section 'server' is missing. Using defaults.";
  }

  // --- Migration config ---
  if (root.contains(QStringLiteral("migrationConfig"))) {
    m_migrationConfig = root.value(QStringLiteral("migrationConfig"))
                            .toString(m_migrationConfig);
  }

  // --- Document root (static files) ---
  if (root.contains(QStringLiteral("documentRoot"))) {
    m_documentRoot =
        root.value(QStringLiteral("documentRoot")).toString(m_documentRoot);
  }

  // --- Logs Retention ---
  if (root.contains(QStringLiteral("logsRetentionDays"))) {
    m_logsRetentionDays =
        root.value(QStringLiteral("logsRetentionDays")).toInt(m_logsRetentionDays);
  }

  // --- Override with environment variables ---
  const char* envDbHost = std::getenv("DB_HOST");
  if (envDbHost) m_dbHost = QString::fromUtf8(envDbHost);

  const char* envDbPort = std::getenv("DB_PORT");
  if (envDbPort) m_dbPort = QString::fromUtf8(envDbPort).toInt();

  const char* envDbName = std::getenv("DB_NAME");
  if (envDbName) m_dbName = QString::fromUtf8(envDbName);

  const char* envDbUser = std::getenv("DB_USER");
  if (envDbUser) m_dbUser = QString::fromUtf8(envDbUser);

  const char* envDbPass = std::getenv("DB_PASSWORD");
  if (envDbPass) m_dbPassword = QString::fromUtf8(envDbPass);

  const char* envSrvHost = std::getenv("SERVER_HOST");
  if (envSrvHost) m_serverHost = QString::fromUtf8(envSrvHost);

  const char* envSrvPort = std::getenv("SERVER_PORT");
  if (envSrvPort) m_serverPort = QString::fromUtf8(envSrvPort).toInt();

  m_loaded = true;
  qDebug() << "[AppConfig] Loaded successfully from" << filePath;
  return true;
}

// --- Getters ---

bool AppConfig::isLoaded() const { return m_loaded; }
QString AppConfig::dbHost() const { return m_dbHost; }
int AppConfig::dbPort() const { return m_dbPort; }
QString AppConfig::dbName() const { return m_dbName; }
QString AppConfig::dbUser() const { return m_dbUser; }
QString AppConfig::dbPassword() const { return m_dbPassword; }
QString AppConfig::serverHost() const { return m_serverHost; }
int AppConfig::serverPort() const { return m_serverPort; }
QString AppConfig::migrationConfig() const { return m_migrationConfig; }
QString AppConfig::documentRoot() const { return m_documentRoot; }
int AppConfig::logsRetentionDays() const { return m_logsRetentionDays; }
