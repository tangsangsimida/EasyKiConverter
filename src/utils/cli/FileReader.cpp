#include "FileReader.h"

#include "services/BomParser.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace {
bool isValidComponentId(const QString& id) {
    return EasyKiConverter::BomParser::validateId(id);
}
}  // namespace

namespace EasyKiConverter {

QStringList FileReader::readBomFile(const QString& filePath, QString& errorMessage) {
    QStringList componentIds;

    if (!QFile::exists(filePath)) {
        errorMessage = QCoreApplication::translate("CliConverter", "输入文件不存在: %1").arg(filePath);
        return componentIds;
    }

    // 使用 BomParser 解析文件
    BomParser parser;
    componentIds = parser.parse(filePath);

    if (componentIds.isEmpty()) {
        errorMessage = QCoreApplication::translate("CliConverter", "BOM 表中没有找到有效的元器件编号");
    }

    return componentIds;
}

QStringList FileReader::readComponentListFile(const QString& filePath, QString& errorMessage) {
    QStringList componentIds;

    if (!QFile::exists(filePath)) {
        errorMessage = QCoreApplication::translate("CliConverter", "输入文件不存在: %1").arg(filePath);
        return componentIds;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QCoreApplication::translate("CliConverter", "无法打开输入文件: %1").arg(filePath);
        return componentIds;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            if (isValidComponentId(line)) {
                componentIds.append(line.toUpper());
            } else {
                qWarning() << "FileReader: skipping invalid component ID:" << line;
            }
        }
    }

    file.close();
    return componentIds;
}

bool FileReader::fileExists(const QString& filePath) {
    return QFile::exists(filePath);
}

QString FileReader::getFileExtension(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    return fileInfo.suffix().toLower();
}

}  // namespace EasyKiConverter
