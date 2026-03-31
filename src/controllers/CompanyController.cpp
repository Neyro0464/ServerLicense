#include "controllers/CompanyController.h"
#include "database/DatabaseController.h"

void CompanyController::getCompanies(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {

  try {
    QVector<CompanyRecord> companies =
        DatabaseController::instance().getAllCompanies();
    Json::Value responseJson(Json::arrayValue);

    for (const auto &comp : companies) {
      Json::Value item;
      item["companyName"] = comp.companyName.toStdString();
      item["dateAdded"] =
          comp.dateAdded.toString("yyyy-MM-dd HH:mm:ss").toStdString();
      item["city"] = comp.city.toStdString();
      item["contacts"] = comp.contacts.toStdString();
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

void CompanyController::addCompany(
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
    // Junior managers are now allowed to add companies.


    QString companyName =
        QString::fromStdString((*jsonPtr)["companyName"].asString());

    QString city = "";
    if (jsonPtr->isMember("city")) {
      city = QString::fromStdString((*jsonPtr)["city"].asString());
    }

    // Optional contacts JSON string
    QString contacts = "{}";
    if (jsonPtr->isMember("contacts") &&
        !(*jsonPtr)["contacts"].asString().empty()) {
      contacts = QString::fromStdString((*jsonPtr)["contacts"].asString());
    }

    if (companyName.isEmpty()) {
      Json::Value error;
      error["error"] = "Company name is strictly required";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    CompanyRecord record;
    record.companyName = companyName;
    record.city = city;
    record.contacts = contacts;

    if (!DatabaseController::instance().addCompany(record)) {
      throw std::runtime_error("Error saving company to database");
    }

    std::string username = req->session()->get<std::string>("user_id");
    std::string empId = req->session()->get<std::string>("employee_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(empId),
        "ADD_COMPANY", "Company: " + companyName);

    Json::Value responseJson;
    responseJson["status"] = "success";
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

void CompanyController::updateCompany(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string companyNameStr) {

  auto jsonPtr = req->getJsonObject();
  if (!jsonPtr) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  try {
    std::string role = req->session()->get<std::string>("role");
    if (role == "junior_manager") {
      Json::Value error;
      error["error"] = "Access denied: Junior managers cannot update companies.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k403Forbidden);
      callback(resp);
      return;
    }

    QString companyName = QString::fromStdString(drogon::utils::urlDecode(companyNameStr));

    QString newName = companyName; // Default to old if not specified, though usually we might update it
    if (jsonPtr->isMember("companyName")) {
      newName = QString::fromStdString((*jsonPtr)["companyName"].asString());
    }
    
    QString city = "";
    if (jsonPtr->isMember("city")) {
      city = QString::fromStdString((*jsonPtr)["city"].asString());
    }

    QString contacts = "{}";
    if (jsonPtr->isMember("contacts")) {
      contacts = QString::fromStdString((*jsonPtr)["contacts"].asString());
    }

    CompanyRecord record;
    record.companyName = newName;
    record.city = city;
    record.contacts = contacts;

    if (!DatabaseController::instance().updateCompany(companyName, record)) {
      throw std::runtime_error("Error updating company in database");
    }

    std::string username = req->session()->get<std::string>("user_id");
    std::string empId = req->session()->get<std::string>("employee_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(empId),
        "UPDATE_COMPANY", "Company: " + companyName + " -> " + newName);

    Json::Value responseJson;
    responseJson["status"] = "success";
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

void CompanyController::deleteCompany(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    std::string companyNameStr) {

  try {
    std::string role = req->session()->get<std::string>("role");
    if (role == "junior_manager") {
      Json::Value error;
      error["error"] = "Access denied: Junior managers cannot delete companies.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k403Forbidden);
      callback(resp);
      return;
    }

    QString companyName = QString::fromStdString(drogon::utils::urlDecode(companyNameStr));

    if (!DatabaseController::instance().deleteCompany(companyName)) {
      Json::Value error;
      error["error"] = "Cannot delete company. It may not exist or has associated licenses.";
      auto resp = HttpResponse::newHttpJsonResponse(error);
      resp->setStatusCode(k400BadRequest);
      callback(resp);
      return;
    }

    std::string username = req->session()->get<std::string>("user_id");
    std::string empId = req->session()->get<std::string>("employee_id");
    DatabaseController::instance().logAction(
        QString::fromStdString(username), QString::fromStdString(empId),
        "DELETE_COMPANY", "Deleted Company: " + companyName);

    Json::Value responseJson;
    responseJson["status"] = "success";
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
