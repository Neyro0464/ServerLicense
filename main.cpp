#include <drogon/drogon.h>
#include <iostream>

#include "models/AppConfig.h"
#include "database/DatabaseController.h"
#include "version.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>

using namespace drogon;

void customMessageHandler(QtMsgType type, const QMessageLogContext &context,
                          const QString &msg) {
  QFile file("logs/server.log");
  if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << " ["
        << type << "] " << msg << "\n";
  }
  QTextStream console(stdout);
  console << msg << Qt::endl;
}

int main() {
  qInstallMessageHandler(customMessageHandler);
  std::cout << "Starting Server. License Lib Version: " << VER_MAJOR << "."
            << VER_BUILD << std::endl;

  // --- Load config from file ---
  // Look for config.json next to the executable
  AppConfig &cfg = AppConfig::instance();
  if (!cfg.load("config.json")) {
    std::cerr << "[main] Warning: config.json not found or invalid. "
                 "Using built-in defaults."
              << std::endl;
  }

  // --- Initialize DB (parameters from config) ---
  if (!DatabaseController::instance().initialize(
          cfg.dbHost(), cfg.dbPort(), cfg.dbName(), cfg.dbUser(),
          cfg.dbPassword(), cfg.migrationConfig())) {
    std::cerr << "[main] Failed to initialize database!" << std::endl;
    return 1;
  }

  // --- Cleanup old logs ---
  DatabaseController::instance().cleanupOldLogs(cfg.logsRetentionDays());

  // --- Запускаем сервер ---
  app().enableSession(24 * 60 * 60);
  app().setDocumentRoot(cfg.documentRoot().toStdString());
  app().addListener(cfg.serverHost().toStdString(), cfg.serverPort());
  app().run();

  return 0;
}
