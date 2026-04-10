#include "ExportWorkerHelpers.h"

#include <QDir>
#include <QFile>

// ExportOptions is defined in ExportProgress.h which is included by the header

namespace EasyKiConverter {

QString ExportWorkerHelpers::defaultOutputDir(const QString& subdir) {
    return QDir::currentPath() + QStringLiteral("/export/") + subdir;
}

QString ExportWorkerHelpers::ensureOutputDir(const struct ExportOptions& options, const QString& subdir) {
    QString outputDir = options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = defaultOutputDir(subdir);
    }
    QDir dir;
    if (!dir.mkpath(outputDir)) {
        return QString();
    }
    return outputDir;
}

QString ExportWorkerHelpers::buildFilePath(const QString& componentId,
                                           const QString& outputDir,
                                           const QString& fileExtension) {
    return outputDir + QStringLiteral("/") + componentId + fileExtension;
}

bool ExportWorkerHelpers::shouldSkipExisting(const QString& filePath, const struct ExportOptions& options) {
    return QFile::exists(filePath) && !options.overwriteExistingFiles;
}

}  // namespace EasyKiConverter
