#ifndef LICENSERECORD_H
#define LICENSERECORD_H

#include <QDate>
#include <QDateTime>
#include <QString>

struct LicenseRecord {
  QString signature;
  QString companyName;
  QString hardwareId;
  QDate issueDate;
  QDate expiredDate;
  QString modules;
  QDateTime generatedAt;
  QString note;
};

#endif // LICENSERECORD_H
