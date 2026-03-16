#ifndef LICENSEFILEJSON_H
#define LICENSEFILEJSON_H
#include "ILicenseFile.h"
#include "LicenseLib_Export.h"

class LICENSE_API LicenseFileJson : public ILicenseFile
{
public:
    LicenseFileJson() = default;
    ~LicenseFileJson() override = default;

    LicenseData ReadFile (const QString& fileName) override;
    QString LoadBin(const QString& fileName) override;

    // Debug output control
    static void SetDebugOutputState(bool state);
    static bool GetDebugOutputState();

private:
    void CreateFile(const QString& fileName, const LicenseData& data) override;
    void SaveBin(const QString& fileName, const QString& data) override;

    static bool s_outputState;
};
#endif // LICENSEFILEJSON_H
