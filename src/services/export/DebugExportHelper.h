#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSharedPointer>
#include <QString>

namespace EasyKiConverter {

class ComponentData;

/**
 * @brief Debug data exporter - exports raw component data for troubleshooting.
 *        Output path format: outputPath/debug/componentId/
 */
class DebugExportHelper {
public:
    static bool exportDebugData(const QString& componentId,
                                const QSharedPointer<ComponentData>& data,
                                const QString& outputPath);

private:
    static bool writeAtomically(const QString& tempDir, const QString& filePath, const QByteArray& data);
};

}  // namespace EasyKiConverter