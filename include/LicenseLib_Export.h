#ifndef LICENSELIB_EXPORT_H
#define LICENSELIB_EXPORT_H

#include <QtGlobal>

// Если мы собираем саму библиотеку
#ifdef BUILDING_LICENSE_LIB
#define LICENSE_API Q_DECL_EXPORT
// Если мы используем библиотеку
#else
#define LICENSE_API Q_DECL_IMPORT
#endif

#endif // LICENSELIB_EXPORT_H
