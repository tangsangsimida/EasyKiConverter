#include "AltiumSchComponentRecordWriter.h"

#include "AltiumSchLibWriter.h"
#include "utils/AltiumCoord.h"

#include <QSet>

#include <cmath>

namespace EasyKiConverter {

namespace {

/** @brief 表示一个待写入 SchLib RECORD=41 的参数字段。 */
struct ParameterField {
    QString name;
    QString value;
    int locationX = 0;
    int locationY = 0;
    int fontId = 1;
    uint32_t color = 0x000000;
    bool hasLocation = false;
    bool hasVisibility = false;
    bool isHidden = true;
    bool readOnly = false;
    int orientation = 0;
    int ownerPartId = -1;
    double fontSizeMm = 0.0;
};

/**
 * @brief 将来源元数据和显式参数统一转换为参数字段。
 * @details 保留未知元数据键，使新增供应商字段无需修改 SchLib 协议模型。
 */
QList<ParameterField> componentParameterFields(const AltiumSchComponent& component) {
    static const QMap<QString, QString> knownNames = {
        {QStringLiteral("description"), QStringLiteral("Description")},
        {QStringLiteral("manufacturer"), QStringLiteral("Manufacturer")},
        {QStringLiteral("manufacturerPart"), QStringLiteral("Manufacturer Part Number")},
        {QStringLiteral("datasheet"), QStringLiteral("Datasheet")},
        {QStringLiteral("lcscId"), QStringLiteral("LCSC Part")},
        {QStringLiteral("jlcId"), QStringLiteral("JLCPCB Part")},
        {QStringLiteral("jlcpcbPartClass"), QStringLiteral("JLCPCB Part Class")},
        {QStringLiteral("supplierPart"), QStringLiteral("Supplier Part Number")},
        {QStringLiteral("supplier"), QStringLiteral("Supplier")},
        {QStringLiteral("category"), QStringLiteral("Category")},
        {QStringLiteral("source"), QStringLiteral("Source")},
    };

    QList<ParameterField> fields;
    QSet<QString> names;
    const auto appendField = [&](const QString& name,
                                 const QString& value,
                                 bool hasLocation = false,
                                 int locationX = 0,
                                 int locationY = 0,
                                 bool hasVisibility = false,
                                 bool isHidden = true,
                                 bool readOnly = false,
                                 int orientation = 0,
                                 int ownerPartId = -1,
                                 int fontId = 1,
                                 uint32_t color = 0x000000,
                                 double fontSizeMm = 0.0) {
        const QString normalizedName = name.trimmed();
        const QString deduplicationKey = QStringLiteral("%1:%2").arg(normalizedName).arg(ownerPartId);
        if (normalizedName.isEmpty() || value.trimmed().isEmpty() || names.contains(deduplicationKey))
            return;
        ParameterField field;
        field.name = normalizedName;
        field.value = value.trimmed();
        field.hasLocation = hasLocation;
        field.locationX = locationX;
        field.locationY = locationY;
        field.hasVisibility = hasVisibility;
        field.isHidden = isHidden;
        field.readOnly = readOnly;
        field.orientation = orientation;
        field.ownerPartId = ownerPartId;
        field.fontId = fontId;
        field.color = color;
        field.fontSizeMm = fontSizeMm;
        fields.append(field);
        names.insert(deduplicationKey);
    };

    QString value = component.sourceMetadata.value(QStringLiteral("value")).trimmed();
    if (value.isEmpty())
        value = component.name.trimmed();
    if (!value.isEmpty())
        appendField(QStringLiteral("Comment"), value);

    QString description = component.sourceMetadata.value(QStringLiteral("description")).trimmed();
    if (description.isEmpty())
        description = component.description.trimmed();
    if (!description.isEmpty())
        appendField(QStringLiteral("Description"), description);

    for (auto it = component.sourceMetadata.constBegin(); it != component.sourceMetadata.constEnd(); ++it) {
        const QString metadataValue = it.value().trimmed();
        if (metadataValue.isEmpty() || it.key() == QStringLiteral("value") || it.key() == QStringLiteral("description"))
            continue;
        appendField(knownNames.value(it.key(), it.key()), metadataValue);
    }

    if (!component.aliases.isEmpty())
        appendField(QStringLiteral("Aliases"), component.aliases.join(QStringLiteral(", ")));

    for (const AltiumSchParameter& parameter : component.parameters) {
        QString name = parameter.name.trimmed();
        if (name.compare(QStringLiteral("Value"), Qt::CaseInsensitive) == 0)
            name = QStringLiteral("Comment");
        appendField(name,
                    parameter.value,
                    parameter.locationX != 0 || parameter.locationY != 0,
                    parameter.locationX,
                    parameter.locationY,
                    true,
                    parameter.isHidden,
                    parameter.readOnly,
                    parameter.orientation,
                    parameter.ownerPartId,
                    parameter.fontId,
                    parameter.color,
                    parameter.fontSizeMm);
    }
    return fields;
}

}  // namespace

/** @brief 保存协作者所属的 SchLib 主写入器。 */
AltiumSchComponentRecordWriter::AltiumSchComponentRecordWriter(AltiumSchLibWriter& owner) : m_owner(owner) {}

/** @brief 写入 Designator、来源元数据和显式参数字段。 */
void AltiumSchComponentRecordWriter::writeParameters(AltiumBinaryWriter& writer, const AltiumSchComponent& component) {
    QString designator = component.designatorPrefix.trimmed();
    if (designator.isEmpty())
        designator = "?";
    else if (!designator.endsWith('?'))
        designator += '?';

    QMap<QString, QString> designatorParams;
    designatorParams["RECORD"] = "34";
    designatorParams["OWNERPARTID"] = "-1";
    designatorParams["LOCATION.X_FRAC"] = "-5";
    designatorParams["LOCATION.Y_FRAC"] = "5";
    designatorParams["COLOR"] = "8388608";
    designatorParams["FONTID"] = "1";
    designatorParams["TEXT"] = designator;
    designatorParams["NAME"] = "Designator";
    designatorParams["READONLYSTATE"] = "1";
    m_owner.addUniqueID(designatorParams);
    writer.writeCStringParameterBlockUtf8(designatorParams);

    for (const ParameterField& field : componentParameterFields(component)) {
        QMap<QString, QString> parameterParams;
        parameterParams["RECORD"] = "41";
        const int ownerPartId = m_owner.normalizeOwnerPartId(field.ownerPartId, QStringLiteral("参数"));
        parameterParams["OWNERPARTID"] = QString::number(ownerPartId);
        if (ownerPartId >= 1) {
            parameterParams["OWNERPARTDISPLAYMODE"] = "1";
            m_owner.addContentIndex(parameterParams);
        }
        if (field.hasLocation) {
            m_owner.addCoordParam(parameterParams, "LOCATION.X", field.locationX);
            m_owner.addCoordParam(parameterParams, "LOCATION.Y", field.locationY);
        } else {
            parameterParams["LOCATION.X_FRAC"] = "-5";
            parameterParams["LOCATION.Y_FRAC"] = "-15";
        }
        if (field.color != 0)
            parameterParams["COLOR"] = QString::number(field.color);
        int fontId = field.fontId;
        if (std::isfinite(field.fontSizeMm) && field.fontSizeMm > 0.0) {
            constexpr double MILLIMETERS_PER_POINT = 25.4 / 72.0;
            fontId = m_owner.m_fontRegistry.getOrAdd(QStringLiteral("Times New Roman"),
                                                     qMax(1, qRound(field.fontSizeMm / MILLIMETERS_PER_POINT)));
        } else if (fontId < 1 || fontId > m_owner.m_fontRegistry.size()) {
            fontId = 1;
        }
        parameterParams["FONTID"] = QString::number(fontId);
        parameterParams["TEXT"] = field.value;
        parameterParams["NAME"] = field.name;
        if (field.orientation != 0)
            parameterParams["Orientation"] = QString::number(field.orientation);
        if (field.hasVisibility && field.isHidden)
            parameterParams["IsHidden"] = "T";
        if (field.readOnly)
            parameterParams["READONLYSTATE"] = "1";
        m_owner.addUniqueID(parameterParams);
        writer.writeCStringParameterBlockUtf8(parameterParams);
    }
}

/** @brief 写入实现容器、模型信息、引脚映射和实现参数。 */
void AltiumSchComponentRecordWriter::writeImplementations(AltiumBinaryWriter& writer,
                                                          const AltiumSchComponent& component) {
    QMap<QString, QString> listParams;
    listParams["RECORD"] = "44";
    listParams["DataFileFormatID"] = "";
    listParams["Description"] = "";
    listParams["FileName"] = "";
    writer.writeCStringParameterBlockUtf8(listParams);

    for (int implementationIndex = 0; implementationIndex < component.implementations.size(); ++implementationIndex) {
        const AltiumSchComponent::Implementation& implementation = component.implementations.at(implementationIndex);
        QMap<QString, QString> implementationParams;
        implementationParams["RECORD"] = "45";
        implementationParams["DESCRIPTION"] = implementation.modelName;
        implementationParams["MODELNAME"] = implementation.modelName;
        implementationParams["MODELTYPE"] = implementation.modelType;
        const QString modelType = implementation.modelType.trimmed().toUpper();
        const bool isPcbLibrary = modelType.isEmpty() || modelType == QStringLiteral("PCBLIB");
        const bool hasExplicitDataFileKind =
            !implementation.dataFileKind.trimmed().isEmpty() &&
            (isPcbLibrary || implementation.dataFileKind.compare(QStringLiteral("PCBLib"), Qt::CaseInsensitive) != 0);
        const bool hasDataFile =
            isPcbLibrary || hasExplicitDataFileKind || !implementation.dataFileEntity.trimmed().isEmpty();
        implementationParams["DATAFILECOUNT"] = hasDataFile ? "1" : "0";
        if (hasDataFile) {
            implementationParams["MODELDATAFILEKIND1"] =
                implementation.dataFileKind.isEmpty() ? "PCBLib" : implementation.dataFileKind;
            implementationParams["MODELDATAFILEENTITY1"] =
                implementation.dataFileEntity.isEmpty()
                    ? (isPcbLibrary && !m_owner.m_libraryName.isEmpty() ? m_owner.m_libraryName + ".PcbLib" : "*")
                    : implementation.dataFileEntity;
        }
        // Altium 只允许一个默认实现，候选封装和附加模型不能全部标记为当前。
        if (implementationIndex == 0)
            implementationParams["ISCURRENT"] = "T";
        m_owner.addUniqueID(implementationParams);
        writer.writeCStringParameterBlockUtf8(implementationParams);

        QMap<QString, QString> mapListParams;
        mapListParams["RECORD"] = "46";
        writer.writeCStringParameterBlockUtf8(mapListParams);

        // Altium 通过 RECORD=47 建立符号引脚与当前 PCBLib 实现之间的关联。
        for (int pinIndex = 1; pinIndex <= component.pins.size(); ++pinIndex) {
            QMap<QString, QString> pinMappingParams;
            pinMappingParams["RECORD"] = "47";
            pinMappingParams["DESINTF"] = QString::number(pinIndex);
            pinMappingParams["DESIMPCOUNT"] = "1";
            pinMappingParams["DESIMP0"] =
                implementation.pinMappings.value(QString::number(pinIndex), QString::number(pinIndex));
            pinMappingParams["ISTRIVIAL"] = "T";
            m_owner.addUniqueID(pinMappingParams);
            writer.writeCStringParameterBlockUtf8(pinMappingParams);
        }

        QMap<QString, QString> parameterParams;
        parameterParams["RECORD"] = "48";
        for (auto it = implementation.parameters.constBegin(); it != implementation.parameters.constEnd(); ++it)
            parameterParams[it.key()] = it.value();
        writer.writeCStringParameterBlockUtf8(parameterParams);
    }
}

/** @brief 返回 Designator 加上所有去重后的参数字段数量。 */
int AltiumSchComponentRecordWriter::parameterRecordCount(const AltiumSchComponent& component) {
    return 1 + componentParameterFields(component).size();
}

}  // namespace EasyKiConverter
