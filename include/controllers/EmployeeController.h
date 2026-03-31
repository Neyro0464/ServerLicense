#pragma once
#include <drogon/HttpController.h>

class EmployeeController : public drogon::HttpController<EmployeeController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(EmployeeController::getEmployees, "/api/employees", drogon::Get, "AuthFilter");
  ADD_METHOD_TO(EmployeeController::addEmployee, "/api/employees", drogon::Post, "AuthFilter");
  ADD_METHOD_TO(EmployeeController::updateEmployee, "/api/employees/{1}", drogon::Put, "AuthFilter");
  ADD_METHOD_TO(EmployeeController::deleteEmployee, "/api/employees/{1}", drogon::Delete, "AuthFilter");
  METHOD_LIST_END

  void getEmployees(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void addEmployee(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback);
  void updateEmployee(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback, std::string employeeId);
  void deleteEmployee(const drogon::HttpRequestPtr &req, std::function<void(const drogon::HttpResponsePtr &)> &&callback, std::string employeeId);
};
