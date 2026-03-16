#include <QString>
#include <QVector>
#include <drogon/drogon.h>
#include <iostream>

// Подключаем заголовки из папки include
#include "DatabaseController.h"
#include "LicenseData.h"
#include "LicenseRecord.h"
#include "LicenseValidator.h"
#include "version.h"

using namespace drogon;

int main() {
  std::cout << "Starting Server. License Lib Version: " << VER_MAJOR << "."
            << VER_BUILD << std::endl;

  // Инициализируем БД
  if (!DatabaseController::instance().initialize(
          "127.0.0.1", 5432, "licenses_db", "postgres", "postgres")) {
    std::cerr << "Failed to initialize database!" << std::endl;
    return 1;
  }

  app().enableSession(24 * 60 * 60);
  app().setDocumentRoot("../public");
  app().addListener("0.0.0.0", 8080);
  app().run();

  return 0;
}
