#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class CompanyController : public drogon::HttpController<CompanyController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(CompanyController::getCompanies, "/api/companies", Get,
                "AuthFilter");
  ADD_METHOD_TO(CompanyController::addCompany, "/api/companies", Post,
                "AuthFilter");
  ADD_METHOD_TO(CompanyController::updateCompany, "/api/companies/{1}", Put,
                "AuthFilter");
  ADD_METHOD_TO(CompanyController::deleteCompany, "/api/companies/{1}", Delete,
                "AuthFilter");
  METHOD_LIST_END

  void getCompanies(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);

  void addCompany(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback);

  void updateCompany(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     std::string companyName);

  void deleteCompany(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     std::string companyName);
};
