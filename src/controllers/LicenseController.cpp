#include "controllers/LicenseController.h"

#include <drogon/drogon.h>

#include "database/DatabaseController.h"
#include "models/LicenseData.h"
#include "models/LicenseRecord.h"
#include "models/LicenseValidator.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QString>
#include <QVector>

using namespace drogon;

void LicenseController::generate(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  auto jsonPtr = req->getJsonObject();
  if (!jsonPtr) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  try {
    // Extract required parameters
    QString companyName =
        QString::fromStdString((*jsonPtr)["companyName"].asString());
    QString hardwareId =
        QString::fromStdString((*jsonPtr)["hardwareId"].asString());
    QString issueDate =
        QString::fromStdString((*jsonPtr)["issueDate"].asString());
    QString expiredDate =
        QString::fromStdString((*jsonPtr)["expiredDate"].asString());
    QString note = "";
    if (jsonPtr->isMember("note") && !(*jsonPtr)["note"].isNull()) {
      note = QString::fromStdString((*jsonPtr)["note"].asString());
    }

    if (companyName.isEmpty() || hardwareId.isEmpty() || issueDate.isEmpty() ||
        expiredDate.isEmpty()) {
      Json::Value error;
      error["error"] = "Все поля (название компании, Hardware ID, дата выдачи, "
                       "дата окончания) должны быть заполнены.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    QRegularExpression hwRegex("^[A-Za-z0-9\\-]+$");
    QRegularExpression companyRegex("^[a-zA-Zа-яА-ЯёЁ0-9\\s,.\\-&\"']+$");

    if (!hwRegex.match(hardwareId).hasMatch()) {
      Json::Value error;
      error["error"] =
          "Hardware ID может содержать только буквы, цифры и дефисы.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    if (!companyRegex.match(companyName).hasMatch()) {
      Json::Value error;
      error["error"] = "Название компании содержит недопустимые символы.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    QVector<QString> modules;
    const auto &jsonModules = (*jsonPtr)["modules"];
    if (jsonModules.isArray()) {
      for (const auto &mod : jsonModules) {
        modules.append(QString::fromStdString(mod.asString()));
      }
    }

    // Initialize business logic model
    LicenseData data;
    data.Init(companyName, hardwareId, issueDate, expiredDate, "", modules);

    // Generate cryptographic signature
    QString signature = LicenseValidator::GenerateHash(data);

    // Create record for database insertion
    LicenseRecord record;
    record.companyName = companyName;
    record.hardwareId = hardwareId;
    record.issueDate = QDate::fromString(issueDate, "dd.MM.yyyy");
    record.expiredDate = QDate::fromString(expiredDate, "dd.MM.yyyy");
    record.modules = modules.toList().join(", ");
    record.generatedAt = QDateTime::currentDateTime();
    record.signature = signature;
    record.note = note;

    // Validate dates before saving
    if (!record.issueDate.isValid() || !record.expiredDate.isValid()) {
      Json::Value error;
      error["error"] = "Неверный формат даты. Используйте дд.мм.гггг.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    // Persist to database
    std::string employeeId = req->session()->get<std::string>("employee_id");
    if (!DatabaseController::instance().saveLicense(
            record, QString::fromStdString(employeeId))) {
      throw std::runtime_error("Ошибка сохранения лицензии в базу данных.");
    }

    // Log the successful generation
    std::string username = req->session()->get<std::string>("user_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(employeeId),
        "GENERATE_LICENSE",
        "Company: " + companyName + ", HardwareID: " + hardwareId);

    // Send successful response
    Json::Value responseJson;
    responseJson["status"] = "success";
    responseJson["signature"] = signature.toStdString();

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

void LicenseController::getLicenses(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  try {
    int limit = 0;
    int offset = 0;

    std::string limitStr = req->getParameter("limit");
    if (!limitStr.empty()) {
      limit = std::stoi(limitStr);
    }

    std::string offsetStr = req->getParameter("offset");
    if (!offsetStr.empty()) {
      offset = std::stoi(offsetStr);
    }

    if (limit < 0)
      limit = 0;
    if (offset < 0)
      offset = 0;

    std::string role = req->session()->get<std::string>("role");
    QString filterEmpId = "";
    if (role == "junior_manager") {
      filterEmpId = QString::fromStdString(
          req->session()->get<std::string>("employee_id"));
    }

    QVector<LicenseRecord> records =
        DatabaseController::instance().loadAllLicenses(limit, offset,
                                                       filterEmpId);

    Json::Value responseJson(Json::arrayValue);

    for (const auto &record : records) {
      Json::Value item;
      item["id"] = record.signature.toStdString(); // Send signature as ID
      item["companyName"] = record.companyName.toStdString();
      item["hardwareId"] = record.hardwareId.toStdString();
      item["issueDate"] = record.issueDate.toString("dd.MM.yyyy").toStdString();
      item["expiredDate"] =
          record.expiredDate.toString("dd.MM.yyyy").toStdString();
      item["modules"] = record.modules.toStdString();
      // Format with timezone abbreviation (e.g., "2026-04-28 08:03:34 MSK")
      item["generatedAt"] =
          record.generatedAt.toString("yyyy-MM-dd HH:mm:ss t").toStdString();
      item["signature"] = record.signature.toStdString();
      item["note"] = record.note.toStdString();
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

void LicenseController::deleteLicense(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback, std::string id) {

  std::string role = req->session()->get<std::string>("role");
  if (role != "admin") {
    Json::Value error;
    error["error"] = "Недостаточно прав для удаления лицензии.";
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }

  std::string username = req->session()->get<std::string>("user_id");
  std::string empId = req->session()->get<std::string>("employee_id");

  if (DatabaseController::instance().deleteLicense(
          QString::fromStdString(id))) {
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(empId),
        "DELETE_LICENSE", "ID: " + QString::fromStdString(id));
    Json::Value ret;
    ret["status"] = "success";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
  } else {
    Json::Value error;
    error["error"] =
        "Не удалось удалить лицензию. Возможно, она не существует.";
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k404NotFound);
    callback(resp);
  }
}

void LicenseController::getModules(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  try {
    QVector<ModuleRecord> modules = DatabaseController::instance().getAllModules();

    Json::Value responseJson(Json::arrayValue);
    for (const auto &mod : modules) {
      Json::Value item;
      item["moduleName"]   = mod.moduleName.toStdString();
      item["displayLabel"] = mod.displayLabel.toStdString();
      item["parentModule"] = mod.parentModule.toStdString();
      item["sortOrder"]    = mod.sortOrder;
      item["isSelectable"] = mod.isSelectable;
      item["requiresDevice"] = mod.requiresDevice;
      item["dependencyGroup"] = mod.dependencyGroup.toStdString();
      item["isActive"] = mod.isActive;
      item["description"] = mod.description.toStdString();

      Json::Value reqWithArray(Json::arrayValue);
      for (const QString &rw : mod.requiredWith) {
        reqWithArray.append(rw.toStdString());
      }
      item["requiredWith"] = reqWithArray;

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
