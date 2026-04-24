#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class ConfigurationController
    : public drogon::HttpController<ConfigurationController> {
public:
  METHOD_LIST_BEGIN
  // GET  /api/configurations  — all roles (authenticated)
  ADD_METHOD_TO(ConfigurationController::getConfigurations,
                "/api/configurations", Get, "AuthFilter");
  // POST /api/configurations  — admin only
  ADD_METHOD_TO(ConfigurationController::addConfiguration,
                "/api/configurations", Post, "AuthFilter");
  // PUT  /api/configurations/{id}  — admin only
  ADD_METHOD_TO(ConfigurationController::updateConfiguration,
                "/api/configurations/{id}", Put, "AuthFilter");
  // DELETE /api/configurations/{id}  — admin only
  ADD_METHOD_TO(ConfigurationController::deleteConfiguration,
                "/api/configurations/{id}", Delete, "AuthFilter");
  METHOD_LIST_END

  void getConfigurations(const HttpRequestPtr &req,
                         std::function<void(const HttpResponsePtr &)> &&callback);

  void addConfiguration(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback);

  void updateConfiguration(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback,
                           std::string id);

  void deleteConfiguration(const HttpRequestPtr &req,
                           std::function<void(const HttpResponsePtr &)> &&callback,
                           std::string id);
};
