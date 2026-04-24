#include "controllers/EmployeeController.h"
#include "database/DatabaseController.h"
#include <stdexcept>

using namespace drogon;

void EmployeeController::addEmployee(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  if (req->session()->get<std::string>("role") != "admin") {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k403Forbidden);
    callback(res);
    return;
  }

  auto json = req->getJsonObject();
  if (!json) {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k400BadRequest);
    callback(res);
    return;
  }

  try {
    QString login = QString::fromStdString((*json)["login"].asString());
    QString password = QString::fromStdString((*json)["password"].asString());
    QString role = QString::fromStdString((*json)["role"].asString());
    QString firstName = QString::fromStdString((*json)["first_name"].asString());
    QString lastName = QString::fromStdString((*json)["last_name"].asString());
    QString middleName = "";
    if (json->isMember("middle_name")) {
        middleName = QString::fromStdString((*json)["middle_name"].asString());
    }

    if (login.isEmpty() || password.isEmpty() || role.isEmpty() ||
        firstName.isEmpty() || lastName.isEmpty()) {
      throw std::invalid_argument("Необходимо заполнить все обязательные поля.");
    }

    std::string adminEmpId = req->session()->get<std::string>("employee_id");
    std::string adminUsername = req->session()->get<std::string>("user_id");

    bool success = DatabaseController::instance().createEmployee(
        login, password, role, firstName, lastName, middleName,
        QString::fromStdString(adminUsername),
        QString::fromStdString(adminEmpId));

    if (!success) {
      throw std::runtime_error(
          "Ошибка сервера: не удалось сохранить сотрудника. "
          "Возможно, логин или ID сотрудника уже существует.");
    }

    Json::Value ret;
    ret["status"] = "success";
    auto res = HttpResponse::newHttpJsonResponse(ret);
    callback(res);

  } catch (const std::exception &e) {
    Json::Value ret;
    ret["error"] = e.what();
    auto res = HttpResponse::newHttpJsonResponse(ret);
    res->setStatusCode(k400BadRequest);
    callback(res);
  }
}

void EmployeeController::getEmployees(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  if (req->session()->get<std::string>("role") != "admin") {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k403Forbidden);
    callback(res);
    return;
  }

  try {
    auto employees = DatabaseController::instance().getAllEmployees();
    Json::Value responseJson(Json::arrayValue);

    for (const auto &emp : employees) {
      Json::Value item;
      item["employeeId"] = emp.employeeId.toStdString();
      item["login"] = emp.login.toStdString();
      item["role"] = emp.role.toStdString();
      item["firstName"] = emp.firstName.toStdString();
      item["lastName"] = emp.lastName.toStdString();
      item["middleName"] = emp.middleName.toStdString();
      responseJson.append(item);
    }

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    callback(resp);

  } catch (const std::exception &e) {
    Json::Value error;
    error["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}

void EmployeeController::updateEmployee(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string employeeId) {
  if (req->session()->get<std::string>("role") != "admin") {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k403Forbidden);
    callback(res);
    return;
  }

  auto json = req->getJsonObject();
  if (!json) {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k400BadRequest);
    callback(res);
    return;
  }

  try {
    QString qEmployeeId = QString::fromStdString(employeeId);
    QString login = QString::fromStdString((*json)["login"].asString());
    QString password = json->isMember("password") ? QString::fromStdString((*json)["password"].asString()) : "";
    QString role = QString::fromStdString((*json)["role"].asString());
    QString firstName = QString::fromStdString((*json)["first_name"].asString());
    QString lastName = QString::fromStdString((*json)["last_name"].asString());
    QString middleName = json->isMember("middle_name") ? QString::fromStdString((*json)["middle_name"].asString()) : "";

    if (login.isEmpty() || role.isEmpty() || firstName.isEmpty() || lastName.isEmpty()) {
      throw std::invalid_argument("All mandatory fields must be provided.");
    }

    bool success = DatabaseController::instance().updateEmployee(
        qEmployeeId, login, password, role, firstName, lastName, middleName);

    if (!success) {
      throw std::runtime_error("Ошибка сервера: не удалось обновить данные сотрудника.");
    }
    
    std::string adminEmpId = req->session()->get<std::string>("employee_id");
    std::string adminUsername = req->session()->get<std::string>("user_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(adminUsername), QString::fromStdString(adminEmpId),
        "UPDATE_EMPLOYEE", "Updated Employee ID: " + qEmployeeId);

    Json::Value ret;
    ret["status"] = "success";
    auto res = HttpResponse::newHttpJsonResponse(ret);
    callback(res);

  } catch (const std::exception &e) {
    Json::Value ret;
    ret["error"] = e.what();
    auto res = HttpResponse::newHttpJsonResponse(ret);
    res->setStatusCode(k400BadRequest);
    callback(res);
  }
}

void EmployeeController::deleteEmployee(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string employeeId) {
  if (req->session()->get<std::string>("role") != "admin") {
    auto res = HttpResponse::newHttpResponse();
    res->setStatusCode(k403Forbidden);
    callback(res);
    return;
  }

  try {
    QString qEmployeeId = QString::fromStdString(employeeId);

    if (qEmployeeId == QString::fromStdString(req->session()->get<std::string>("employee_id"))) {
      throw std::invalid_argument("Вы не можете удалить собственный аккаунт.");
    }

    bool success = DatabaseController::instance().deleteEmployee(qEmployeeId);
    if (!success) {
      throw std::runtime_error("Ошибка удаления сотрудника (убедитесь, что нет зависимых записей).");
    }

    std::string adminEmpId = req->session()->get<std::string>("employee_id");
    std::string adminUsername = req->session()->get<std::string>("user_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(adminUsername), QString::fromStdString(adminEmpId),
        "DELETE_EMPLOYEE", "Deleted Employee ID: " + qEmployeeId);

    Json::Value ret;
    ret["status"] = "success";
    auto res = HttpResponse::newHttpJsonResponse(ret);
    callback(res);

  } catch (const std::exception &e) {
    Json::Value ret;
    ret["error"] = e.what();
    auto res = HttpResponse::newHttpJsonResponse(ret);
    res->setStatusCode(k400BadRequest);
    callback(res);
  }
}

