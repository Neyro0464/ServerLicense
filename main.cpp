#include <drogon/drogon.h>
#include <iostream>

#include "AppConfig.h"
#include "DatabaseController.h"
#include "version.h"

using namespace drogon;

int main() {
  std::cout << "Starting Server. License Lib Version: " << VER_MAJOR << "."
            << VER_BUILD << std::endl;

  // --- Загружаем конфигурацию из файла ---
  // Ищем config.json рядом с исполняемым файлом
  AppConfig &cfg = AppConfig::instance();
  if (!cfg.load("config.json")) {
    std::cerr << "[main] Warning: config.json not found or invalid. "
                 "Using built-in defaults."
              << std::endl;
  }

  // --- Инициализируем БД (параметры берутся из конфига) ---
  if (!DatabaseController::instance().initialize(
          cfg.dbHost(), cfg.dbPort(), cfg.dbName(), cfg.dbUser(),
          cfg.dbPassword(), cfg.migrationConfig())) {
    std::cerr << "[main] Failed to initialize database!" << std::endl;
    return 1;
  }

  // --- Запускаем сервер ---
  app().enableSession(24 * 60 * 60);
  app().setDocumentRoot(cfg.documentRoot().toStdString());
  app().addListener(cfg.serverHost().toStdString(), cfg.serverPort());
  app().run();

  return 0;
}
