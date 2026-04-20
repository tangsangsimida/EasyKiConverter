#include "FileReader.h"

#include "services/BomParser.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace EasyKiConverter {

QStringList FileReader::readBomFile(const QString& filePath, QString& errorMessage) {
    QStringList componentIds;

    if (!QFile::exists(filePath)) {
        errorMessage = QString("输入文件不存在: %1").arg(filePath);
        return componentIds;
    }

    // 使用 BomParser 解析文件
    BomParser parser;
    componentIds = parser.parse(filePath);

    if (componentIds.isEmpty()) {
        errorMessage = "BOM 表中没有找到有效的元器件编号";
    }

    return componentIds;
}

QStringList FileReader::readComponentListFile(const QString& filePath, QString& errorMessage) {
    QStringList componentIds;

    if (!QFile::exists(filePath)) {
        errorMessage = QString("输入文件不存在: %1").arg(filePath);
        return componentIds;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorMessage = QString("无法打开输入文件: %1").arg(filePath);
        return componentIds;
    }

    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();
        if (!line.isEmpty()) {
            componentIds.append(line);
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
