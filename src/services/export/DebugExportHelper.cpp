#include "DebugExportHelper.h"

#include "ComponentData.h"
#include "core/utils/AtomicFileWriter.h"

#include <QDebug>
#include <QDir>
#include <QFile>

namespace EasyKiConverter {

namespace {

static const QString kCinfoRawFile = QStringLiteral("cinfo_raw.json");
static const QString kCadRawFile = QStringLiteral("cad_raw.json");
static const QString kModel3dRawFile = QStringLiteral("model3d_raw.obj");
static const QString kSymbolFile = QStringLiteral("symbol.json");
static const QString kFootprintFile = QStringLiteral("footprint.json");
static const QString kComponentInfoFile = QStringLiteral("component_info.json");

}  // anonymous namespace

bool DebugExportHelper::exportDebugData(const QString& componentId,
                                        const QSharedPointer<ComponentData>& data,
                                        const QString& outputPath) {
    if (!data) {
        qWarning() << "DebugExportHelper: data is null, skip export for" << componentId;
        return false;
    }

    QString debugDirPath = QStringLiteral("%1/debug/%2").arg(outputPath, componentId);
    if (!AtomicFileWriter::createDirectory(debugDirPath)) {
        qWarning() << "DebugExportHelper: Failed to create debug directory:" << debugDirPath;
        return false;
    }

    qDebug() << "DebugExportHelper: Exporting debug data for" << componentId << "to" << debugDirPath;

    bool success = true;

    if (!data->cinfoJsonRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/") + kCinfoRawFile;
        if (!writeAtomically(debugDirPath, filePath, data->cinfoJsonRaw())) {
            success = false;
        }
    }

    if (!data->cadJsonRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/") + kCadRawFile;
        if (!writeAtomically(debugDirPath, filePath, data->cadJsonRaw())) {
            success = false;
        }
    }

    if (!data->model3DObjRaw().isEmpty()) {
        QString filePath = debugDirPath + QStringLiteral("/") + kModel3dRawFile;
        if (!writeAtomically(debugDirPath, filePath, data->model3DObjRaw())) {
            success = false;
        }
    }

    if (data->symbolData()) {
        QJsonObject symbolObj = data->symbolData()->toJson();
        if (!symbolObj.isEmpty()) {
            QString filePath = debugDirPath + QStringLiteral("/") + kSymbolFile;
            QJsonDocument doc(symbolObj);
            if (!writeAtomically(debugDirPath, filePath, doc.toJson(QJsonDocument::Indented))) {
                success = false;
            }
        }
    }

    if (data->footprintData()) {
        QJsonObject footprintObj = data->footprintData()->toJson();
        if (!footprintObj.isEmpty()) {
            QString filePath = debugDirPath + QStringLiteral("/") + kFootprintFile;
            QJsonDocument doc(footprintObj);
            if (!writeAtomically(debugDirPath, filePath, doc.toJson(QJsonDocument::Indented))) {
                success = false;
            }
        }
    }

    QJsonObject componentInfo;
    componentInfo[QStringLiteral("lcscId")] = data->lcscId();
    componentInfo[QStringLiteral("name")] = data->name();
    componentInfo[QStringLiteral("prefix")] = data->prefix();
    componentInfo[QStringLiteral("package")] = data->package();
    componentInfo[QStringLiteral("manufacturer")] = data->manufacturer();
    componentInfo[QStringLiteral("manufacturerPart")] = data->manufacturerPart();
    if (!data->previewImages().isEmpty()) {
        QJsonArray previewArray;
        for (const QString& url : data->previewImages()) {
            previewArray.append(url);
        }
        componentInfo[QStringLiteral("previewImages")] = previewArray;
    }
    QString filePath = debugDirPath + QStringLiteral("/") + kComponentInfoFile;
    QJsonDocument doc(componentInfo);
    if (!writeAtomically(debugDirPath, filePath, doc.toJson(QJsonDocument::Indented))) {
        success = false;
    }

    return success;
}

bool DebugExportHelper::writeAtomically(const QString& tempDir, const QString& filePath, const QByteArray& data) {
    return AtomicFileWriter::writeAtomically(tempDir, filePath, QStringLiteral(".tmp"), data);
}

}  // namespace EasyKiConverter