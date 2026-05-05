#include "KiCadLibraryTableManager.h"

#include "core/utils/KiCadTableUtils.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

namespace EasyKiConverter {

KiCadLibraryTableManager::RegistrationResult KiCadLibraryTableManager::registerSymbolLibrary(
    const QString& outputDir,
    const QString& libName,
    const QString& libFilePath,
    const QString& libraryDescription) {
    const QString projectRoot = findKiCadProjectRoot(outputDir);
    if (projectRoot.isEmpty()) {
        return writeImportGuide(outputDir) ? RegistrationResult::ImportGuideWritten : RegistrationResult::Failed;
    }

    const QString uri = makeProjectUri(projectRoot, libFilePath);
    return updateProjectLibraryTable(projectRoot,
                                     QStringLiteral("sym-lib-table"),
                                     QStringLiteral("sym_lib_table"),
                                     libName,
                                     uri,
                                     libraryDescription)
               ? RegistrationResult::Registered
               : RegistrationResult::Failed;
}

KiCadLibraryTableManager::RegistrationResult KiCadLibraryTableManager::registerFootprintLibrary(
    const QString& outputDir,
    const QString& libName,
    const QString& libDirPath,
    const QString& libraryDescription) {
    const QString projectRoot = findKiCadProjectRoot(outputDir);
    if (projectRoot.isEmpty()) {
        return writeImportGuide(outputDir) ? RegistrationResult::ImportGuideWritten : RegistrationResult::Failed;
    }

    const QString uri = makeProjectUri(projectRoot, libDirPath);
    return updateProjectLibraryTable(projectRoot,
                                     QStringLiteral("fp-lib-table"),
                                     QStringLiteral("fp_lib_table"),
                                     libName,
                                     uri,
                                     libraryDescription)
               ? RegistrationResult::Registered
               : RegistrationResult::Failed;
}

bool KiCadLibraryTableManager::writeImportGuide(const QString& outputDir) {
    QDir dir(outputDir);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        qWarning() << "KiCadLibraryTableManager: Failed to create output directory for import guide:" << outputDir;
        return false;
    }

    const QString guidePath = dir.filePath(QStringLiteral("KiCad_IMPORT.md"));
    QSaveFile file(guidePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "KiCadLibraryTableManager: Failed to open import guide:" << guidePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "# KiCad Library Import\n\n";
    out << "EasyKiConverter did not find a KiCad project (`*.kicad_pro`) in this output path or its parent "
           "directories, so it did not modify any project library table.\n\n";
    out << "To show library descriptions in KiCad, add the exported libraries to the target project's library "
           "tables or to KiCad's global library tables.\n\n";

    const QString symTablePath = dir.filePath(QStringLiteral("sym-lib-table"));
    if (QFileInfo::exists(symTablePath)) {
        out << "## Symbol Library Entry\n\n";
        out << "Use the entry from:\n\n";
        out << "```text\n" << symTablePath << "\n```\n\n";
    }

    const QString fpTablePath = dir.filePath(QStringLiteral("fp-lib-table"));
    if (QFileInfo::exists(fpTablePath)) {
        out << "## Footprint Library Entry\n\n";
        out << "Use the entry from:\n\n";
        out << "```text\n" << fpTablePath << "\n```\n\n";
    }

    out << "Project-specific KiCad library tables are named `sym-lib-table` and `fp-lib-table` and live next to the "
           "project's `.kicad_pro` file.\n";

    if (!file.commit()) {
        qWarning() << "KiCadLibraryTableManager: Failed to commit import guide:" << guidePath;
        return false;
    }

    return true;
}

QString KiCadLibraryTableManager::findKiCadProjectRoot(const QString& outputDir) {
    QDir dir(QFileInfo(outputDir).absoluteFilePath());
    if (!dir.exists()) {
        return QString();
    }

    while (true) {
        const QStringList projectFiles = dir.entryList(QStringList(QStringLiteral("*.kicad_pro")), QDir::Files);
        if (!projectFiles.isEmpty()) {
            return dir.absolutePath();
        }

        if (!dir.cdUp()) {
            break;
        }
    }

    return QString();
}

bool KiCadLibraryTableManager::updateProjectLibraryTable(const QString& projectRoot,
                                                         const QString& tableFileName,
                                                         const QString& tableRootName,
                                                         const QString& libName,
                                                         const QString& uri,
                                                         const QString& libraryDescription) {
    QDir projectDir(projectRoot);
    const QString tablePath = projectDir.filePath(tableFileName);

    QString content;
    QFile existingFile(tablePath);
    if (existingFile.exists()) {
        if (!existingFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "KiCadLibraryTableManager: Failed to read library table:" << tablePath;
            return false;
        }
        QTextStream in(&existingFile);
        in.setEncoding(QStringConverter::Utf8);
        content = in.readAll();
    }

    if (content.trimmed().isEmpty()) {
        content = QStringLiteral("(%1\n  (version 7)\n)\n").arg(tableRootName);
    }

    QStringList lines = content.split('\n');
    const QRegularExpression entryRegex(
        QStringLiteral(R"(^\s*\(lib\s+\(name\s+"?%1"?\))").arg(QRegularExpression::escape(libName)));

    QStringList filteredLines;
    filteredLines.reserve(lines.size() + 1);
    for (const QString& line : lines) {
        if (!entryRegex.match(line).hasMatch()) {
            filteredLines.append(line);
        }
    }

    int insertIndex = filteredLines.size();
    while (insertIndex > 0 && filteredLines.at(insertIndex - 1).trimmed().isEmpty()) {
        --insertIndex;
    }
    if (insertIndex > 0 && filteredLines.at(insertIndex - 1).trimmed() == QStringLiteral(")")) {
        --insertIndex;
    }

    filteredLines.insert(insertIndex, makeLibraryEntry(libName, uri, libraryDescription));
    QString updatedContent = filteredLines.join('\n');
    if (!updatedContent.endsWith('\n')) {
        updatedContent += '\n';
    }

    QSaveFile file(tablePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "KiCadLibraryTableManager: Failed to write library table:" << tablePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << updatedContent;
    if (!file.commit()) {
        qWarning() << "KiCadLibraryTableManager: Failed to commit library table:" << tablePath;
        return false;
    }

    qDebug() << "KiCadLibraryTableManager: Updated project library table:" << tablePath;
    return true;
}

QString KiCadLibraryTableManager::makeLibraryEntry(const QString& libName,
                                                   const QString& uri,
                                                   const QString& libraryDescription) {
    QString entry = QStringLiteral("  (lib (name \"%1\")(type \"KiCad\")(uri \"%2\")(options \"\")")
                        .arg(KiCadTableUtils::escapeTableValue(libName), KiCadTableUtils::escapeTableValue(uri));
    if (!libraryDescription.isEmpty()) {
        entry += QStringLiteral("(descr \"%1\")").arg(KiCadTableUtils::escapeTableValue(libraryDescription));
    } else {
        entry += QStringLiteral("(descr \"\")");
    }
    entry += QStringLiteral(")");
    return entry;
}

QString KiCadLibraryTableManager::makeProjectUri(const QString& projectRoot, const QString& libraryPath) {
    QString relativePath = QDir(projectRoot).relativeFilePath(QFileInfo(libraryPath).absoluteFilePath());
    relativePath.replace(QDir::separator(), QLatin1Char('/'));
    if (relativePath == QStringLiteral(".")) {
        return QStringLiteral("${KIPRJMOD}");
    }
    return QStringLiteral("${KIPRJMOD}/%1").arg(relativePath);
}

}  // namespace EasyKiConverter
