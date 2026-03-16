#include "AuthController.h"
#include "DatabaseController.h"

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

  if (DatabaseController::instance().validateUser(
          QString::fromStdString(username), QString::fromStdString(password))) {
    req->session()->insert("user_id", username);
    DatabaseController::instance().logAction(
        QString::fromStdString(username), "LOGIN_SUCCESS",
        "User logged in to web interface.");
    Json::Value ret;
    ret["status"] = "success";
    auto res = HttpResponse::newHttpJsonResponse(ret);
    callback(res);
  } else {
    DatabaseController::instance().logAction(QString::fromStdString(username),
                                             "LOGIN_FAILED",
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
    auto res = HttpResponse::newHttpJsonResponse(ret);
    callback(res);
  } else {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k401Unauthorized);
    callback(res);
  }
}
