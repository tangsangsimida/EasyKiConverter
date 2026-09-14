#include "SymbolData.h"

#include "SymbolDataSerializer.h"

#include <QRegularExpression>

#include <cmath>

namespace EasyKiConverter {

SymbolData::SymbolData() : m_info(), m_bbox() {}

QJsonObject SymbolData::toJson() const {
    return SymbolDataSerializer::toJson(*this);
}

bool SymbolData::fromJson(const QJsonObject& json) {
    return SymbolDataSerializer::fromJson(*this, json);
}

bool SymbolData::isValid() const {
    return validationErrors().isEmpty();
}

QString SymbolData::validate() const {
    const QStringList errors = validationErrors();
    return errors.isEmpty() ? QString() : errors.first();
}

QStringList SymbolData::validationErrors() const {
    QStringList errors;
    const auto addError = [&](const QString& message) { errors.append(message); };
    const auto isFinite = [](double value) { return std::isfinite(value); };
    const auto validatePointList = [&](const QList<QPointF>& points, const QString& type, int index, int minimum) {
        if (points.size() < minimum) {
            addError(QString("%1 %2 has fewer than %3 points").arg(type).arg(index).arg(minimum));
            return;
        }
        for (const QPointF& point : points) {
            if (!isFinite(point.x()) || !isFinite(point.y())) {
                addError(QString("%1 %2 contains a non-finite point").arg(type).arg(index));
                break;
            }
        }
    };
    const auto validateFlatPointString = [&](const QString& value, const QString& type, int index, int minimum) {
        const QStringList parts =
            value.trimmed().split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
        if (parts.size() < minimum * 2 || parts.size() % 2 != 0) {
            addError(QString("%1 %2 has an invalid point list").arg(type).arg(index));
            return;
        }
        for (const QString& part : parts) {
            bool ok = false;
            const double coordinate = part.toDouble(&ok);
            if (!ok || !isFinite(coordinate)) {
                addError(QString("%1 %2 contains an invalid coordinate").arg(type).arg(index));
                break;
            }
        }
    };
    const auto validatePart = [&](const SymbolPart& part, int partIndex) {
        const QString prefix = QStringLiteral("Part %1 ").arg(partIndex);
        for (int i = 0; i < part.pins.size(); ++i) {
            if (part.pins[i].settings.spicePinNumber.trimmed().isEmpty())
                addError(QString("%1Pin %2 has empty number").arg(prefix).arg(i));
        }
        for (int i = 0; i < part.rectangles.size(); ++i) {
            const SymbolRectangle& rectangle = part.rectangles[i];
            if (!isFinite(rectangle.width) || !isFinite(rectangle.height) || rectangle.width <= 0.0 ||
                rectangle.height <= 0.0)
                addError(QString("%1Rectangle %2 has a non-positive size").arg(prefix).arg(i));
            if (!isFinite(rectangle.rx) || !isFinite(rectangle.ry) || rectangle.rx < 0.0 || rectangle.ry < 0.0)
                addError(QString("%1Rectangle %2 has a negative or non-finite corner radius").arg(prefix).arg(i));
        }
        for (int i = 0; i < part.circles.size(); ++i)
            if (!isFinite(part.circles[i].radius) || part.circles[i].radius <= 0.0)
                addError(QString("%1Circle %2 has a non-positive radius").arg(prefix).arg(i));
        for (int i = 0; i < part.ellipses.size(); ++i)
            if (!isFinite(part.ellipses[i].radiusX) || !isFinite(part.ellipses[i].radiusY) ||
                part.ellipses[i].radiusX <= 0.0 || part.ellipses[i].radiusY <= 0.0)
                addError(QString("%1Ellipse %2 has a non-positive radius").arg(prefix).arg(i));
        for (int i = 0; i < part.arcs.size(); ++i)
            validatePointList(part.arcs[i].path, prefix + QStringLiteral("Arc"), i, 3);
        for (int i = 0; i < part.polylines.size(); ++i)
            validateFlatPointString(part.polylines[i].points, prefix + QStringLiteral("Polyline"), i, 2);
        for (int i = 0; i < part.polygons.size(); ++i)
            validateFlatPointString(part.polygons[i].points, prefix + QStringLiteral("Polygon"), i, 3);
        for (int i = 0; i < part.paths.size(); ++i)
            if (part.paths[i].paths.trimmed().isEmpty())
                addError(QString("%1Path %2 has no commands").arg(prefix).arg(i));
        for (int i = 0; i < part.texts.size(); ++i)
            if (part.texts[i].text.trimmed().isEmpty())
                addError(QString("%1Text %2 is empty").arg(prefix).arg(i));
    };

    if (m_info.name.trimmed().isEmpty())
        addError(QStringLiteral("Symbol name is empty"));
    if (!isFinite(m_bbox.x) || !isFinite(m_bbox.y) || !isFinite(m_bbox.width) || !isFinite(m_bbox.height))
        addError(QStringLiteral("Symbol bbox contains a non-finite value"));
    else if (m_bbox.width <= 0.0 || m_bbox.height <= 0.0)
        addError(QStringLiteral("Symbol bbox is empty"));

    for (int i = 0; i < m_pins.size(); ++i) {
        if (m_pins[i].settings.spicePinNumber.trimmed().isEmpty())
            addError(QString("Pin %1 has empty number").arg(i));
    }
    for (int i = 0; i < m_rectangles.size(); ++i) {
        const SymbolRectangle& rectangle = m_rectangles[i];
        if (!isFinite(rectangle.width) || !isFinite(rectangle.height) || rectangle.width <= 0.0 ||
            rectangle.height <= 0.0)
            addError(QString("Rectangle %1 has a non-positive size").arg(i));
        if (!isFinite(rectangle.rx) || !isFinite(rectangle.ry) || rectangle.rx < 0.0 || rectangle.ry < 0.0)
            addError(QString("Rectangle %1 has a negative or non-finite corner radius").arg(i));
    }
    for (int i = 0; i < m_circles.size(); ++i) {
        if (!isFinite(m_circles[i].radius) || m_circles[i].radius <= 0.0)
            addError(QString("Circle %1 has a non-positive radius").arg(i));
    }
    for (int i = 0; i < m_ellipses.size(); ++i) {
        if (!isFinite(m_ellipses[i].radiusX) || !isFinite(m_ellipses[i].radiusY) || m_ellipses[i].radiusX <= 0.0 ||
            m_ellipses[i].radiusY <= 0.0)
            addError(QString("Ellipse %1 has a non-positive radius").arg(i));
    }
    for (int i = 0; i < m_arcs.size(); ++i)
        validatePointList(m_arcs[i].path, QStringLiteral("Arc"), i, 3);
    for (int i = 0; i < m_polylines.size(); ++i)
        validateFlatPointString(m_polylines[i].points, QStringLiteral("Polyline"), i, 2);
    for (int i = 0; i < m_polygons.size(); ++i)
        validateFlatPointString(m_polygons[i].points, QStringLiteral("Polygon"), i, 3);
    for (int i = 0; i < m_paths.size(); ++i)
        if (m_paths[i].paths.trimmed().isEmpty())
            addError(QString("Path %1 has no commands").arg(i));
    for (int i = 0; i < m_texts.size(); ++i)
        if (m_texts[i].text.trimmed().isEmpty())
            addError(QString("Text %1 is empty").arg(i));
    for (int i = 0; i < m_parts.size(); ++i)
        validatePart(m_parts[i], i);

    return errors;
}

void SymbolData::clear() {
    m_info = SymbolInfo();
    m_bbox = SymbolBBox();
    m_pins.clear();
    m_rectangles.clear();
    m_circles.clear();
    m_arcs.clear();
    m_ellipses.clear();
    m_polylines.clear();
    m_polygons.clear();
    m_paths.clear();
    m_texts.clear();
    m_parts.clear();
}

}  // namespace EasyKiConverter
