#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class ModuleController : public drogon::HttpController<ModuleController> {
public:
  METHOD_LIST_BEGIN
  // GET  /api/modules  — all roles (authenticated) - already exists in LicenseController
  // POST /api/modules  — admin only
  ADD_METHOD_TO(ModuleController::addModule, "/api/modules", Post, "AuthFilter");
  // PUT  /api/modules/{name}  — admin only
  ADD_METHOD_TO(ModuleController::updateModule, "/api/modules/{name}", Put, "AuthFilter");
  // DELETE /api/modules/{name}  — admin only
  ADD_METHOD_TO(ModuleController::deleteModule, "/api/modules/{name}", Delete, "AuthFilter");
  METHOD_LIST_END

  void addModule(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);

  void updateModule(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    std::string name);

  void deleteModule(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    std::string name);
};
