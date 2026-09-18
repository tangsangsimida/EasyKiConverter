#include "AltiumSymbolImplementationConverter.h"

#include <QSet>

namespace EasyKiConverter {

/** @brief 根据符号封装候选、显式模型和来源元数据创建实现记录。 */
QList<AltiumSchComponent::Implementation> AltiumSymbolImplementationConverter::convert(
    const IR::SymbolComponentIR& symbol) {
    QList<AltiumSchComponent::Implementation> implementations;

    QStringList footprintNames = symbol.footprintNames;
    if (footprintNames.isEmpty() && !symbol.footprintName.isEmpty())
        footprintNames.append(symbol.footprintName);

    QSet<QString> uniqueFootprints;
    for (const QString& footprintName : footprintNames) {
        const QString normalizedName = footprintName.trimmed();
        if (normalizedName.isEmpty() || uniqueFootprints.contains(normalizedName))
            continue;
        AltiumSchComponent::Implementation implementation;
        implementation.modelName = normalizedName;
        implementation.modelType = QStringLiteral("PCBLIB");
        implementation.dataFileKind = QStringLiteral("PCBLib");
        implementations.append(implementation);
        uniqueFootprints.insert(normalizedName);
    }

    for (const IR::SymbolModelIR& model : symbol.models) {
        const QString modelName = model.name.trimmed();
        if (modelName.isEmpty())
            continue;
        AltiumSchComponent::Implementation implementation;
        implementation.modelName = modelName;
        implementation.modelType = model.type.trimmed().isEmpty() ? QStringLiteral("SIM") : model.type.trimmed();
        implementation.dataFileKind = model.fileKind.trimmed();
        implementation.dataFileEntity = model.fileEntity.trimmed();
        implementation.parameters = model.parameters;
        implementation.pinMappings = model.pinMappings;
        implementations.append(implementation);
    }

    // 兼容没有显式 SymbolModelIR 的调用方，允许通过来源元数据关联模型。
    const QMap<QString, QString> metadataModels = {
        {QStringLiteral("spiceModel"), QStringLiteral("SPICE")},
        {QStringLiteral("simulationModel"), QStringLiteral("SIM")},
        {QStringLiteral("model3D"), QStringLiteral("STEP")},
        {QStringLiteral("model3d"), QStringLiteral("STEP")},
    };
    for (auto it = metadataModels.constBegin(); it != metadataModels.constEnd(); ++it) {
        const QString modelName = symbol.sourceMetadata.value(it.key()).trimmed();
        if (modelName.isEmpty())
            continue;
        AltiumSchComponent::Implementation implementation;
        implementation.modelName = modelName;
        implementation.modelType = it.value();
        implementation.dataFileKind = symbol.sourceMetadata.value(it.key() + QStringLiteral("FileKind")).trimmed();
        implementation.dataFileEntity = symbol.sourceMetadata.value(it.key() + QStringLiteral("File")).trimmed();
        implementations.append(implementation);
    }

    return implementations;
}

}  // namespace EasyKiConverter
