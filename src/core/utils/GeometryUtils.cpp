#include "GeometryUtils.h"
#include <cmath>
#include <limits>
#include <QDebug>
#include <QRegularExpression>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace EasyKiConverter
{

    QPointF GeometryUtils::getMiddleArcPos(double centerX, double centerY,
                                           double radius,
                                           double angleStart, double angleEnd)
    {
        double middleX = centerX + radius * std::cos((angleStart + angleEnd) / 2.0);
        double middleY = centerY + radius * std::sin((angleStart + angleEnd) / 2.0);
        return QPointF(middleX, middleY);
    }

    QPointF GeometryUtils::getArcCenter(double startX, double startY,
                                        double endX, double endY,
                                        int rotationDirection, double radius)
    {
        double arcDistance = distance(startX, startY, endX, endY);

        // 检查距离是否有效
        if (arcDistance < std::numeric_limits<double>::epsilon())
        {
            qWarning() << "Arc distance is too small, cannot calculate center";
            return QPointF(startX, startY);
        }

        double mX = (startX + endX) / 2.0;
        double mY = (startY + endY) / 2.0;

        double u = (endX - startX) / arcDistance;
        double v = (endY - startY) / arcDistance;

        double hSquared = radius * radius - (arcDistance * arcDistance) / 4.0;

        // 检查 h² 是否为负数（半径太小）
        if (hSquared < 0)
        {
            qWarning() << "Radius is too small for the given arc distance";
            return QPointF(mX, mY);
        }

        double h = std::sqrt(hSquared);

        double centerX = mX - rotationDirection * h * v;
        double centerY = mY + rotationDirection * h * u;

        return QPointF(centerX, centerY);
    }

    double GeometryUtils::getArcAngleEnd(double centerX, double endX,
                                         double radius, bool flagLargeArc)
    {
        // 计算相对于圆心的角度
        double dx = endX - centerX;
        double angle = std::acos(dx / radius);

        // 根据大圆弧标志调整角度
        if (flagLargeArc)
        {
            angle = 2.0 * M_PI - angle;
        }

        return angle;
    }

    int GeometryUtils::pxToMil(double dim)
    {
        return static_cast<int>(10.0 * dim);
    }

    double GeometryUtils::pxToMm(double dim)
    {
        return 10.0 * dim * 0.0254;
    }

    double GeometryUtils::convertToMm(double dim)
    {
        // 检查输入是否有效
        if (std::isnan(dim))
        {
            qWarning() << "convertToMm: Input is NaN, returning 0.0";
            return 0.0;
        }

        if (std::isinf(dim))
        {
            qWarning() << "convertToMm: Input is infinite, returning 0.0";
            return 0.0;
        }

        // 执行单位转换
        double result = dim * 10.0 * 0.0254;

        // 检查结果是否合理（大多数元器件尺寸不会超过1米）
        constexpr double MAX_REASONABLE_SIZE = 1000.0; // 1米 = 1000mm
        if (std::abs(result) > MAX_REASONABLE_SIZE)
        {
            qWarning() << QString("convertToMm: Converted value (%1mm) exceeds reasonable range, "
                                  "original value: %2")
                              .arg(result)
                              .arg(dim);
        }

        return result;
    }

    double GeometryUtils::distance(double x1, double y1, double x2, double y2)
    {
        double dx = x2 - x1;
        double dy = y2 - y1;
        return std::sqrt(dx * dx + dy * dy);
    }

    double GeometryUtils::degreesToRadians(double degrees)
    {
        return degrees * M_PI / 180.0;
    }

    double GeometryUtils::radiansToDegrees(double radians)
    {
        return radians * 180.0 / M_PI;
    }

    double GeometryUtils::normalizeAngle(double angle)
    {
        // 将角度规范化到 [0, 2π) 范围
        while (angle < 0.0)
        {
            angle += 2.0 * M_PI;
        }
        while (angle >= 2.0 * M_PI)
        {
            angle -= 2.0 * M_PI;
        }
        return angle;
    }

    void GeometryUtils::computeArc(double startX, double startY,
                                   double radiusX, double radiusY,
                                   double angle,
                                   bool largeArcFlag, bool sweepFlag,
                                   double endX, double endY,
                                   double &centerX, double &centerY,
                                   double &angleExtent)
    {
        // Compute the half distance between the current and the final point
        double dx2 = (startX - endX) / 2.0;
        double dy2 = (startY - endY) / 2.0;

        // Convert angle from degrees to radians
        angle = degreesToRadians(std::fmod(angle, 360.0));
        double cosAngle = std::cos(angle);
        double sinAngle = std::sin(angle);

        // Step 1: Compute (x1, y1)
        double x1 = cosAngle * dx2 + sinAngle * dy2;
        double y1 = -sinAngle * dx2 + cosAngle * dy2;

        // Ensure radii are large enough
        radiusX = std::abs(radiusX);
        radiusY = std::abs(radiusY);
        double PradiusX = radiusX * radiusX;
        double PradiusY = radiusY * radiusY;
        double Px1 = x1 * x1;
        double Py1 = y1 * y1;

        // Check that radii are large enough
        double radiiCheck = 0.0;
        if (PradiusX != 0.0 && PradiusY != 0.0)
        {
            radiiCheck = Px1 / PradiusX + Py1 / PradiusY;
        }

        if (radiiCheck > 1.0)
        {
            radiusX = std::sqrt(radiiCheck) * radiusX;
            radiusY = std::sqrt(radiiCheck) * radiusY;
            PradiusX = radiusX * radiusX;
            PradiusY = radiusY * radiusY;
        }

        // Step 2: Compute (cx1, cy1)
        int sign = (largeArcFlag == sweepFlag) ? -1 : 1;
        double sq = 0.0;
        if (PradiusX * Py1 + PradiusY * Px1 > 0.0)
        {
            sq = (PradiusX * PradiusY - PradiusX * Py1 - PradiusY * Px1) /
                 (PradiusX * Py1 + PradiusY * Px1);
        }
        sq = std::max(sq, 0.0);
        double coef = sign * std::sqrt(sq);
        double cx1 = coef * ((radiusX * y1) / radiusY);
        double cy1 = (radiusX != 0.0) ? coef * -((radiusY * x1) / radiusX) : 0.0;

        // Step 3: Compute (cx, cy) from (cx1, cy1)
        double sx2 = (startX + endX) / 2.0;
        double sy2 = (startY + endY) / 2.0;
        centerX = sx2 + (cosAngle * cx1 - sinAngle * cy1);
        centerY = sy2 + (sinAngle * cx1 + cosAngle * cy1);

        // Step 4: Compute the angle_extent (dangle)
        double ux = (radiusX != 0.0) ? (x1 - cx1) / radiusX : 0.0;
        double uy = (radiusY != 0.0) ? (y1 - cy1) / radiusY : 0.0;
        double vx = (radiusX != 0.0) ? (-x1 - cx1) / radiusX : 0.0;
        double vy = (radiusY != 0.0) ? (-y1 - cy1) / radiusY : 0.0;

        // Compute the angle extent
        double n = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        double p = ux * vx + uy * vy;
        sign = ((ux * vy - uy * vx) < 0.0) ? -1 : 1;

        if (n != 0.0)
        {
            double ratio = p / n;
            if (std::abs(ratio) < 1.0)
            {
                angleExtent = radiansToDegrees(sign * std::acos(ratio));
            }
            else
            {
                angleExtent = 360.0 + 359.0;
            }
        }
        else
        {
            angleExtent = 360.0 + 359.0;
        }

        if (!sweepFlag && angleExtent > 0.0)
        {
            angleExtent -= 360.0;
        }
        else if (sweepFlag && angleExtent < 0.0)
        {
            angleExtent += 360.0;
        }

        int angleExtentSign = (angleExtent < 0.0) ? 1 : -1;
        angleExtent = (std::abs(angleExtent) / 360.0) * 360.0 * angleExtentSign;
    }

    // ============================================================================
    // SVG 弧计算相关函数
    // ============================================================================

    GeometryUtils::SvgArcResult GeometryUtils::solveSvgArc(const QString &param)
    {
        SvgArcResult res;

        // 解析 SVG 弧参数字符串
        // 格式: "M x1 y1 A rx ry phi fa fs x2 y2"
        QStringList args = param.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        if (args.size() < 10)
        {
            qWarning() << "Invalid SVG arc parameter:" << param;
            return res;
        }

        if (args[0] != "M" && args[0] != "m")
        {
            qWarning() << "SVG arc must start with M command:" << param;
            return res;
        }

        if (args[3] != "A" && args[3] != "a")
        {
            qWarning() << "Expected A command in SVG arc:" << param;
            return res;
        }

        double x1 = args[1].toDouble();
        double y1 = args[2].toDouble();
        double rx = args[4].toDouble();
        double ry = args[5].toDouble();
        double phiAngle = args[6].toDouble();
        int fa = args[7].toInt(); // large-arc-flag
        int fs = args[8].toInt(); // sweep-flag
        double x2 = args[9].toDouble();
        double y2 = (args.size() > 10) ? args[10].toDouble() : 0.0;

        res.rx = rx;
        res.ry = ry;
        res.startPt.x = x1;
        res.startPt.y = y1;
        res.endPt.x = x2;
        res.endPt.y = y2;
        res.xRotate = phiAngle;

        // 如果半径为0，退化为直线
        if (rx == 0 || ry == 0)
        {
            res.cx = (x1 + x2) / 2.0;
            res.cy = (y1 + y2) / 2.0;
            res.deltaAngle = 360.0;
            return res;
        }

        // 转换旋转角度为弧度
        double phi = degreesToRadians(phiAngle);
        double cos_phi = std::cos(phi);
        double sin_phi = std::sin(phi);

        // 计算中点
        double dx = (x1 - x2) / 2.0;
        double dy = (y1 - y2) / 2.0;

        // 变换到未旋转的坐标系
        double x1_ = cos_phi * dx + sin_phi * dy;
        double y1_ = -sin_phi * dx + cos_phi * dy;

        // 确保半径足够大
        double rx_sq = rx * rx;
        double ry_sq = ry * ry;
        double x1_sq = x1_ * x1_;
        double y1_sq = y1_ * y1_;

        double lambda = x1_sq / rx_sq + y1_sq / ry_sq;
        if (lambda > 1.0)
        {
            double sqrtLambda = std::sqrt(lambda);
            rx *= sqrtLambda;
            ry *= sqrtLambda;
            rx_sq = rx * rx;
            ry_sq = ry * ry;
        }

        // 计算圆心
        double temp = std::sqrt(std::max(0.0, (rx_sq * ry_sq - rx_sq * y1_sq - ry_sq * x1_sq) /
                                                  (rx_sq * y1_sq + ry_sq * x1_sq)));

        int sign = (fa == fs) ? -1 : 1;
        double cx_ = sign * temp * rx * y1_ / ry;
        double cy_ = -sign * temp * ry * x1_ / rx;

        // 变换回旋转的坐标系
        res.cx = cos_phi * cx_ - sin_phi * cy_ + (x1 + x2) / 2.0;
        res.cy = sin_phi * cx_ + cos_phi * cy_ + (y1 + y2) / 2.0;

        // 计算起始角度和角度增量
        double angle1 = getAngle(1.0, 0.0, (x1_ - cx_) / rx, (y1_ - cy_) / ry);
        double angle2 = getAngle((x1_ - cx_) / rx, (y1_ - cy_) / ry,
                                 (-x1_ - cx_) / rx, (-y1_ - cy_) / ry);

        res.startAngle = radiansToDegrees(angle1);
        res.deltaAngle = radiansToDegrees(angle2);

        // 规范化起始角度
        while (res.startAngle < 0)
            res.startAngle += 360.0;
        while (res.startAngle >= 360.0)
            res.startAngle -= 360.0;
        res.startAngle = std::abs(res.startAngle);

        // 根据 sweep-flag 调整角度增量
        if (fs != 0)
        {
            if (res.deltaAngle < 0)
                res.deltaAngle += 360.0;
        }
        else
        {
            if (res.deltaAngle > 0)
                res.deltaAngle -= 360.0;
        }

        return res;
    }

    double GeometryUtils::getAngle(double x1, double y1, double x2, double y2)
    {
        // angle = arccos( (U dot V) / (|U|*|V|) )
        double factor = (x1 * y2 - y1 * x2 > 0) ? 1.0 : -1.0;
        double dot = x1 * x2 + y1 * y2;
        double mag1 = std::sqrt(x1 * x1 + y1 * y1);
        double mag2 = std::sqrt(x2 * x2 + y2 * y2);

        if (mag1 == 0 || mag2 == 0)
            return 0.0;

        double ratio = dot / (mag1 * mag2);
        ratio = std::clamp(ratio, -1.0, 1.0); // 防止数值误差

        return factor * std::acos(ratio);
    }

    GeometryUtils::SvgArcEndpoints GeometryUtils::calcSvgArc(const SvgArcResult &arc)
    {
        SvgArcEndpoints res;

        double phi = degreesToRadians(arc.xRotate);
        double theta = -degreesToRadians(arc.startAngle);
        double dTheta = -degreesToRadians(arc.deltaAngle);

        // 计算起点
        res.x1 = std::cos(phi) * std::cos(theta) * arc.rx - std::sin(phi) * std::sin(theta) * arc.rx + arc.cx;
        res.y1 = std::sin(phi) * std::cos(theta) * arc.ry - std::cos(phi) * std::sin(theta) * arc.ry + arc.cy;

        // 计算终点
        theta += dTheta;
        res.x2 = std::cos(phi) * std::cos(theta) * arc.rx - std::sin(phi) * std::sin(theta) * arc.rx + arc.cx;
        res.y2 = std::sin(phi) * std::cos(theta) * arc.ry - std::cos(phi) * std::sin(theta) * arc.ry + arc.cy;

        return res;
    }

    QList<GeometryUtils::SvgPoint> GeometryUtils::arcToPath(const SvgArcResult &arc, bool includeStart)
    {
        QList<SvgPoint> res;

        const int splitCount = 32; // 与 lckiconverter 保持一致
        double step = 360.0 / splitCount;

        double startAngle = arc.startAngle;
        double deltaAngle = arc.deltaAngle;

        if (deltaAngle < 0)
            step = -step;

        while (std::abs(deltaAngle) > 0.1)
        {
            if (std::abs(deltaAngle) < std::abs(step))
            {
                step = deltaAngle;
            }

            SvgArcResult tempArc = arc;
            tempArc.startAngle = startAngle;
            tempArc.deltaAngle = step;

            SvgArcEndpoints pt = calcSvgArc(tempArc);

            if (res.isEmpty() && includeStart)
            {
                res.append(SvgPoint(pt.x1, pt.y1));
            }
            res.append(SvgPoint(pt.x2, pt.y2));

            deltaAngle -= step;
            startAngle += step;
        }

        return res;
    }

    QList<GeometryUtils::SvgPoint> GeometryUtils::parseSvgPath(const QString &path)
    {
        QList<SvgPoint> res;

        // 简化的 SVG 路径解析器
        QStringList tokens;
        QString currentToken;

        for (int i = 0; i < path.length(); ++i)
        {
            QChar c = path[i];

            if (c.isLetter())
            {
                if (!currentToken.isEmpty())
                {
                    tokens.append(currentToken);
                    currentToken.clear();
                }
                tokens.append(QString(c));
            }
            else if (c.isSpace() || c == ',')
            {
                if (!currentToken.isEmpty())
                {
                    tokens.append(currentToken);
                    currentToken.clear();
                }
            }
            else
            {
                currentToken += c;
            }
        }

        if (!currentToken.isEmpty())
        {
            tokens.append(currentToken);
        }

        double currentX = 0.0;
        double currentY = 0.0;
        double startX = 0.0;
        double startY = 0.0;

        for (int i = 0; i < tokens.size();)
        {
            QString token = tokens[i];

            if (token == "M" || token == "m")
            {
                if (i + 2 >= tokens.size())
                    break;

                double x = tokens[i + 1].toDouble();
                double y = tokens[i + 2].toDouble();

                if (token == "m")
                {
                    x += currentX;
                    y += currentY;
                }

                currentX = x;
                currentY = y;
                startX = x;
                startY = y;
                res.append(SvgPoint(x, y));

                i += 3;

                // 处理连续的点
                while (i + 2 < tokens.size())
                {
                    QString nextToken = tokens[i];
                    if (nextToken.startsWith("-") || nextToken[0].isDigit())
                    {
                        x = tokens[i].toDouble();
                        y = tokens[i + 1].toDouble();
                        currentX = x;
                        currentY = y;
                        res.append(SvgPoint(x, y));
                        i += 2;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else if (token == "L" || token == "l")
            {
                if (i + 2 >= tokens.size())
                    break;

                double x = tokens[i + 1].toDouble();
                double y = tokens[i + 2].toDouble();

                if (token == "l")
                {
                    x += currentX;
                    y += currentY;
                }

                currentX = x;
                currentY = y;
                res.append(SvgPoint(x, y));

                i += 3;

                // 处理连续的点
                while (i + 2 < tokens.size())
                {
                    QString nextToken = tokens[i];
                    if (nextToken.startsWith("-") || nextToken[0].isDigit())
                    {
                        x = tokens[i].toDouble();
                        y = tokens[i + 1].toDouble();
                        currentX = x;
                        currentY = y;
                        res.append(SvgPoint(x, y));
                        i += 2;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else if (token == "A" || token == "a")
            {
                if (i + 7 >= tokens.size())
                    break;

                double rx = tokens[i + 1].toDouble();
                double ry = tokens[i + 2].toDouble();
                double phi = tokens[i + 3].toDouble();
                int fa = tokens[i + 4].toInt();
                int fs = tokens[i + 5].toInt();
                double x = tokens[i + 6].toDouble();
                double y = tokens[i + 7].toDouble();

                if (token == "a")
                {
                    x += currentX;
                    y += currentY;
                }

                // 构造 SVG 弧参数
                QString arcParam = QString("M %1 %2 A %3 %4 %5 %6 %7 %8 %9")
                                       .arg(currentX)
                                       .arg(currentY)
                                       .arg(rx)
                                       .arg(ry)
                                       .arg(phi)
                                       .arg(fa)
                                       .arg(fs)
                                       .arg(x)
                                       .arg(y);

                SvgArcResult arc = solveSvgArc(arcParam);
                QList<SvgPoint> arcPoints = arcToPath(arc, false);

                res.append(arcPoints);

                currentX = x;
                currentY = y;

                i += 8;
            }
            else if (token == "C" || token == "c")
            {
                // 贝塞尔曲线 - 转换为多段线
                if (i + 7 >= tokens.size())
                    break;

                double cp1x = tokens[i + 1].toDouble();
                double cp1y = tokens[i + 2].toDouble();
                double cp2x = tokens[i + 3].toDouble();
                double cp2y = tokens[i + 4].toDouble();
                double x = tokens[i + 5].toDouble();
                double y = tokens[i + 6].toDouble();

                if (token == "c")
                {
                    cp1x += currentX;
                    cp1y += currentY;
                    cp2x += currentX;
                    cp2y += currentY;
                    x += currentX;
                    y += currentY;
                }

                QList<SvgPoint> bezierPoints = bezierToPolyline(currentX, currentY,
                                                                cp1x, cp1y,
                                                                cp2x, cp2y,
                                                                x, y);
                res.append(bezierPoints);

                currentX = x;
                currentY = y;

                i += 7;
            }
            else if (token == "H" || token == "h")
            {
                if (i + 1 >= tokens.size())
                    break;

                double x = tokens[i + 1].toDouble();

                if (token == "h")
                {
                    x += currentX;
                }

                currentX = x;
                res.append(SvgPoint(currentX, currentY));

                i += 2;
            }
            else if (token == "V" || token == "v")
            {
                if (i + 1 >= tokens.size())
                    break;

                double y = tokens[i + 1].toDouble();

                if (token == "v")
                {
                    y += currentY;
                }

                currentY = y;
                res.append(SvgPoint(currentX, currentY));

                i += 2;
            }
            else if (token == "Z" || token == "z")
            {
                // 闭合路径
                if (!res.isEmpty())
                {
                    res.append(SvgPoint(startX, startY));
                }
                i += 1;
            }
            else
            {
                // 未知命令，跳过
                i += 1;
            }
        }

        return res;
    }

    QList<GeometryUtils::SvgPoint> GeometryUtils::bezierToPolyline(
        double startX, double startY,
        double cp1X, double cp1Y,
        double cp2X, double cp2Y,
        double endX, double endY,
        int segments)
    {
        QList<SvgPoint> res;

        // 三次贝塞尔曲线公式
        // B(t) = (1-t)^3 * P0 + 3*(1-t)^2*t * P1 + 3*(1-t)*t^2 * P2 + t^3 * P3

        for (int i = 0; i <= segments; ++i)
        {
            double t = static_cast<double>(i) / segments;
            double t2 = t * t;
            double t3 = t2 * t;
            double mt = 1.0 - t;
            double mt2 = mt * mt;
            double mt3 = mt2 * mt;

            double x = mt3 * startX + 3.0 * mt2 * t * cp1X + 3.0 * mt * t2 * cp2X + t3 * endX;
            double y = mt3 * startY + 3.0 * mt2 * t * cp1Y + 3.0 * mt * t2 * cp2Y + t3 * endY;

            res.append(SvgPoint(x, y));
        }

        return res;
    }

    double GeometryUtils::pxToMmFloor(double px)
    {
        // 使用 floor 而非 round，与 lckiconverter 保持一致
        // 1 unit = 10 mil = 0.254 mm
        double mm = px * 0.254;
        return std::floor(mm * 100.0) / 100.0; // 保留2位小数
    }

    bool GeometryUtils::isASCII(const QString &str)
    {
        for (int i = 0; i < str.length(); ++i)
        {
            if (str[i].unicode() > 127)
            {
                return false;
            }
        }
        return true;
    }

} // namespace EasyKiConverter
