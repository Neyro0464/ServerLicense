#include "controllers/AuthController.h"
#include "database/DatabaseController.h"

void AuthController::login(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto json = req->getJsonObject();
  if (!json) {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k400BadRequest);
    callback(res);
    return;
  }

  std::string username = (*json)["username"].asString();
  std::string password = (*json)["password"].asString();

  AuthResult authRes = DatabaseController::instance().validateUser(
      QString::fromStdString(username), QString::fromStdString(password));

  if (authRes.success) {
    req->session()->erase("user_id");
    req->session()->erase("role");
    req->session()->erase("employee_id");
    req->session()->insert("user_id", username);
    req->session()->insert("role", authRes.role.toStdString());
    req->session()->insert("employee_id", authRes.employeeId.toStdString());
    DatabaseController::instance().logAction(
        QString::fromStdString(username), authRes.employeeId, "LOGIN_SUCCESS",
        "User logged in to web interface.");
    Json::Value ret;
    ret["status"] = "success";
    auto res = HttpResponse::newHttpJsonResponse(ret);
    callback(res);
  } else {
    DatabaseController::instance().logAction(QString::fromStdString(username),
                                             "", "LOGIN_FAILED",
                                             "Invalid credentials attempted.");
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k401Unauthorized);
    callback(res);
  }
}

void AuthController::logout(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  req->session()->erase("user_id");
  req->session()->erase("role");
  req->session()->erase("employee_id");
  auto res = HttpResponse::newHttpResponse();
  res->setStatusCode(k200OK);
  callback(res);
}

void AuthController::me(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (req->session()->find("user_id")) {
    Json::Value ret;
    ret["username"] = req->session()->get<std::string>("user_id");
    ret["role"] = req->session()->get<std::string>("role");
    auto res = HttpResponse::newHttpJsonResponse(ret);
    callback(res);
  } else {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k401Unauthorized);
    callback(res);
  }
}
