#ifndef LICENSERECORD_H
#define LICENSERECORD_H

#include <QDate>
#include <QDateTime>
#include <QString>

struct LicenseRecord {
  int id = 0;
  QString companyName;
  QString hardwareId;
  QDate issueDate;
  QDate expiredDate;
  QString modules;
  QDateTime generatedAt;
  QString signature;
};

#endif // LICENSERECORD_H
