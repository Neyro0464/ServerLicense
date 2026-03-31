#include "models/LicenseData.h"
#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QVector>

// Initialize static member
bool LicenseData::s_outputState = true;

void LicenseData::SetDebugOutputState(bool state) { s_outputState = state; }

bool LicenseData::GetDebugOutputState() { return s_outputState; }

bool LicenseData::Init(const QString &companyName, const QString &hardwareId,
                       const QString &issueDate, const QString &expiredDate,
                       const QString &signature, QVector<QString> modules) {
  if (companyName.isEmpty() || hardwareId.isEmpty() || issueDate.isEmpty() ||
      expiredDate.isEmpty() || modules.isEmpty()) {
    if (s_outputState) {
      qDebug() << "[LicenseData]: All params must be not empty";
    }
    return false;
  }

  QDateTime tm_issueDate = QDateTime::fromString(issueDate, "dd.MM.yyyy");
  QDateTime tm_expiredDate = QDateTime::fromString(expiredDate, "dd.MM.yyyy");

  if (tm_issueDate > tm_expiredDate) {
    if (s_outputState) {
      qDebug()
          << "[LicenseData]: Expired date can't be earlier than issue date";
    }
    return false;
  }

  m_companyName = companyName;
  m_hardware_id = hardwareId;
  m_issueDate = issueDate;
  m_expiredDate = expiredDate;
  m_signature = signature;
  m_modules = modules;

  if (s_outputState) {
    qDebug() << "[LicenseData]: Init complete successfully!";
  }
  return true;
}

void LicenseData::PrintDataToConsole() const {
  if (!s_outputState) {
    return;
  }
  qDebug() << "Company name:" << m_companyName << "\nIssue Date:" << m_issueDate
           << "\nExpired Date:" << m_expiredDate
           << "\nSignature:" << m_signature << "\nConnected modules:";
  for (const auto &module : m_modules) {
    qDebug() << " " << module;
  }
}

QString LicenseData::GetDataToString() const {
  QString res = "Company name: " + m_companyName +
                " Issue Date: " + m_issueDate +
                " Expired Date: " + m_expiredDate +
                " Signature: " + m_signature + " Connected modules:";
  for (const auto &module : m_modules) {
    res += " " + module;
  }
  return res;
}

bool LicenseData::SetSignature(QString sign) {
  if (sign.isEmpty()) {
    if (s_outputState) {
      qDebug() << "[LicenseData]: SetSignature is empty";
    }
    return false;
  }
  m_signature = sign;
  return true;
}
