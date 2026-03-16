#include "LicenseValidator.h"
#include "LicenseFileJson.h"
#include "version.h"
#include <QByteArray>
#include <QDebug>
#include <QVector>

// Initialize static member
bool LicenseValidator::s_outputState = true;

void LicenseValidator::SetDebugOutputState(bool state) {
  s_outputState = state;
}

bool LicenseValidator::GetDebugOutputState() { return s_outputState; }

QString LicenseValidator::GetVersion() {
  QString res = QString("v%1.%2 %3").arg(VER_MAJOR, VER_BUILD).arg(VER_HASH);
  return res;
}

QString LicenseValidator::GetFullVersion() {
  QString res = QString("v%1.%2 %3 %4 (%5)")
                    .arg(VER_MAJOR)
                    .arg(VER_BUILD)
                    .arg(VER_BRANCH)
                    .arg(VER_HASH)
                    .arg(COMMIT_DATE);
  return res;
}

LicenseValidator::LicenseValidator(const LicenseData &licenseData)
    : m_licenseData(licenseData) {}

QString
LicenseValidator::GenerateHash(LicenseData data,
                               QCryptographicHash::Algorithm hashAlgorythm) {
  QCryptographicHash hash(hashAlgorythm);

  QVector<QString> modules = data.GetModules();
  QString modulesNames{};
  for (const auto &module : std::as_const(modules)) {
    modulesNames.append(module);
  }

  hash.addData(QByteArray(QString(data.GetCompanyName() + data.GetHardwareId() +
                                  data.GetIssueDate() + data.GetExpiredData() +
                                  "manOfHonor" + modulesNames)
                              .toLatin1()));
  return QString(hash.result().toHex()).toLatin1();
}

bool LicenseValidator::VerifyLicense(const QString &hash) {
  QString generatedHash =
      GenerateHash(m_licenseData, QCryptographicHash::Algorithm::Md5);

  if (s_outputState) {
    qDebug() << generatedHash;
  }

  return (hash == generatedHash);
}

bool LicenseValidator::IsLicenseActive() {
  if (!VerifyLicense(m_licenseData.GetSignature())) {
    return false;
  }
  return IsDateActive(m_licenseData.GetExpiredData());
}

int LicenseValidator::GetDaysRemaining() const {
  QDateTime expiredDate =
      QDateTime::fromString(m_licenseData.GetExpiredData(), "dd.MM.yyyy");
  QDateTime currentDate = QDateTime::currentDateTime();
  if (currentDate > expiredDate) {
    return 0;
  }
  return currentDate.daysTo(expiredDate);
}

bool LicenseValidator::IsDateActive(const QString &dateString) const {
  QDateTime expiredDate = QDateTime::fromString(dateString, "dd.MM.yyyy");
  QDateTime currentDate = QDateTime::currentDateTime();
  return (currentDate <= expiredDate);
}
