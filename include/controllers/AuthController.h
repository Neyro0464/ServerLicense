#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class AuthController : public drogon::HttpController<AuthController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(AuthController::login, "/api/login", Post);
  ADD_METHOD_TO(AuthController::logout, "/api/logout", Post);
  ADD_METHOD_TO(AuthController::me, "/api/me", Get);
  METHOD_LIST_END

  void login(const HttpRequestPtr &req,
             std::function<void(const HttpResponsePtr &)> &&callback);
  void logout(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback);
  void me(const HttpRequestPtr &req,
          std::function<void(const HttpResponsePtr &)> &&callback);
};
