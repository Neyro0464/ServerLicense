#ifndef ILICENSEFILE_H
#define ILICENSEFILE_H

#include <QVector>
#include <QString>
#include <QCryptographicHash>

#include "models/LicenseData.h"
#include "LicenseLib_Export.h"


class LICENSE_API ILicenseFile
{
public:
    // ILicenseFile() = delete;

    virtual LicenseData ReadFile (const QString& fileName) = 0;
    virtual QString LoadBin(const QString& fileName) = 0;


    virtual ~ILicenseFile() = default;

private:
    virtual void CreateFile(const QString& fileName, const LicenseData& data) = 0;
    virtual void SaveBin(const QString& fileName, const QString& data) = 0;
};

#endif // ILICENSEFILE_H
