#include "controllers/ConfigurationController.h"
#include "database/DatabaseController.h"

#include <drogon/drogon.h>

using namespace drogon;

// ─── Helper: check admin role ────────────────────────────────────────────────
static bool requireAdmin(const HttpRequestPtr &req,
                          std::function<void(const HttpResponsePtr &)> &callback) {
  std::string role = req->session()->get<std::string>("role");
  if (role != "admin") {
    Json::Value err;
    err["error"] = "Доступ запрещён: требуется роль Administrator.";
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return false;
  }
  return true;
}

// ─── GET /api/configurations ─────────────────────────────────────────────────
void ConfigurationController::getConfigurations(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    QVector<ConfigurationRecord> configs =
        DatabaseController::instance().getAllConfigurations();

    Json::Value responseJson(Json::arrayValue);
    for (const auto &cfg : configs) {
      Json::Value item;
      item["configId"]    = cfg.configId;
      item["configName"]  = cfg.configName.toStdString();
      item["description"] = cfg.description.toStdString();

      Json::Value mods(Json::arrayValue);
      for (const auto &m : cfg.modules)
        mods.append(m.toStdString());
      item["modules"] = mods;

      responseJson.append(item);
    }

    auto resp = HttpResponse::newHttpJsonResponse(responseJson);
    callback(resp);
  } catch (const std::exception &e) {
    Json::Value err;
    err["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}

// ─── POST /api/configurations ────────────────────────────────────────────────
void ConfigurationController::addConfiguration(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  if (!requireAdmin(req, callback)) return;

  auto jsonPtr = req->getJsonObject();
  if (!jsonPtr) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  try {
    QString name = QString::fromStdString((*jsonPtr)["configName"].asString()).trimmed();
    if (name.isEmpty()) {
      Json::Value err;
      err["error"] = "Название конфигурации не может быть пустым.";
      auto resp = HttpResponse::newHttpJsonResponse(err);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    QString description = QString::fromStdString((*jsonPtr)["description"].asString());

    QStringList modules;
    const auto &jsonMods = (*jsonPtr)["modules"];
    if (jsonMods.isArray()) {
      for (const auto &m : jsonMods)
        modules.append(QString::fromStdString(m.asString()));
    }

    if (!DatabaseController::instance().addConfiguration(name, description, modules)) {
      throw std::runtime_error("Ошибка сохранения конфигурации в базу данных.");
    }

    std::string username = req->session()->get<std::string>("user_id");
    std::string empId    = req->session()->get<std::string>("employee_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(empId),
        "ADD_CONFIGURATION", "Config: " + name);

    Json::Value ret;
    ret["status"] = "success";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);

  } catch (const std::exception &e) {
    Json::Value err;
    err["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}

// ─── PUT /api/configurations/{id} ────────────────────────────────────────────
void ConfigurationController::updateConfiguration(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string id) {

  if (!requireAdmin(req, callback)) return;

  auto jsonPtr = req->getJsonObject();
  if (!jsonPtr) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  try {
    int configId = std::stoi(id);
    QString name = QString::fromStdString((*jsonPtr)["configName"].asString()).trimmed();
    if (name.isEmpty()) {
      Json::Value err;
      err["error"] = "Название конфигурации не может быть пустым.";
      auto resp = HttpResponse::newHttpJsonResponse(err);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    QString description = QString::fromStdString((*jsonPtr)["description"].asString());

    QStringList modules;
    const auto &jsonMods = (*jsonPtr)["modules"];
    if (jsonMods.isArray()) {
      for (const auto &m : jsonMods)
        modules.append(QString::fromStdString(m.asString()));
    }

    if (!DatabaseController::instance().updateConfiguration(configId, name, description, modules)) {
      throw std::runtime_error("Ошибка обновления конфигурации.");
    }

    std::string username = req->session()->get<std::string>("user_id");
    std::string empId    = req->session()->get<std::string>("employee_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(empId),
        "UPDATE_CONFIGURATION", "ConfigID: " + QString::fromStdString(id) + " -> " + name);

    Json::Value ret;
    ret["status"] = "success";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);

  } catch (const std::exception &e) {
    Json::Value err;
    err["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}

// ─── DELETE /api/configurations/{id} ─────────────────────────────────────────
void ConfigurationController::deleteConfiguration(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string id) {

  if (!requireAdmin(req, callback)) return;

  try {
    int configId = std::stoi(id);
    if (!DatabaseController::instance().deleteConfiguration(configId)) {
      Json::Value err;
      err["error"] = "Не удалось удалить конфигурацию. Возможно, она не существует.";
      auto resp = HttpResponse::newHttpJsonResponse(err);
      resp->setStatusCode(k404NotFound);
      callback(resp);
      return;
    }

    std::string username = req->session()->get<std::string>("user_id");
    std::string empId    = req->session()->get<std::string>("employee_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(empId),
        "DELETE_CONFIGURATION", "ConfigID: " + QString::fromStdString(id));

    Json::Value ret;
    ret["status"] = "success";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);

  } catch (const std::exception &e) {
    Json::Value err;
    err["error"] = e.what();
    auto resp = HttpResponse::newHttpJsonResponse(err);
    resp->setStatusCode(k500InternalServerError);
    callback(resp);
  }
}
