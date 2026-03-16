#include "LicenseController.h"

#include <drogon/drogon.h>

#include "DatabaseController.h"
#include "LicenseData.h"
#include "LicenseRecord.h"
#include "LicenseValidator.h"

#include <QDate>
#include <QDateTime>
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

    if (companyName.isEmpty() || hardwareId.isEmpty() || issueDate.isEmpty() ||
        expiredDate.isEmpty()) {
      Json::Value error;
      error["error"] = "All fields (companyName, hardwareId, issueDate, "
                       "expiredDate) must be non-empty.";
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

    // Validate dates before saving
    if (!record.issueDate.isValid() || !record.expiredDate.isValid()) {
      Json::Value error;
      error["error"] = "Некорректный формат даты";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    // Persist to database
    if (!DatabaseController::instance().saveLicense(record)) {
      throw std::runtime_error("Ошибка записи в базу данных");
    }

    // Log the successful generation
    std::string username = req->session()->get<std::string>("user_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), "GENERATE_LICENSE",
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

    QVector<LicenseRecord> records =
        DatabaseController::instance().loadAllLicenses(limit, offset);

    Json::Value responseJson(Json::arrayValue);

    for (const auto &record : records) {
      Json::Value item;
      item["id"] = record.id;
      item["companyName"] = record.companyName.toStdString();
      item["hardwareId"] = record.hardwareId.toStdString();
      item["issueDate"] = record.issueDate.toString("dd.MM.yyyy").toStdString();
      item["expiredDate"] =
          record.expiredDate.toString("dd.MM.yyyy").toStdString();
      item["modules"] = record.modules.toStdString();
      item["generatedAt"] =
          record.generatedAt.toString("yyyy-MM-dd HH:mm:ss").toStdString();
      item["signature"] = record.signature.toStdString();
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
    std::function<void(const HttpResponsePtr &)> &&callback, int id) {

  std::string username = req->session()->get<std::string>("user_id");
  if (username != "admin") {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k403Forbidden);
    callback(resp);
    return;
  }

  if (DatabaseController::instance().deleteLicense(id)) {
    DatabaseController::instance().logAction(QString::fromStdString(username),
                                             "DELETE_LICENSE",
                                             "ID: " + QString::number(id));
    Json::Value ret;
    ret["status"] = "success";
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
  } else {
    Json::Value error;
    error["error"] = "Failed to delete license. It may not exist.";
    auto resp = HttpResponse::newHttpJsonResponse(error);
    resp->setStatusCode(k404NotFound);
    callback(resp);
  }
}
