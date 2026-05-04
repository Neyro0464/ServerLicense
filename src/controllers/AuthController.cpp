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
    std::string role = req->session()->get<std::string>("role");
    ret["role"] = role;

    // Add permissions based on role
    Json::Value permissions(Json::arrayValue);

    // Base permissions for all authenticated users
    permissions.append("view_licenses");

    // Junior manager and above
    if (role == "junior_manager" || role == "senior_manager" || role == "admin") {
      permissions.append("create_license");
      permissions.append("add_company");
      permissions.append("view_companies");
      permissions.append("view_database");
    }

    // Senior manager and above
    if (role == "senior_manager" || role == "admin") {
      permissions.append("edit_company");
      permissions.append("delete_company");
      permissions.append("edit_license");
      permissions.append("delete_license");
    }

    // Admin only
    if (role == "admin") {
      permissions.append("add_config");
      permissions.append("edit_config");
      permissions.append("delete_config");
      permissions.append("view_modules");
      permissions.append("add_module");
      permissions.append("edit_module");
      permissions.append("delete_module");
      permissions.append("view_employees");
      permissions.append("add_employee");
      permissions.append("edit_employee");
      permissions.append("delete_employee");
    }

    ret["permissions"] = permissions;
    auto res = HttpResponse::newHttpJsonResponse(ret);
    callback(res);
  } else {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k401Unauthorized);
    callback(res);
  }
}
