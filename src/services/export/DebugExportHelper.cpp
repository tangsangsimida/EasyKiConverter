#include "DebugExportHelper.h"

#include "ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFile>

namespace EasyKiConverter {

bool DebugExportHelper::exportDebugData(const QString& componentId,
                                        const QSharedPointer<ComponentData>& data,
                                        const QString& outputPath) {
    if (!data) {
        qWarning() << "DebugExportHelper: data is null, skip export for" << componentId;
        return false;
    }

    QString debugDirPath = QStringLiteral("%1/debug/%2").arg(outputPath, componentId);
    if (!createOutputDirectory(debugDirPath)) {
        qWarning() << "DebugExportHelper: Failed to create debug directory:" << debugDirPath;
        return false;
    }

    qDebug() << "DebugExportHelper: Exporting debug data for" << componentId << "to" << debugDirPath;

    bool success = true;

    // 导出 cinfo_raw.json
    if (!data->cinfoJsonRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/cinfo_raw.json");
        if (!writeFile(filePath, data->cinfoJsonRaw())) {
            qWarning() << "DebugExportHelper: Failed to write cinfo_raw.json";
            success = false;
        }
    }

    // 导出 cad_raw.json
    if (!data->cadJsonRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/cad_raw.json");
        if (!writeFile(filePath, data->cadJsonRaw())) {
            qWarning() << "DebugExportHelper: Failed to write cad_raw.json";
            success = false;
        }
    }

    // 导出 model3d_raw.obj
    if (!data->model3DObjRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/model3d_raw.obj");
        if (!writeFile(filePath, data->model3DObjRaw())) {
            qWarning() << "DebugExportHelper: Failed to write model3d_raw.obj";
            success = false;
        }
    }

    // 导出 symbol.json（解析后的符号数据）
    if (data->symbolData()) {
        QJsonObject symbolObj = data->symbolData()->toJson();
        if (!symbolObj.isEmpty()) {
            QString filePath = debugDirPath + QStringLiteral("/symbol.json");
            QJsonDocument doc(symbolObj);
            if (!writeFile(filePath, doc.toJson(QJsonDocument::Indented))) {
                qWarning() << "DebugExportHelper: Failed to write symbol.json";
                success = false;
            }
        }
    }

    // 导出 footprint.json（解析后的封装数据）
    if (data->footprintData()) {
        QJsonObject footprintObj = data->footprintData()->toJson();
        if (!footprintObj.isEmpty()) {
            QString filePath = debugDirPath + QStringLiteral("/footprint.json");
            QJsonDocument doc(footprintObj);
            if (!writeFile(filePath, doc.toJson(QJsonDocument::Indented))) {
                qWarning() << "DebugExportHelper: Failed to write footprint.json";
                success = false;
            }
        }
    }

    // 导出 component_info.json（包含基本信息）
    QJsonObject componentInfo;
    componentInfo["lcscId"] = data->lcscId();
    componentInfo["name"] = data->name();
    componentInfo["prefix"] = data->prefix();
    componentInfo["package"] = data->package();
    componentInfo["manufacturer"] = data->manufacturer();
    componentInfo["manufacturerPart"] = data->manufacturerPart();
    if (!data->previewImages().isEmpty()) {
        QJsonArray previewArray;
        for (const QString& url : data->previewImages()) {
            previewArray.append(url);
        }
        componentInfo["previewImages"] = previewArray;
    }
    QString filePath = debugDirPath + QStringLiteral("/component_info.json");
    QJsonDocument doc(componentInfo);
    if (!writeFile(filePath, doc.toJson(QJsonDocument::Indented))) {
        qWarning() << "DebugExportHelper: Failed to write component_info.json";
        success = false;
    }

    return success;
}

bool DebugExportHelper::createOutputDirectory(const QString& path) {
    QDir dir;
    return dir.mkpath(path);
}

bool DebugExportHelper::writeFile(const QString& filePath, const QByteArray& data) {
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
        return true;
    }
    qWarning() << "DebugExportHelper: Failed to open file for writing:" << filePath;
    return false;
}

}  // namespace EasyKiConverter