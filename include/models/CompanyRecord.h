#ifndef COMPANYRECORD_H
#define COMPANYRECORD_H

#include <QDateTime>
#include <QString>

struct CompanyRecord {
  QString companyName;
  QDateTime dateAdded;
  QString city;
  QString contacts; // JSON string
  QString note;     // Optional note
};

#endif // COMPANYRECORD_H
