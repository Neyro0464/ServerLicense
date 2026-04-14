#pragma once

#include <QStringList>
#include <drogon/HttpController.h>

using namespace drogon;

class LicenseController : public drogon::HttpController<LicenseController> {
public:
  METHOD_LIST_BEGIN
  // Map POST request to /api/generate
  ADD_METHOD_TO(LicenseController::generate, "/api/generate", Post,
                "AuthFilter");
  // Map GET request to /api/licenses
  ADD_METHOD_TO(LicenseController::getLicenses, "/api/licenses", Get,
                "AuthFilter");
  // Map DELETE request to /api/licenses/{id}
  ADD_METHOD_TO(LicenseController::deleteLicense, "/api/licenses/{id}", Delete,
                "AuthFilter");
  // Map GET request to /api/modules
  ADD_METHOD_TO(LicenseController::getModules, "/api/modules", Get,
                "AuthFilter");
  METHOD_LIST_END

  // Route handler for generating a license
  void generate(const HttpRequestPtr &req,
                std::function<void(const HttpResponsePtr &)> &&callback);

  // Route handler for getting all licenses
  void getLicenses(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);

  // Route handler for deleting a license by ID
  void deleteLicense(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     std::string id);

  // Route handler for getting all available modules
  void getModules(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);
};
