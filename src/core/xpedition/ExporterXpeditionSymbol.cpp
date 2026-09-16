#include "ExporterXpeditionSymbol.h"

#include "XpeditionZipWriter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QtMath>

namespace EasyKiConverter {

namespace {

constexpr double kThousandthInchMm = 0.0254;

/** @brief 将毫米转换为目标格式的千分之一英寸单位。 */
double toTh(double valueMm) {
    return valueMm / kThousandthInchMm;
}

/** @brief 清理符号名称，使其可作为 ZIP 条目名称。 */
QString safeName(QString name) {
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    return name.isEmpty() ? QStringLiteral("unnamed") : name;
}

/** @brief 使用稳定精度输出几何数值。 */
QString fmt(double value) {
    return QString::number(value, 'f', 4);
}

/** @brief 将引脚方向转换为单位方向向量。 */
QPointF directionVector(IR::PinDirection direction) {
    // 方向枚举到单位向量的转换决定引脚内外端点的几何关系。
    switch (direction) {
        case IR::PinDirection::Left:
            return {-1.0, 0.0};
        case IR::PinDirection::Up:
            return {0.0, 1.0};
        case IR::PinDirection::Down:
            return {0.0, -1.0};
        case IR::PinDirection::Right:
        default:
            return {1.0, 0.0};
    }
}

/** @brief 写入一个符号直线图元及其默认图形样式。 */
void appendLine(QString& output, double x1, double y1, double x2, double y2) {
    output += QStringLiteral("l 2 %1 %2 %3 %4\n|GRPHSTL -1 0 0 1\n")
                  .arg(fmt(toTh(x1)), fmt(toTh(y1)), fmt(toTh(x2)), fmt(toTh(y2)));
}

/** @brief 将点列拆分为目标格式可识别的连续线段。 */
void appendPolyline(QString& output, const QList<QPointF>& points) {
    for (int i = 1; i < points.size(); ++i)
        appendLine(output, points[i - 1].x(), points[i - 1].y(), points[i].x(), points[i].y());
}

/**
 * @brief 写入一个符号引脚及可选的名称、编号文本。
 * @param output 输出缓冲区。
 * @param pin 统一表示中的引脚。
 * @param index 目标文件中的引脚序号。
 */
void appendPin(QString& output, const IR::SymbolPinIR& pin, int index) {
    const QPointF direction = directionVector(pin.direction);
    const QPointF outer = pin.position;
    const QPointF inner = outer - direction * pin.length;
    const int side = pin.direction == IR::PinDirection::Left   ? 3
                     : pin.direction == IR::PinDirection::Up   ? 0
                     : pin.direction == IR::PinDirection::Down ? 1
                                                               : 2;
    output += QStringLiteral("P %1 %2 %3 %4 %5 0 %6 0\n")
                  .arg(index)
                  .arg(fmt(toTh(outer.x())))
                  .arg(fmt(toTh(outer.y())))
                  .arg(fmt(toTh(inner.x())))
                  .arg(fmt(toTh(inner.y())))
                  .arg(side);
    if (pin.display.showName && !pin.name.isEmpty())
        output += QStringLiteral("L %1 %2 8 0 2 0 1 0 %3\n")
                      .arg(fmt(toTh(pin.namePosition.x())))
                      .arg(fmt(toTh(pin.namePosition.y())))
                      .arg(pin.name);
    if (pin.display.showDesignator && !pin.designator.isEmpty())
        output += QStringLiteral("A %1 %2 8 0 3 3 #=%4\n")
                      .arg(fmt(toTh(pin.numberPosition.x())))
                      .arg(fmt(toTh(pin.numberPosition.y())))
                      .arg(pin.designator);
}

}  // namespace

// 符号归档中的每个条目都是 ASCII 部件文件，统一使用符号扩展名。
QString ExporterXpeditionSymbol::libraryFileExtension() const {
    // 符号库同样使用 ZIP，但条目内容是 ASCII 符号部件文件。
    return QStringLiteral("_Symbols.zip");
}

// 诊断信息由符号批量入口生成，读取操作不改变导出器状态。
QStringList ExporterXpeditionSymbol::diagnostics() const {
    // 诊断信息描述未支持图元和文件写入失败，供 CLI 与 GUI 共用。
    return m_diagnostics;
}

// 多单元符号通过“名称.部件序号”形成稳定的库条目名称。
QString ExporterXpeditionSymbol::symbolFileName(const IR::SymbolComponentIR& symbol, int partIndex) const {
    // 部件序号从 1 开始，符合目标符号库的多单元命名约定。
    return QStringLiteral("%1.%2").arg(safeName(symbol.name)).arg(partIndex + 1);
}

// 符号部件正文按照头信息、引脚、图元和结束记录的顺序拼接。
QByteArray ExporterXpeditionSymbol::symbolFile(const IR::SymbolComponentIR& symbol, int partIndex) const {
    // 每个部件独立成文件，同时保留 commonToAllParts 引脚，避免多单元符号丢失公共引脚。
    QString body;
    body += QStringLiteral("V 50\nK 1000000000 %1\nY 1\nZ 0\ni 0\n").arg(safeName(symbol.name));
    body += QStringLiteral("U 0 0 10 0 5 0 %1\n").arg(safeName(symbol.name));
    body += QStringLiteral("U 0 0 5 0 5 0 REFDES=%1\n").arg(symbol.designatorPrefix);
    body += QStringLiteral("U 0 0 5 0 5 0 VALUE=%1\n").arg(symbol.name);

    int pinIndex = 1;
    for (const IR::SymbolPinIR& pin : symbol.pins) {
        if (pin.partIndex == partIndex || pin.commonToAllParts)
            appendPin(body, pin, pinIndex++);
    }
    for (const IR::SymbolRectangleIR& rect : symbol.rectangles) {
        if (rect.partIndex != partIndex)
            continue;
        appendLine(body, rect.x0, rect.y0, rect.x1, rect.y0);
        appendLine(body, rect.x1, rect.y0, rect.x1, rect.y1);
        appendLine(body, rect.x1, rect.y1, rect.x0, rect.y1);
        appendLine(body, rect.x0, rect.y1, rect.x0, rect.y0);
    }
    for (const IR::SymbolPolylineIR& polyline : symbol.polylines) {
        if (polyline.partIndex == partIndex)
            appendPolyline(body, polyline.points);
    }
    for (const IR::SymbolPolygonIR& polygon : symbol.polygons) {
        if (polygon.partIndex == partIndex)
            appendPolyline(body, polygon.points);
    }
    for (const IR::SymbolCircleIR& circle : symbol.circles) {
        if (circle.partIndex != partIndex)
            continue;
        body += QStringLiteral("c %1 %2 %3\n|GRPHSTL_EXT01 255 -1 0 1 1\n")
                    .arg(fmt(toTh(circle.center.x())))
                    .arg(fmt(toTh(circle.center.y())))
                    .arg(fmt(toTh(circle.radius)));
    }
    for (const IR::SymbolArcIR& arc : symbol.arcs) {
        if (arc.partIndex != partIndex)
            continue;
        body += QStringLiteral("a %1 %2 %3 %4 %5 %6\n|GRPHSTL_EXT01 255 -1 0 1 1\n")
                    .arg(fmt(toTh(arc.startPoint.x())))
                    .arg(fmt(toTh(arc.startPoint.y())))
                    .arg(fmt(toTh(arc.midPoint.x())))
                    .arg(fmt(toTh(arc.midPoint.y())))
                    .arg(fmt(toTh(arc.endPoint.x())))
                    .arg(fmt(toTh(arc.endPoint.y())));
    }
    body += QStringLiteral("E\n");
    return body.toUtf8();
}

// 单符号入口委托批量入口，避免两条路径产生不同的归档结构。
bool ExporterXpeditionSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    // 单符号入口委托批量入口，避免两条路径产生不同的 ZIP 结构。
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

// 批量入口负责清理诊断、展开多单元并写入符号 ZIP。
bool ExporterXpeditionSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                  const QString&,
                                                  const QString& filePath,
                                                  bool,
                                                  bool,
                                                  const QString&) {
    // 清理任务级诊断后再写入各个部件，保证调用者读取到的是本次结果。
    m_diagnostics.clear();
    XpeditionZipWriter archive;
    for (const IR::SymbolComponentIR& symbol : symbols) {
        if (symbol.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("跳过名称为空的 Xpedition 符号"));
            continue;
        }
        const int partCount = qMax(1, symbol.partCount);
        if (!symbol.ellipses.isEmpty() || !symbol.pies.isEmpty() || !symbol.ellipticalArcs.isEmpty() ||
            !symbol.paths.isEmpty() || !symbol.beziers.isEmpty() || !symbol.ieeeSymbols.isEmpty() ||
            !symbol.texts.isEmpty() || !symbol.textFrames.isEmpty() || !symbol.images.isEmpty()) {
            m_diagnostics.append(QStringLiteral("Xpedition 符号 %1 包含当前未写入的 IR 图元").arg(symbol.name));
        }
        for (int partIndex = 0; partIndex < partCount; ++partIndex) {
            if (!archive.addFile(symbolFileName(symbol, partIndex), symbolFile(symbol, partIndex))) {
                m_diagnostics.append(QStringLiteral("Xpedition 符号文件名重复：%1").arg(symbol.name));
                return false;
            }
        }
    }
    if (archive.write(filePath))
        return true;
    m_diagnostics.append(QStringLiteral("无法写入 Xpedition 符号 ZIP：%1").arg(QFileInfo(filePath).fileName()));
    return false;
}

}  // namespace EasyKiConverter
