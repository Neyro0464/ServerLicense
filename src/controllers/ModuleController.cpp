#include "controllers/ModuleController.h"
#include "database/DatabaseController.h"
#include <drogon/drogon.h>

void ModuleController::addModule(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  // Check admin role
  std::string role = req->session()->get<std::string>("role");
  if (role != "admin") {
    Json::Value error;
    error["error"] = "Недостаточно прав для добавления модулей.";
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }

  auto jsonPtr = req->getJsonObject();
  if (!jsonPtr) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  try {
    QString moduleName = QString::fromStdString((*jsonPtr)["moduleName"].asString());
    QString displayLabel = QString::fromStdString((*jsonPtr)["displayLabel"].asString());
    QString description = QString::fromStdString((*jsonPtr)["description"].asString());
    QString parentModule = QString::fromStdString((*jsonPtr)["parentModule"].asString());
    int sortOrder = (*jsonPtr)["sortOrder"].asInt();
    bool isSelectable = (*jsonPtr)["isSelectable"].asBool();
    bool requiresDevice = (*jsonPtr)["requiresDevice"].asBool();
    QString dependencyGroup = QString::fromStdString((*jsonPtr)["dependencyGroup"].asString());

    QStringList requiredWith;
    const auto &reqWithJson = (*jsonPtr)["requiredWith"];
    if (reqWithJson.isArray()) {
      for (const auto &item : reqWithJson) {
        requiredWith.append(QString::fromStdString(item.asString()));
      }
    }

    if (moduleName.isEmpty()) {
      Json::Value error;
      error["error"] = "Название модуля обязательно.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    if (DatabaseController::instance().addModule(
            moduleName, displayLabel, description, parentModule,
            sortOrder, isSelectable, requiresDevice, dependencyGroup, requiredWith)) {

      std::string username = req->session()->get<std::string>("user_id");
      std::string empId = req->session()->get<std::string>("employee_id");
      DatabaseController::instance().logAction(
          QString::fromStdString(username), QString::fromStdString(empId),
          "ADD_MODULE", "Module: " + moduleName);

      Json::Value ret;
      ret["status"] = "success";
      auto resp = HttpResponse::newHttpJsonResponse(ret);
      callback(resp);
    } else {
      Json::Value error;
      error["error"] = "Не удалось добавить модуль. Возможно, он уже существует.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k500InternalServerError);
      callback(resp);
    }
  } catch (const std::exception &e) {
    Json::Value error;
    error["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}

void ModuleController::updateModule(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string name) {

  // Check admin role
  std::string role = req->session()->get<std::string>("role");
  if (role != "admin") {
    Json::Value error;
    error["error"] = "Недостаточно прав для редактирования модулей.";
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }

  auto jsonPtr = req->getJsonObject();
  if (!jsonPtr) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  try {
    QString moduleName = QString::fromStdString(name);
    QString displayLabel = QString::fromStdString((*jsonPtr)["displayLabel"].asString());
    QString description = QString::fromStdString((*jsonPtr)["description"].asString());
    QString parentModule = QString::fromStdString((*jsonPtr)["parentModule"].asString());
    int sortOrder = (*jsonPtr)["sortOrder"].asInt();
    bool isSelectable = (*jsonPtr)["isSelectable"].asBool();
    bool requiresDevice = (*jsonPtr)["requiresDevice"].asBool();
    QString dependencyGroup = QString::fromStdString((*jsonPtr)["dependencyGroup"].asString());
    bool isActive = (*jsonPtr).get("isActive", true).asBool();

    QStringList requiredWith;
    const auto &reqWithJson = (*jsonPtr)["requiredWith"];
    if (reqWithJson.isArray()) {
      for (const auto &item : reqWithJson) {
        requiredWith.append(QString::fromStdString(item.asString()));
      }
    }

    if (DatabaseController::instance().updateModule(
            moduleName, displayLabel, description, parentModule,
            sortOrder, isSelectable, requiresDevice, dependencyGroup, requiredWith, isActive)) {

      std::string username = req->session()->get<std::string>("user_id");
      std::string empId = req->session()->get<std::string>("employee_id");
      DatabaseController::instance().logAction(
          QString::fromStdString(username), QString::fromStdString(empId),
          "UPDATE_MODULE", "Module: " + moduleName);

      Json::Value ret;
      ret["status"] = "success";
      auto resp = HttpResponse::newHttpJsonResponse(ret);
      callback(resp);
    } else {
      Json::Value error;
      error["error"] = "Не удалось обновить модуль.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k404NotFound);
      callback(resp);
    }
  } catch (const std::exception &e) {
    Json::Value error;
    error["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}

void ModuleController::deleteModule(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string name) {

  // Check admin role
  std::string role = req->session()->get<std::string>("role");
  if (role != "admin") {
    Json::Value error;
    error["error"] = "Недостаточно прав для удаления модулей.";
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }

  QString moduleName = QString::fromStdString(name);

  if (DatabaseController::instance().deleteModule(moduleName)) {
    std::string username = req->session()->get<std::string>("user_id");
    std::string empId = req->session()->get<std::string>("employee_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(empId),
        "DELETE_MODULE", "Module: " + moduleName);

    Json::Value ret;
    ret["status"] = "success";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
  } else {
    Json::Value error;
    error["error"] = "Не удалось удалить модуль. Возможно, он используется в лицензиях или имеет дочерние модули.";
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k400BadRequest);
    callback(resp);
  }
}
