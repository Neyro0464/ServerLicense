#ifndef LICENSEVALIDATOR_H
#define LICENSEVALIDATOR_H
#include "LicenseData.h"
#include "LicenseLib_Export.h"
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QCryptographicHash>

class LICENSE_API LicenseValidator
{
public:
    explicit LicenseValidator(const LicenseData& licenseData);

    // Проверка лицензии
    bool VerifyLicense(const QString& hash);

    // Проверка активности лицензии из файла
    bool IsLicenseActive();

    // Оставшееся количество дней до истечения лицензии
    int GetDaysRemaining() const;

    // Геттер для данных лицензии
    LicenseData GetLicenseData() const { return m_licenseData; }

    static QString GetVersion();
    static QString GetFullVersion();

    // Debug output control
    static void SetDebugOutputState(bool state);
    static bool GetDebugOutputState();

    static QString GenerateHash(LicenseData data, QCryptographicHash::Algorithm hashAlgorythm = QCryptographicHash::Algorithm::Md5);

private:
    // Проверка срока действия даты
    bool IsDateActive(const QString& dateString) const;
    LicenseData m_licenseData;

    static bool s_outputState;
};
#endif // LICENSEVALIDATOR_H
