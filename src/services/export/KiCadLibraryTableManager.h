#ifndef KICADLIBRARYTABLEMANAGER_H
#define KICADLIBRARYTABLEMANAGER_H

#include <QString>

namespace EasyKiConverter {

class KiCadLibraryTableManager {
public:
    enum class RegistrationResult {
        Registered,
        ImportGuideWritten,
        Failed,
    };

    static RegistrationResult registerSymbolLibrary(const QString& outputDir,
                                                    const QString& libName,
                                                    const QString& libFilePath,
                                                    const QString& libraryDescription);

    static RegistrationResult registerFootprintLibrary(const QString& outputDir,
                                                       const QString& libName,
                                                       const QString& libDirPath,
                                                       const QString& libraryDescription);

private:
    static bool writeImportGuide(const QString& outputDir);
    static QString findKiCadProjectRoot(const QString& outputDir);

    static bool updateProjectLibraryTable(const QString& projectRoot,
                                          const QString& tableFileName,
                                          const QString& tableRootName,
                                          const QString& libName,
                                          const QString& uri,
                                          const QString& libraryDescription);

    static QString makeLibraryEntry(const QString& libName, const QString& uri, const QString& libraryDescription);
    static QString makeProjectUri(const QString& projectRoot, const QString& libraryPath);
};

}  // namespace EasyKiConverter

#endif  // KICADLIBRARYTABLEMANAGER_H
