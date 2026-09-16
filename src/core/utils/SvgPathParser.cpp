/**
 * @file SvgPathParser.cpp
 * @brief SvgPathParser 的实现。
 */
#include "SvgPathParser.h"

#include <QDebug>
#include <QRegularExpression>

#include <cmath>

namespace EasyKiConverter {

const double PI = 3.14159265358979323846;

namespace {

struct SvgArcGeometry {
    bool valid = false;
    QPointF center;
    double radiusX = 0.0;
    double radiusY = 0.0;
    double startAngle = 0.0;
    double deltaAngle = 0.0;
};

// 检查点坐标是否为有限数，避免无效几何继续参与路径计算。
bool isFinitePoint(const QPointF& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}

// 检查路径线段的端点、控制点、圆弧参数是否全部可用于后续导出。
bool isFiniteSegment(const SvgPathSegment& segment) {
    return isFinitePoint(segment.start) && isFinitePoint(segment.control1) && isFinitePoint(segment.control2) &&
           isFinitePoint(segment.arcMid) && isFinitePoint(segment.arcCenter) && isFinitePoint(segment.end) &&
           std::isfinite(segment.radiusX) && std::isfinite(segment.radiusY) && std::isfinite(segment.arcStartAngle) &&
           std::isfinite(segment.arcEndAngle);
}

SvgArcGeometry calculateArcGeometry(const QPointF& start,
                                    double radiusX,
                                    double radiusY,
                                    double xRotation,
                                    bool largeArc,
                                    bool sweep,
                                    const QPointF& end) {
    SvgArcGeometry geometry;
    radiusX = std::abs(radiusX);
    radiusY = std::abs(radiusY);
    if (radiusX <= 0.0 || radiusY <= 0.0 || start == end)
        return geometry;

    const double phi = xRotation * PI / 180.0;
    const double cosPhi = std::cos(phi);
    const double sinPhi = std::sin(phi);
    const double dx = (start.x() - end.x()) / 2.0;
    const double dy = (start.y() - end.y()) / 2.0;
    const double xPrime = cosPhi * dx + sinPhi * dy;
    const double yPrime = -sinPhi * dx + cosPhi * dy;
    double rxSquared = radiusX * radiusX;
    double rySquared = radiusY * radiusY;
    const double lambda = xPrime * xPrime / rxSquared + yPrime * yPrime / rySquared;
    if (lambda > 1.0) {
        const double scale = std::sqrt(lambda);
        radiusX *= scale;
        radiusY *= scale;
        rxSquared = radiusX * radiusX;
        rySquared = radiusY * radiusY;
    }

    const double denominator = rxSquared * yPrime * yPrime + rySquared * xPrime * xPrime;
    if (qFuzzyIsNull(denominator))
        return geometry;
    const double numerator =
        std::max(0.0, rxSquared * rySquared - rxSquared * yPrime * yPrime - rySquared * xPrime * xPrime);
    const double factor = (largeArc == sweep ? -1.0 : 1.0) * std::sqrt(numerator / denominator);
    const double centerPrimeX = factor * radiusX * yPrime / radiusY;
    const double centerPrimeY = -factor * radiusY * xPrime / radiusX;
    const QPointF center(cosPhi * centerPrimeX - sinPhi * centerPrimeY + (start.x() + end.x()) / 2.0,
                         sinPhi * centerPrimeX + cosPhi * centerPrimeY + (start.y() + end.y()) / 2.0);
    const double ux = (xPrime - centerPrimeX) / radiusX;
    const double uy = (yPrime - centerPrimeY) / radiusY;
    const double vx = (-xPrime - centerPrimeX) / radiusX;
    const double vy = (-yPrime - centerPrimeY) / radiusY;
    double deltaAngle = std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
    if (sweep && deltaAngle < 0.0)
        deltaAngle += 2.0 * PI;
    else if (!sweep && deltaAngle > 0.0)
        deltaAngle -= 2.0 * PI;

    geometry.valid = true;
    geometry.center = center;
    geometry.radiusX = radiusX;
    geometry.radiusY = radiusY;
    geometry.startAngle = std::atan2(uy, ux);
    geometry.deltaAngle = deltaAngle;
    return geometry;
}

// 将旋转椭圆圆弧按不超过九十度的区间近似为三次贝塞尔线段。
QList<SvgPathSegment> approximateRotatedEllipse(const SvgArcGeometry& geometry, double xRotation) {
    QList<SvgPathSegment> segments;
    const double segmentAngle = PI / 2.0;
    const int segmentCount = qMax(1, static_cast<int>(std::ceil(std::abs(geometry.deltaAngle) / segmentAngle)));
    const double phi = xRotation * PI / 180.0;
    const double cosPhi = std::cos(phi);
    const double sinPhi = std::sin(phi);
    const auto pointAt = [&](double angle) {
        return QPointF(geometry.center.x() + cosPhi * geometry.radiusX * std::cos(angle) -
                           sinPhi * geometry.radiusY * std::sin(angle),
                       geometry.center.y() + sinPhi * geometry.radiusX * std::cos(angle) +
                           cosPhi * geometry.radiusY * std::sin(angle));
    };
    const auto derivativeAt = [&](double angle) {
        return QPointF(-cosPhi * geometry.radiusX * std::sin(angle) - sinPhi * geometry.radiusY * std::cos(angle),
                       -sinPhi * geometry.radiusX * std::sin(angle) + cosPhi * geometry.radiusY * std::cos(angle));
    };

    for (int i = 0; i < segmentCount; ++i) {
        const double startAngle = geometry.startAngle + geometry.deltaAngle * i / segmentCount;
        const double endAngle = geometry.startAngle + geometry.deltaAngle * (i + 1) / segmentCount;
        const double deltaAngle = endAngle - startAngle;
        const double controlFactor = 4.0 / 3.0 * std::tan(deltaAngle / 4.0);
        const QPointF start = pointAt(startAngle);
        const QPointF end = pointAt(endAngle);
        const QPointF startDerivative = derivativeAt(startAngle);
        const QPointF endDerivative = derivativeAt(endAngle);
        SvgPathSegment segment;
        segment.type = SvgPathSegment::Type::CubicBezier;
        segment.start = i == 0 ? start : segments.last().end;
        segment.control1 = start + controlFactor * startDerivative;
        segment.control2 = end - controlFactor * endDerivative;
        segment.end = i == segmentCount - 1 ? pointAt(geometry.startAngle + geometry.deltaAngle) : end;
        segments.append(segment);
    }
    return segments;
}

}  // namespace

// 解析 SVG 路径并提取用于兼容旧调用方的折线点集合。
QList<QPointF> SvgPathParser::parsePath(const QString& path) {
    QList<QPointF> points;
    if (path.isEmpty()) {
        return points;
    }

    QStringList tokens = splitPath(path);
    for (const QString& token : tokens) {
        if (token.isEmpty() || token.at(0).isLetter())
            continue;
        bool ok = false;
        const double value = token.toDouble(&ok);
        if (!ok || !std::isfinite(value))
            return {};
    }
    double currentX = 0.0;
    double currentY = 0.0;
    QPointF lastCubicControl;
    QPointF lastQuadraticControl;
    QChar previousCommand;

    int i = 0;
    while (i < tokens.size()) {
        QString cmd = tokens[i];
        if (cmd.isEmpty()) {
            i++;
            continue;
        }
        // 参数只能由前面的命令消费；遇到残留数字时跳过，避免把参数误报为命令。
        if (!cmd.at(0).isLetter()) {
            i++;
            continue;
        }

        QChar command = cmd[0].toUpper();

        // 处理M/m（MoveTo）命
        if (command == 'M') {
            bool relative = (cmd[0] == 'm');
            i++;
            while (i < tokens.size()) {
                bool okX, okY;
                double x = tokens[i].toDouble(&okX);
                if (!okX)
                    break;
                i++;
                if (i >= tokens.size())
                    break;
                double y = tokens[i].toDouble(&okY);
                if (!okY)
                    break;
                i++;

                QPointF pt = createPoint(x, y, relative, currentX, currentY);
                points.append(pt);
            }
            previousCommand = 'M';
        }
        // 处理L/l（LineTo）命
        else if (command == 'L') {
            bool relative = (cmd[0] == 'l');
            i++;
            while (i < tokens.size()) {
                bool okX, okY;
                double x = tokens[i].toDouble(&okX);
                if (!okX)
                    break;
                i++;
                if (i >= tokens.size())
                    break;
                double y = tokens[i].toDouble(&okY);
                if (!okY)
                    break;
                i++;

                QPointF pt = createPoint(x, y, relative, currentX, currentY);
                points.append(pt);
            }
            previousCommand = 'L';
        }
        // 处理H/h（Horizontal LineTo）命
        else if (command == 'H') {
            bool relative = (cmd[0] == 'h');
            i++;
            while (i < tokens.size()) {
                bool ok;
                double dx = tokens[i].toDouble(&ok);
                if (!ok)
                    break;
                i++;

                if (relative) {
                    currentX += dx;
                } else {
                    currentX = dx;
                }
                points.append(QPointF(currentX, currentY));
            }
            previousCommand = 'H';
        }
        // 处理V/v（Vertical LineTo）命
        else if (command == 'V') {
            bool relative = (cmd[0] == 'v');
            i++;
            while (i < tokens.size()) {
                bool ok;
                double dy = tokens[i].toDouble(&ok);
                if (!ok)
                    break;
                i++;

                if (relative) {
                    currentY += dy;
                } else {
                    currentY = dy;
                }
                points.append(QPointF(currentX, currentY));
            }
            previousCommand = 'V';
        }
        // 处理A/a（Arc）命
        else if (command == 'A') {
            bool relative = (cmd[0] == 'a');
            if (points.isEmpty()) {
                qWarning() << "Arc without origin point";
                i++;
                continue;
            }

            QPointF startPoint = points.last();
            i++;

            if (i + 6 >= tokens.size()) {
                qWarning() << "Arc param length error";
                continue;
            }

            bool okRx, okRy, okXRot, okLarge, okSweep, okX, okY;
            double rx = tokens[i].toDouble(&okRx);
            i++;
            double ry = tokens[i].toDouble(&okRy);
            i++;
            double xRotation = tokens[i].toDouble(&okXRot);
            i++;
            int largeArcFlag = tokens[i].toInt(&okLarge);
            i++;
            int sweepFlag = tokens[i].toInt(&okSweep);
            i++;
            double endX = tokens[i].toDouble(&okX);
            i++;
            double endY = tokens[i].toDouble(&okY);
            i++;

            if (!okRx || !okRy || !okXRot || !okLarge || !okSweep || !okX || !okY) {
                qWarning() << "Arc param parse error";
                continue;
            }

            QPointF endPoint(endX, endY);
            if (relative) {
                endPoint = startPoint + QPointF(endX, endY);
            }

            QList<QPointF> arcPoints =
                parseArc(startPoint, rx, ry, xRotation, largeArcFlag != 0, sweepFlag != 0, endPoint);
            for (const QPointF& point : arcPoints) {
                if (points.isEmpty() || point != points.last())
                    points.append(point);
            }
            currentX = endPoint.x();
            currentY = endPoint.y();
            previousCommand = 'A';
        }
        // 处理C/c（Bezier Curve）命
        else if (command == 'C') {
            bool relative = (cmd[0] == 'c');
            if (points.isEmpty()) {
                qWarning() << "Bezier without origin point";
                i += 7;
                continue;
            }

            QPointF startPoint = points.last();
            i++;

            if (i + 5 >= tokens.size()) {
                qWarning() << "Bezier param length error";
                continue;
            }

            bool okCp1x, okCp1y, okCp2x, okCp2y, okX, okY;
            double cp1x = tokens[i].toDouble(&okCp1x);
            i++;
            double cp1y = tokens[i].toDouble(&okCp1y);
            i++;
            double cp2x = tokens[i].toDouble(&okCp2x);
            i++;
            double cp2y = tokens[i].toDouble(&okCp2y);
            i++;
            double endX = tokens[i].toDouble(&okX);
            i++;
            double endY = tokens[i].toDouble(&okY);
            i++;

            if (!okCp1x || !okCp1y || !okCp2x || !okCp2y || !okX || !okY) {
                qWarning() << "Bezier param parse error";
                continue;
            }

            if (relative) {
                cp1x += startPoint.x();
                cp1y += startPoint.y();
                cp2x += startPoint.x();
                cp2y += startPoint.y();
                endX += startPoint.x();
                endY += startPoint.y();
            }

            QList<QPointF> bezierPoints =
                bezierToPolyline(startPoint.x(), startPoint.y(), cp1x, cp1y, cp2x, cp2y, endX, endY);
            for (const QPointF& point : bezierPoints) {
                if (points.isEmpty() || point != points.last())
                    points.append(point);
            }
            currentX = endX;
            currentY = endY;
            lastCubicControl = QPointF(cp2x, cp2y);
            previousCommand = 'C';
        }
        // 处理S/s（平滑三次贝塞尔曲线）命令
        else if (command == 'S') {
            const bool relative = (cmd[0] == 's');
            if (points.isEmpty() || i + 4 >= tokens.size()) {
                qWarning() << "Smooth cubic bezier param length error";
                i++;
                continue;
            }
            const QPointF startPoint = points.last();
            const QPointF cp1 =
                (previousCommand == 'C' || previousCommand == 'S')
                    ? QPointF(2.0 * startPoint.x() - lastCubicControl.x(), 2.0 * startPoint.y() - lastCubicControl.y())
                    : startPoint;
            bool okCp2x = false, okCp2y = false, okX = false, okY = false;
            i++;
            double cp2x = tokens[i++].toDouble(&okCp2x);
            double cp2y = tokens[i++].toDouble(&okCp2y);
            double endX = tokens[i++].toDouble(&okX);
            double endY = tokens[i++].toDouble(&okY);
            if (!okCp2x || !okCp2y || !okX || !okY)
                continue;
            if (relative) {
                cp2x += startPoint.x();
                cp2y += startPoint.y();
                endX += startPoint.x();
                endY += startPoint.y();
            }
            const QList<QPointF> bezierPoints =
                bezierToPolyline(startPoint.x(), startPoint.y(), cp1.x(), cp1.y(), cp2x, cp2y, endX, endY);
            for (const QPointF& point : bezierPoints) {
                if (point != points.last())
                    points.append(point);
            }
            currentX = endX;
            currentY = endY;
            lastCubicControl = QPointF(cp2x, cp2y);
            previousCommand = 'S';
        }
        // 处理Q/q（二次贝塞尔曲线）命令
        else if (command == 'Q') {
            const bool relative = (cmd[0] == 'q');
            if (points.isEmpty() || i + 4 >= tokens.size()) {
                qWarning() << "Quadratic bezier param length error";
                i++;
                continue;
            }

            const QPointF startPoint = points.last();
            bool okCpX = false, okCpY = false, okEndX = false, okEndY = false;
            i++;
            double cpX = tokens[i].toDouble(&okCpX);
            i++;
            double cpY = tokens[i].toDouble(&okCpY);
            i++;
            double endX = tokens[i].toDouble(&okEndX);
            i++;
            double endY = tokens[i++].toDouble(&okEndY);
            if (!okCpX || !okCpY || !okEndX || !okEndY) {
                qWarning() << "Quadratic bezier param parse error";
                continue;
            }
            if (relative) {
                cpX += startPoint.x();
                cpY += startPoint.y();
                endX += startPoint.x();
                endY += startPoint.y();
            }
            const QPointF cp2(endX + 2.0 * (cpX - endX) / 3.0, endY + 2.0 * (cpY - endY) / 3.0);
            const QPointF cp1(startPoint.x() + 2.0 * (cpX - startPoint.x()) / 3.0,
                              startPoint.y() + 2.0 * (cpY - startPoint.y()) / 3.0);
            const QList<QPointF> bezierPoints =
                bezierToPolyline(startPoint.x(), startPoint.y(), cp1.x(), cp1.y(), cp2.x(), cp2.y(), endX, endY);
            for (const QPointF& point : bezierPoints) {
                if (points.isEmpty() || point != points.last())
                    points.append(point);
            }
            currentX = endX;
            currentY = endY;
            lastQuadraticControl = QPointF(cpX, cpY);
            previousCommand = 'Q';
        }
        // 处理T/t（平滑二次贝塞尔曲线）命令
        else if (command == 'T') {
            const bool relative = (cmd[0] == 't');
            if (points.isEmpty() || i + 2 >= tokens.size()) {
                qWarning() << "Smooth quadratic bezier param length error";
                i++;
                continue;
            }
            const QPointF startPoint = points.last();
            const QPointF control = (previousCommand == 'Q' || previousCommand == 'T')
                                        ? QPointF(2.0 * startPoint.x() - lastQuadraticControl.x(),
                                                  2.0 * startPoint.y() - lastQuadraticControl.y())
                                        : startPoint;
            bool okX = false, okY = false;
            i++;
            double endX = tokens[i++].toDouble(&okX);
            double endY = tokens[i++].toDouble(&okY);
            if (!okX || !okY)
                continue;
            if (relative) {
                endX += startPoint.x();
                endY += startPoint.y();
            }
            const QPointF cp1(startPoint.x() + 2.0 * (control.x() - startPoint.x()) / 3.0,
                              startPoint.y() + 2.0 * (control.y() - startPoint.y()) / 3.0);
            const QPointF cp2(endX + 2.0 * (control.x() - endX) / 3.0, endY + 2.0 * (control.y() - endY) / 3.0);
            const QList<QPointF> bezierPoints =
                bezierToPolyline(startPoint.x(), startPoint.y(), cp1.x(), cp1.y(), cp2.x(), cp2.y(), endX, endY);
            for (const QPointF& point : bezierPoints) {
                if (point != points.last())
                    points.append(point);
            }
            currentX = endX;
            currentY = endY;
            lastQuadraticControl = control;
            previousCommand = 'T';
        }
        // 处理Z/z（ClosePath）命
        else if (command == 'Z') {
            if (!points.isEmpty()) {
                points.append(points.first());
            }
            i++;
            previousCommand = 'Z';
        }
        // 未知命令
        else {
            qWarning() << "SVG: Unknown cmd:" << command;
            i++;
        }
    }

    for (const QPointF& point : points) {
        if (!isFinitePoint(point))
            return {};
    }
    return points;
}

// 解析 SVG 路径并保留直线、贝塞尔曲线和圆弧的结构化线段信息。
QList<SvgPathSegment> SvgPathParser::parseSegments(const QString& path) {
    QList<SvgPathSegment> segments;
    if (path.isEmpty())
        return segments;

    const QStringList tokens = splitPath(path);
    QPointF current;
    QPointF subpathStart;
    QPointF lastCubicControl;
    QPointF lastQuadraticControl;
    QChar previousCommand;
    bool hasCurrent = false;
    bool hasSubpath = false;

    auto isNumber = [&](int index) {
        if (index >= tokens.size())
            return false;
        bool ok = false;
        const double value = tokens.at(index).toDouble(&ok);
        return ok && std::isfinite(value);
    };
    auto readNumbers = [&](int& index, int count, QList<double>& values) {
        if (index + count > tokens.size())
            return false;
        values.clear();
        values.reserve(count);
        for (int n = 0; n < count; ++n) {
            bool ok = false;
            const double value = tokens.at(index++).toDouble(&ok);
            if (!ok || !std::isfinite(value))
                return false;
            values.append(value);
        }
        return true;
    };
    auto addLine = [&](const QPointF& end) {
        if (!hasCurrent) {
            current = end;
            return;
        }
        if (current == end)
            return;
        SvgPathSegment segment;
        segment.type = SvgPathSegment::Type::Line;
        segment.start = current;
        segment.end = end;
        segments.append(segment);
        current = end;
    };
    auto addCubic = [&](const QPointF& control1, const QPointF& control2, const QPointF& end) {
        if (!hasCurrent)
            return;
        SvgPathSegment segment;
        segment.type = SvgPathSegment::Type::CubicBezier;
        segment.start = current;
        segment.control1 = control1;
        segment.control2 = control2;
        segment.end = end;
        segments.append(segment);
        current = end;
    };
    auto addQuadratic = [&](const QPointF& control, const QPointF& end) {
        if (!hasCurrent)
            return;
        SvgPathSegment segment;
        segment.type = SvgPathSegment::Type::QuadraticBezier;
        segment.start = current;
        segment.control1 = control;
        segment.end = end;
        segments.append(segment);
        current = end;
    };

    int index = 0;
    while (index < tokens.size()) {
        const QString commandToken = tokens.at(index++);
        if (commandToken.isEmpty() || commandToken.at(0).isDigit() || commandToken.at(0) == '.' ||
            commandToken.at(0) == '+' || commandToken.at(0) == '-') {
            continue;
        }
        const QChar command = commandToken.at(0).toUpper();
        const bool relative = commandToken.at(0).isLower();

        if (command == 'M') {
            bool first = true;
            while (isNumber(index) && isNumber(index + 1)) {
                QList<double> values;
                if (!readNumbers(index, 2, values))
                    break;
                QPointF point(values[0], values[1]);
                if (relative && hasCurrent)
                    point += current;
                if (first) {
                    current = point;
                    subpathStart = point;
                    hasCurrent = true;
                    hasSubpath = true;
                    first = false;
                } else {
                    addLine(point);
                }
            }
            previousCommand = 'M';
            continue;
        }

        if (command == 'Z') {
            if (hasCurrent && hasSubpath)
                addLine(subpathStart);
            previousCommand = 'Z';
            continue;
        }

        if (command == 'L') {
            while (isNumber(index) && isNumber(index + 1)) {
                QList<double> values;
                if (!readNumbers(index, 2, values))
                    break;
                QPointF point(values[0], values[1]);
                if (relative)
                    point += current;
                addLine(point);
            }
            previousCommand = 'L';
            continue;
        }

        if (command == 'H' || command == 'V') {
            while (isNumber(index)) {
                QList<double> values;
                if (!readNumbers(index, 1, values))
                    break;
                QPointF point = current;
                if (command == 'H')
                    point.setX(relative ? current.x() + values[0] : values[0]);
                else
                    point.setY(relative ? current.y() + values[0] : values[0]);
                addLine(point);
            }
            previousCommand = command;
            continue;
        }

        const int groupSize = command == 'A'                       ? 7
                              : command == 'C'                     ? 6
                              : (command == 'S' || command == 'Q') ? 4
                              : command == 'T'                     ? 2
                                                                   : 0;
        if (groupSize == 0) {
            qWarning() << "SVG: Unknown segment command:" << command;
            continue;
        }

        while (isNumber(index)) {
            QList<double> values;
            if (!readNumbers(index, groupSize, values)) {
                qWarning() << "SVG: Invalid segment parameters for command:" << command;
                break;
            }
            if (!hasCurrent) {
                qWarning() << "SVG: Segment command without origin point:" << command;
                continue;
            }
            const QPointF start = current;
            if (command == 'A') {
                QPointF end(values[5], values[6]);
                if (relative)
                    end += start;
                const QList<QPointF> arcPoints =
                    parseArc(start, values[0], values[1], values[2], values[3] != 0, values[4] != 0, end);
                const bool isCircular = values[0] > 0.0 && qFuzzyCompare(values[0], values[1]);
                if (isCircular && arcPoints.size() >= 3 && start != end) {
                    SvgPathSegment segment;
                    segment.type = SvgPathSegment::Type::CircularArc;
                    segment.start = start;
                    segment.arcMid = arcPoints.at(arcPoints.size() / 2);
                    segment.end = end;
                    segments.append(segment);
                    current = end;
                } else if (!isCircular && values[0] > 0.0 && values[1] > 0.0) {
                    const SvgArcGeometry geometry = calculateArcGeometry(
                        start, values[0], values[1], values[2], values[3] != 0, values[4] != 0, end);
                    if (geometry.valid) {
                        if (qFuzzyIsNull(std::sin(values[2] * PI / 180.0))) {
                            SvgPathSegment segment;
                            segment.type = SvgPathSegment::Type::EllipticalArc;
                            segment.start = start;
                            segment.arcCenter = geometry.center;
                            segment.radiusX = geometry.radiusX;
                            segment.radiusY = geometry.radiusY;
                            segment.arcStartAngle = geometry.startAngle * 180.0 / PI;
                            segment.arcEndAngle = (geometry.startAngle + geometry.deltaAngle) * 180.0 / PI;
                            segment.end = end;
                            segments.append(segment);
                        } else {
                            segments.append(approximateRotatedEllipse(geometry, values[2]));
                        }
                        current = end;
                    } else {
                        for (const QPointF& point : arcPoints)
                            addLine(point);
                    }
                } else {
                    for (const QPointF& point : arcPoints)
                        addLine(point);
                }
            } else if (command == 'C' || command == 'S' || command == 'Q' || command == 'T') {
                QPointF control1;
                QPointF control2;
                QPointF end;
                if (command == 'C') {
                    control1 = QPointF(values[0], values[1]);
                    control2 = QPointF(values[2], values[3]);
                    end = QPointF(values[4], values[5]);
                    if (relative) {
                        control1 += start;
                        control2 += start;
                        end += start;
                    }
                } else if (command == 'S') {
                    control1 =
                        (previousCommand == 'C' || previousCommand == 'S')
                            ? QPointF(2.0 * start.x() - lastCubicControl.x(), 2.0 * start.y() - lastCubicControl.y())
                            : start;
                    control2 = QPointF(values[0], values[1]);
                    end = QPointF(values[2], values[3]);
                    if (relative) {
                        control2 += start;
                        end += start;
                    }
                } else if (command == 'Q' || command == 'T') {
                    QPointF quadraticControl;
                    if (command == 'Q') {
                        quadraticControl = QPointF(values[0], values[1]);
                        end = QPointF(values[2], values[3]);
                        if (relative) {
                            quadraticControl += start;
                            end += start;
                        }
                    } else {
                        quadraticControl = (previousCommand == 'Q' || previousCommand == 'T')
                                               ? QPointF(2.0 * start.x() - lastQuadraticControl.x(),
                                                         2.0 * start.y() - lastQuadraticControl.y())
                                               : start;
                        end = QPointF(values[0], values[1]);
                        if (relative)
                            end += start;
                    }
                    addQuadratic(quadraticControl, end);
                    lastQuadraticControl = quadraticControl;
                }
                if (command == 'C' || command == 'S') {
                    addCubic(control1, control2, end);
                }
                if (command == 'C' || command == 'S')
                    lastCubicControl = control2;
            }
            previousCommand = command;
        }
    }
    for (const SvgPathSegment& segment : segments) {
        if (!isFiniteSegment(segment))
            return {};
    }
    return segments;
}

// 规范化 SVG 命令和参数分隔符，并展开同一命令后的连续参数组。
QStringList SvgPathParser::splitPath(const QString& path) {
    // 将命令字母前后添加空格，然后按空格分
    QString processed = path;
    processed.replace(QRegularExpression("([MmZzLlHhVvCcSsQqTtAa])"), " \\1 ");
    processed.replace(QRegularExpression("(?<=[0-9.])(?=[+-])"), " ");
    const QStringList rawTokens = processed.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);

    // SVG 允许一个命令后连续跟随多组参数。当前主解析循环按一次命令消费一组
    // 曲线/圆弧参数，因此在分词阶段把后续参数组展开成同类型的重复命令。
    QStringList tokens;
    int index = 0;
    while (index < rawTokens.size()) {
        const QString command = rawTokens.at(index++);
        tokens.append(command);
        if (command.size() != 1 || !command.at(0).isLetter())
            continue;

        const QChar upper = command.at(0).toUpper();
        int groupSize = 0;
        if (upper == 'A')
            groupSize = 7;
        else if (upper == 'C')
            groupSize = 6;
        else if (upper == 'S' || upper == 'Q')
            groupSize = 4;
        else if (upper == 'T')
            groupSize = 2;
        if (groupSize == 0)
            continue;

        int parameterCount = 0;
        while (index < rawTokens.size() && !rawTokens.at(index).at(0).isLetter()) {
            if (parameterCount == groupSize) {
                tokens.append(command);
                parameterCount = 0;
            }
            tokens.append(rawTokens.at(index++));
            ++parameterCount;
        }
    }
    return tokens;
}

// 根据绝对或相对坐标更新当前路径位置并返回新的点。
QPointF SvgPathParser::createPoint(double x, double y, bool relative, double& currentX, double& currentY) {
    if (relative) {
        currentX += x;
        currentY += y;
    } else {
        currentX = x;
        currentY = y;
    }
    return QPointF(currentX, currentY);
}

QList<QPointF> SvgPathParser::parseArc(const QPointF& startPoint,
                                       double rx,
                                       double ry,
                                       double xRotation,
                                       bool largeArcFlag,
                                       bool sweepFlag,
                                       const QPointF& endPoint) {
    QList<QPointF> points;

    // SVG 规范规定起点和终点相同时不绘制弧线，避免后续圆心计算出现 0/0。
    if (startPoint == endPoint) {
        points.append(startPoint);
        return points;
    }

    // 如果半径，直接返回起点和终点
    if (rx <= 0 || ry <= 0) {
        points.append(startPoint);
        points.append(endPoint);
        return points;
    }

    // 将角度转换为弧度
    double phi = xRotation * PI / 180.0;

    // 计算中点
    double dx = (startPoint.x() - endPoint.x()) / 2.0;
    double dy = (startPoint.y() - endPoint.y()) / 2.0;

    // 旋转坐标
    double x1 = cos(phi) * dx + sin(phi) * dy;
    double y1 = -sin(phi) * dx + cos(phi) * dy;

    // 计算临时变量
    double rxSq = rx * rx;
    double rySq = ry * ry;
    double x1Sq = x1 * x1;
    double y1Sq = y1 * y1;

    // 计算校正因子
    double temp = (rxSq * rySq - rxSq * y1Sq - rySq * x1Sq) / (rxSq * y1Sq + rySq * x1Sq);
    if (temp < 0)
        temp = 0;
    temp = sqrt(temp);

    // 根据largeArcFlag和sweepFlag确定符号
    double factor = (largeArcFlag == sweepFlag) ? -1.0 : 1.0;
    double cx1 = factor * temp * rx * y1 / ry;
    double cy1 = -factor * temp * ry * x1 / rx;

    // 计算圆心
    double cx = cos(phi) * cx1 - sin(phi) * cy1 + (startPoint.x() + endPoint.x()) / 2.0;
    double cy = sin(phi) * cx1 + cos(phi) * cy1 + (startPoint.y() + endPoint.y()) / 2.0;

    // 计算起始角度和角度增
    double startAngle = getAngle(1.0, 0.0, (x1 - cx1) / rx, (y1 - cy1) / ry);
    double deltaAngle = getAngle((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);

    // 规范化角
    while (startAngle < 0)
        startAngle += 2 * PI;
    while (startAngle >= 2 * PI)
        startAngle -= 2 * PI;

    // 根据sweepFlag调整角度增量
    if (sweepFlag) {
        if (deltaAngle < 0)
            deltaAngle += 2 * PI;
    } else {
        if (deltaAngle > 0)
            deltaAngle -= 2 * PI;
    }

    // 将弧度转换为角度
    double startAngleDeg = startAngle * 180.0 / PI;
    double deltaAngleDeg = deltaAngle * 180.0 / PI;

    // 计算圆弧上的
    points = calcArcPoints(cx, cy, rx, ry, startAngleDeg, deltaAngleDeg, xRotation);

    return points;
}

QList<QPointF> SvgPathParser::calcArcPoints(double cx,
                                            double cy,
                                            double rx,
                                            double ry,
                                            double startAngle,
                                            double deltaAngle,
                                            double xRotation) {
    QList<QPointF> points;
    const int splitCount = 32;  // 分割32段
    double step = deltaAngle / splitCount;

    double phi = xRotation * PI / 180.0;

    for (int i = 0; i <= splitCount; i++) {
        double theta = (startAngle + i * step) * PI / 180.0;
        double cosTheta = cos(theta);
        double sinTheta = sin(theta);

        double x = cos(phi) * rx * cosTheta - sin(phi) * ry * sinTheta + cx;
        double y = sin(phi) * rx * cosTheta + cos(phi) * ry * sinTheta + cy;

        points.append(QPointF(x, y));
    }

    return points;
}

// 根据两个向量的点积和叉积计算带方向的夹角。
double SvgPathParser::getAngle(double x1, double y1, double x2, double y2) {
    // 计算向量点积和叉
    double dot = x1 * x2 + y1 * y2;
    double cross = x1 * y2 - y1 * x2;

    // 计算角度
    double angle = atan2(cross, dot);
    return angle;
}

QList<QPointF> SvgPathParser::bezierToPolyline(double startX,
                                               double startY,
                                               double cp1X,
                                               double cp1Y,
                                               double cp2X,
                                               double cp2Y,
                                               double endX,
                                               double endY,
                                               int segments) {
    QList<QPointF> res;

    // 三次贝塞尔曲线公式
    // B(t) = (1-t)^3 * P0 + 3*(1-t)^2*t * P1 + 3*(1-t)*t^2 * P2 + t^3 * P3

    for (int i = 0; i <= segments; ++i) {
        double t = static_cast<double>(i) / segments;
        double t2 = t * t;
        double t3 = t2 * t;
        double mt = 1.0 - t;
        double mt2 = mt * mt;
        double mt3 = mt2 * mt;

        double x = mt3 * startX + 3.0 * mt2 * t * cp1X + 3.0 * mt * t2 * cp2X + t3 * endX;
        double y = mt3 * startY + 3.0 * mt2 * t * cp1Y + 3.0 * mt * t2 * cp2Y + t3 * endY;

        res.append(QPointF(x, y));
    }

    return res;
}

}  // namespace EasyKiConverter
