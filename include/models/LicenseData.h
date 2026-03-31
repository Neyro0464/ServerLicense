#ifndef LICENSEDATA_H
#define LICENSEDATA_H

#include "LicenseLib_Export.h"
#include <QMap>
#include <QString>
#include <QVector>

/**
 * @brief Class for license data
 * All data about license in this class
 * Used for validation and safely work with data
 */
class LICENSE_API LicenseData {
public:
  LicenseData() = default;

  bool Init(const QString &companyName, const QString &hardwareId,
            const QString &issueDate, const QString &expiredDate,
            const QString &signature, QVector<QString> modules);

  void PrintDataToConsole() const;
  QString GetDataToString() const;

  bool SetSignature(QString sign);

  // Debug output control
  static void SetDebugOutputState(bool state);
  static bool GetDebugOutputState();

  QString GetCompanyName() const { return m_companyName; };
  QString GetIssueDate() const { return m_issueDate; };
  QString GetExpiredData() const { return m_expiredDate; };
  QString GetSignature() const { return m_signature; };
  QString GetHardwareId() const { return m_hardware_id; };
  QVector<QString> GetModules() const { return m_modules; };

private:
  QString m_companyName;
  QString m_issueDate;
  QString m_expiredDate;
  QString m_signature;
  QString m_hardware_id;
  QVector<QString> m_modules;

  static bool s_outputState;

  // bool CheckModulesCorrectness() const;
};

#endif // LICENSEDATA_H
