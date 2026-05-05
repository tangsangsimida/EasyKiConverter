#include "KiCadLibTable.h"

#include "core/utils/KiCadTableUtils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTextStream>

namespace EasyKiConverter {

bool generateKiCadLibTable(const QString& tableType,
                           const QString& fileName,
                           const QString& libName,
                           const QString& libPath,
                           const QString& outputDir,
                           const QString& libraryDescription) {
    QString filePath = QDir(outputDir).filePath(fileName);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to create" << fileName << ":" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    QFileInfo libInfo(libPath);
    QString relativePath = libInfo.fileName();

    out << "(" << tableType << "\n";
    out << "  (version 7)\n";
    out << QString("  (lib (name \"%1\")(type \"KiCad\")(uri \"%2\")(options \"\")")
               .arg(KiCadTableUtils::escapeTableValue(libName), KiCadTableUtils::escapeTableValue(relativePath));
    if (!libraryDescription.isEmpty()) {
        out << QString("(descr \"%1\")").arg(KiCadTableUtils::escapeTableValue(libraryDescription));
    }
    out << ")\n";
    out << ")\n";

    file.close();
    qDebug() << "Generated" << fileName << ":" << filePath;
    return true;
}

}  // namespace EasyKiConverter
