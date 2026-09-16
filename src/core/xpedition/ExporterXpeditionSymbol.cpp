#include "ExporterXpeditionSymbol.h"

#include "XpeditionZipWriter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QtMath>

namespace EasyKiConverter {

namespace {

constexpr double kThousandthInchMm = 0.0254;

double toTh(double valueMm) {
    return valueMm / kThousandthInchMm;
}

QString safeName(QString name) {
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    return name.isEmpty() ? QStringLiteral("unnamed") : name;
}

QString fmt(double value) {
    return QString::number(value, 'f', 4);
}

QPointF directionVector(IR::PinDirection direction) {
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

void appendLine(QString& output, double x1, double y1, double x2, double y2) {
    output += QStringLiteral("l 2 %1 %2 %3 %4\n|GRPHSTL -1 0 0 1\n")
                  .arg(fmt(toTh(x1)), fmt(toTh(y1)), fmt(toTh(x2)), fmt(toTh(y2)));
}

void appendPolyline(QString& output, const QList<QPointF>& points) {
    for (int i = 1; i < points.size(); ++i)
        appendLine(output, points[i - 1].x(), points[i - 1].y(), points[i].x(), points[i].y());
}

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

QString ExporterXpeditionSymbol::libraryFileExtension() const {
    return QStringLiteral("_Symbols.zip");
}

QStringList ExporterXpeditionSymbol::diagnostics() const {
    return m_diagnostics;
}

QString ExporterXpeditionSymbol::symbolFileName(const IR::SymbolComponentIR& symbol, int partIndex) const {
    return QStringLiteral("%1.%2").arg(safeName(symbol.name)).arg(partIndex + 1);
}

QByteArray ExporterXpeditionSymbol::symbolFile(const IR::SymbolComponentIR& symbol, int partIndex) const {
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

bool ExporterXpeditionSymbol::exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) {
    return exportSymbolLibrary({symbol}, symbol.name, filePath, false, false);
}

bool ExporterXpeditionSymbol::exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                                                  const QString&,
                                                  const QString& filePath,
                                                  bool,
                                                  bool,
                                                  const QString&) {
    m_diagnostics.clear();
    XpeditionZipWriter archive;
    for (const IR::SymbolComponentIR& symbol : symbols) {
        if (symbol.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("跳过名称为空的 Xpedition 符号"));
            continue;
        }
        const int partCount = qMax(1, symbol.partCount);
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
