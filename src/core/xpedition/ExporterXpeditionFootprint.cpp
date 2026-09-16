#include "ExporterXpeditionFootprint.h"

#include "XpeditionZipWriter.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <cmath>

namespace EasyKiConverter {

namespace {

constexpr double kThousandthInchMm = 0.0254;

double toTh(double valueMm) {
    return valueMm / kThousandthInchMm;
}

QString fmt(double value) {
    return QString::number(value, 'f', 4);
}

QString padShape(IR::PadShape shape) {
    switch (shape) {
        case IR::PadShape::Ellipse:
            return QStringLiteral("ROUND");
        case IR::PadShape::Oval:
            return QStringLiteral("OBLONG");
        case IR::PadShape::Polygon:
            return QStringLiteral("CUSTOM");
        case IR::PadShape::Rect:
        case IR::PadShape::RoundRect:
        case IR::PadShape::Trapezoid:
        default:
            return QStringLiteral("RECTANGLE");
    }
}

QString padName(const IR::FootprintPadIR& pad) {
    return QStringLiteral("PAD_%1_%2x%3")
        .arg(padShape(pad.shape))
        .arg(fmt(toTh(pad.size.width())))
        .arg(fmt(toTh(pad.size.height())));
}

QRectF footprintBounds(const IR::FootprintComponentIR& footprint) {
    QRectF bounds;
    bool initialized = false;
    const auto addPoint = [&](const QPointF& point) {
        if (!initialized) {
            bounds = QRectF(point, QSizeF(0, 0));
            initialized = true;
        } else {
            bounds = bounds.united(QRectF(point, QSizeF(0, 0)));
        }
    };
    for (const auto& pad : footprint.pads) {
        addPoint(pad.position - QPointF(pad.size.width() / 2.0, pad.size.height() / 2.0));
        addPoint(pad.position + QPointF(pad.size.width() / 2.0, pad.size.height() / 2.0));
    }
    for (const auto& circle : footprint.circles) {
        addPoint(circle.center - QPointF(circle.radius, circle.radius));
        addPoint(circle.center + QPointF(circle.radius, circle.radius));
    }
    for (const auto& rect : footprint.rectangles)
        if (!initialized) {
            bounds = rect.bounds;
            initialized = true;
        } else {
            bounds = bounds.united(rect.bounds);
        }
    for (const auto& outline : footprint.outlines) {
        for (const auto& point : outline.points)
            addPoint(point);
    }
    for (const auto& track : footprint.tracks) {
        for (const auto& point : track.points)
            addPoint(point);
    }
    for (const auto& hole : footprint.holes)
        addPoint(hole.center);
    return initialized ? bounds : QRectF(-1, -1, 2, 2);
}

QString shapeBlock(const QString& kind, const QList<QPointF>& points, double width = 0.0) {
    if (points.isEmpty())
        return {};
    QString output = QStringLiteral(" ...%1\n  ....WIDTH %2\n  ....XY (%3, %4)")
                         .arg(kind)
                         .arg(fmt(toTh(width)))
                         .arg(fmt(toTh(points.first().x())))
                         .arg(fmt(toTh(points.first().y())));
    for (int index = 1; index < points.size(); ++index)
        output +=
            QStringLiteral("\n   ...XY (%1, %2)").arg(fmt(toTh(points[index].x()))).arg(fmt(toTh(points[index].y())));
    output += QLatin1Char('\n');
    return output;
}

}  // namespace

QString ExporterXpeditionFootprint::libraryFileExtension() const {
    return QStringLiteral("_Footprints.zip");
}

bool ExporterXpeditionFootprint::isDirectoryOutput() const {
    return false;
}

QStringList ExporterXpeditionFootprint::diagnostics() const {
    return m_diagnostics;
}

QString ExporterXpeditionFootprint::safeName(QString name) const {
    name = name.trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    return name.isEmpty() ? QStringLiteral("unnamed") : name;
}

QByteArray ExporterXpeditionFootprint::padstackFile(const IR::FootprintComponentIR& footprint) const {
    QString output = QStringLiteral(".FILETYPE PADSTACK_LIBRARY\n.VERSION \"VB99.0\"\n.SCHEMA_VERSION 13\n") +
                     QStringLiteral(".CREATOR \"EasyKiConverter\"\n\n.UNITS TH\n\n");
    QSet<QString> writtenPads;
    QSet<QString> writtenStacks;
    for (const auto& pad : footprint.pads) {
        const QString baseName = padName(pad);
        if (!writtenPads.contains(baseName)) {
            output += QStringLiteral(".PAD \"%1\"\n..PAD_OPTIONS USER_GENERATED_NAME\n..OFFSET (0, 0)\n..%2\n")
                          .arg(baseName, padShape(pad.shape));
            if (pad.shape == IR::PadShape::Ellipse)
                output += QStringLiteral("...DIAMETER %1\n").arg(fmt(toTh(pad.size.width())));
            else if (pad.shape == IR::PadShape::Polygon && !pad.customShapePoints.isEmpty()) {
                output += QStringLiteral("...POLYLINE_SHAPE\n....XY");
                for (const QPointF& point : pad.customShapePoints)
                    output += QStringLiteral(" (%1, %2)").arg(fmt(toTh(point.x()))).arg(fmt(toTh(point.y())));
                output += QStringLiteral("\n....SHAPE_OPTIONS FILLED\n");
            } else {
                output += QStringLiteral("...WIDTH %1\n...HEIGHT %2\n")
                              .arg(fmt(toTh(pad.size.width())))
                              .arg(fmt(toTh(pad.size.height())));
            }
            writtenPads.insert(baseName);
        }

        const QString stackName = baseName + (pad.isThroughHole() ? QStringLiteral("_TH") : QStringLiteral("_SMD"));
        if (writtenStacks.contains(stackName))
            continue;
        output += QStringLiteral(".PADSTACK \"%1\"\n..PADSTACK_TYPE %2\n..TECHNOLOGY \"(Default)\"\n")
                      .arg(stackName, pad.isThroughHole() ? QStringLiteral("PIN_THROUGH") : QStringLiteral("PIN_SMD"));
        output += QStringLiteral("...TECHNOLOGY_OPTIONS NONE\n...TOP_PAD \"%1\"\n...BOTTOM_PAD \"%1\"\n").arg(baseName);
        if (pad.isThroughHole()) {
            const QString holeName = QStringLiteral("HOLE_%1").arg(fmt(toTh(pad.holeSize)));
            output += QStringLiteral(".Hole \"%1\"\n..POSITIVE_TOLERANCE 0\n..NEGATIVE_TOLERANCE 0\n").arg(holeName);
            output += QStringLiteral("..HOLE_OPTIONS %1 DRILLED USER_GENERATED_NAME\n..ROUND\n...DIAMETER %2\n")
                          .arg(pad.isPlated ? QStringLiteral("PLATED") : QStringLiteral("NON_PLATED"))
                          .arg(fmt(toTh(pad.holeSize)));
            output += QStringLiteral("...HOLE_NAME \"%1\"\n").arg(holeName);
        }
        writtenStacks.insert(stackName);
    }
    return output.toUtf8();
}

QByteArray ExporterXpeditionFootprint::cellFile(const IR::FootprintComponentIR& footprint) const {
    const QRectF bounds = footprintBounds(footprint);
    const QPointF origin = bounds.center();
    QString output =
        QStringLiteral(".FILETYPE CELL_LIBRARY\n.VERSION \"1.01.01\"\n.CREATOR \"EasyKiConverter\"\n\n.UNITS TH\n\n");
    output += QStringLiteral(".PACKAGE_CELL \"%1\"\n ..NUMBER_LAYERS 2\n ..PACKAGE_GROUP General\n ..MOUNT_TYPE %2\n")
                  .arg(safeName(footprint.name),
                       footprint.hasThroughHolePads() ? QStringLiteral("THROUGH") : QStringLiteral("SURFACE"));

    int pinIndex = 1;
    for (const auto& pad : footprint.pads) {
        const QString stackName = padName(pad) + (pad.isThroughHole() ? QStringLiteral("_TH") : QStringLiteral("_SMD"));
        output +=
            QStringLiteral(
                " ..PIN \"%1\"\n  ...XY (%2, %3)\n  ...PADSTACK \"%4\"\n  ...ROTATION %5\n  ...PIN_OPTIONS NONE\n")
                .arg(pad.number.isEmpty() ? QString::number(pinIndex) : pad.number)
                .arg(fmt(toTh(pad.position.x() - origin.x())))
                .arg(fmt(-toTh(pad.position.y() - origin.y())))
                .arg(stackName)
                .arg(fmt(pad.rotation));
        ++pinIndex;
    }

    const auto appendPolyline = [&](const QString& section, const QList<QPointF>& points, double width) {
        QList<QPointF> normalized;
        for (const QPointF& point : points)
            normalized.append(point - origin);
        output += QStringLiteral(" ..%1\n  ...SIDE MNT_SIDE\n").arg(section);
        output += shapeBlock(QStringLiteral("POLYLINE_PATH"), normalized, width);
    };
    for (const auto& outline : footprint.outlines)
        appendPolyline(QStringLiteral("ASSEMBLY_OUTLINE"), outline.points, outline.strokeWidth);
    for (const auto& track : footprint.tracks)
        appendPolyline(QStringLiteral("GRAPHIC"), track.points, track.width);
    for (const auto& rect : footprint.rectangles) {
        QList<QPointF> points{rect.bounds.topLeft(),
                              rect.bounds.topRight(),
                              rect.bounds.bottomRight(),
                              rect.bounds.bottomLeft(),
                              rect.bounds.topLeft()};
        appendPolyline(QStringLiteral("SILKSCREEN_OUTLINE"), points, rect.strokeWidth);
    }
    for (const auto& circle : footprint.circles) {
        output += QStringLiteral(
                      " ..SILKSCREEN_OUTLINE\n  ...SIDE MNT_SIDE\n   ...CIRCLE_PATH\n   ....WIDTH %1\n   ....XY (%2, "
                      "%3)\n   ....RADIUS %4\n")
                      .arg(fmt(toTh(circle.strokeWidth)))
                      .arg(fmt(toTh(circle.center.x() - origin.x())))
                      .arg(fmt(-toTh(circle.center.y() - origin.y())))
                      .arg(fmt(toTh(circle.radius)));
    }
    return output.toUtf8();
}

bool ExporterXpeditionFootprint::exportFootprint(const IR::FootprintComponentIR& footprint,
                                                 const QString& filePath,
                                                 const QString&) {
    return exportFootprintLibrary({footprint}, footprint.name, filePath);
}

bool ExporterXpeditionFootprint::exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
                                                        const QString&,
                                                        const QString& filePath,
                                                        bool,
                                                        bool,
                                                        const QString&,
                                                        const QString&,
                                                        bool,
                                                        const QString&) {
    m_diagnostics.clear();
    XpeditionZipWriter archive;
    QSet<QString> usedNames;
    for (const auto& footprint : footprints) {
        if (footprint.name.trimmed().isEmpty()) {
            m_diagnostics.append(QStringLiteral("跳过名称为空的 Xpedition 封装"));
            continue;
        }
        const QString baseName = safeName(footprint.name);
        QString name = baseName;
        int suffix = 2;
        while (usedNames.contains(name))
            name = QStringLiteral("%1_%2").arg(baseName).arg(suffix++);
        if (name != baseName)
            m_diagnostics.append(
                QStringLiteral("Xpedition 封装名称重复，已重命名：%1 -> %2").arg(footprint.name, name));
        usedNames.insert(name);
        if (!archive.addFile(name + QStringLiteral("_Pads.hkp"), padstackFile(footprint)) ||
            !archive.addFile(name + QStringLiteral("_Cell.hkp"), cellFile(footprint))) {
            m_diagnostics.append(QStringLiteral("Xpedition 封装文件名重复：%1").arg(footprint.name));
            return false;
        }
        if (!footprint.models3d.isEmpty())
            m_diagnostics.append(QStringLiteral("Xpedition 封装 %1 未写入 3D 模型关联").arg(footprint.name));
    }
    if (archive.write(filePath))
        return true;
    m_diagnostics.append(QStringLiteral("无法写入 Xpedition 封装 ZIP：%1").arg(QFileInfo(filePath).fileName()));
    return false;
}

}  // namespace EasyKiConverter
