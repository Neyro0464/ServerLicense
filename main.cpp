#include <drogon/drogon.h>
#include <iostream>

#include "models/AppConfig.h"
#include "database/DatabaseController.h"
#include "version.h"
#include "ServerVersion.h"
#include <QDateTime>
#include <QFile>
#include <QTextStream>

using namespace drogon;

// CI/CD Test
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

  // --- SPA Fallback ---
  // Если запрошенный путь не совпадает ни с одним контроллером и не является
  // статическим файлом из documentRoot — отдаём index.html.
  // Это позволяет клиентскому роутеру (SPA) корректно обрабатывать прямые
  // переходы по ссылкам (например /database, /employees).
  // API-маршруты (/api/...) при этом по-прежнему вернут 404.
  std::string docRoot = cfg.documentRoot().toStdString();
  app().registerHandler(
      "/{path}",
      [docRoot](const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback,
                const std::string &path) {
        // Не перехватываем API-маршруты
        if (path.rfind("api", 0) == 0) {
          auto res = HttpResponse::newHttpResponse();
          res->setStatusCode(k404NotFound);
          callback(res);
          return;
        }
        // Если в пути есть расширение файла (.css, .js, .png, …) —
        // отдаём статический файл из documentRoot, не SPA-заглушку
        if (path.find('.') != std::string::npos) {
          auto res = HttpResponse::newFileResponse(docRoot + "/" + path);
          callback(res);
          return;
        }
        // Для SPA-маршрутов без расширения отдаём index.html
        auto res = HttpResponse::newFileResponse(docRoot + "/index.html");
        callback(res);
      },
      {Get});

  app().registerHandler(
      "/api/version",
      [](const HttpRequestPtr &req,
         std::function<void(const HttpResponsePtr &)> &&callback) {
        Json::Value ret;
        QString serverVersion = QString("%1.%2").arg(SERVER_VER_MAJOR).arg(QDate::currentDate().toString("yyyy.MM.dd"));
        ret["serverVersion"] = serverVersion.toStdString();
        ret["libVersion"] = std::to_string(VER_MAJOR) + "." + std::to_string(VER_BUILD) + " " + VER_HASH;
        auto resp = HttpResponse::newHttpJsonResponse(ret);
        callback(resp);
      },
      {Get});

  app().addListener(cfg.serverHost().toStdString(), cfg.serverPort());
  app().run();

  return 0;
}
