#include "SvgPathParser.h"
#include <QRegularExpression>
#include <QDebug>
#include <cmath>

namespace EasyKiConverter
{

    const double PI = 3.14159265358979323846;

    QList<QPointF> SvgPathParser::parsePath(const QString &path)
    {
        QList<QPointF> points;
        if (path.isEmpty())
        {
            return points;
        }

        QStringList tokens = splitPath(path);
        double currentX = 0.0;
        double currentY = 0.0;

        int i = 0;
        while (i < tokens.size())
        {
            QString cmd = tokens[i];
            if (cmd.isEmpty())
            {
                i++;
                continue;
            }

            QChar command = cmd[0].toUpper();

            // 处理M/m（MoveTo）命�?
            if (command == 'M')
            {
                bool relative = (cmd[0] == 'm');
                i++;
                while (i < tokens.size())
                {
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
            }
            // 处理L/l（LineTo）命�?
            else if (command == 'L')
            {
                bool relative = (cmd[0] == 'l');
                i++;
                while (i < tokens.size())
                {
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
            }
            // 处理H/h（Horizontal LineTo）命�?
            else if (command == 'H')
            {
                bool relative = (cmd[0] == 'h');
                i++;
                while (i < tokens.size())
                {
                    bool ok;
                    double dx = tokens[i].toDouble(&ok);
                    if (!ok)
                        break;
                    i++;

                    if (relative)
                    {
                        currentX += dx;
                    }
                    else
                    {
                        currentX = dx;
                    }
                    points.append(QPointF(currentX, currentY));
                }
            }
            // 处理V/v（Vertical LineTo）命�?
            else if (command == 'V')
            {
                bool relative = (cmd[0] == 'v');
                i++;
                while (i < tokens.size())
                {
                    bool ok;
                    double dy = tokens[i].toDouble(&ok);
                    if (!ok)
                        break;
                    i++;

                    if (relative)
                    {
                        currentY += dy;
                    }
                    else
                    {
                        currentY = dy;
                    }
                    points.append(QPointF(currentX, currentY));
                }
            }
            // 处理A/a（Arc）命�?
            else if (command == 'A')
            {
                bool relative = (cmd[0] == 'a');
                if (points.isEmpty())
                {
                    qWarning() << "Arc without origin point";
                    i++;
                    continue;
                }

                QPointF startPoint = points.last();
                i++;

                if (i + 6 >= tokens.size())
                {
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

                if (!okRx || !okRy || !okXRot || !okLarge || !okSweep || !okX || !okY)
                {
                    qWarning() << "Arc param parse error";
                    continue;
                }

                QPointF endPoint(endX, endY);
                if (relative)
                {
                    endPoint = startPoint + QPointF(endX, endY);
                }

                QList<QPointF> arcPoints = parseArc(startPoint, rx, ry, xRotation,
                                                    largeArcFlag != 0, sweepFlag != 0, endPoint);
                points.append(arcPoints);
            }
            // 处理C/c（Bezier Curve）命�?
            else if (command == 'C')
            {
                qWarning() << "Bezier curve not fully supported, skipping";
                i += 7; // 跳过6个控制点参数 + 1个终点参�?
            }
            // 处理Z/z（ClosePath）命�?
            else if (command == 'Z')
            {
                if (!points.isEmpty())
                {
                    points.append(points.first());
                }
                i++;
            }
            // 未知命令
            else
            {
                qWarning() << "SVG: Unknown cmd:" << command;
                i++;
            }
        }

        return points;
    }

    QStringList SvgPathParser::splitPath(const QString &path)
    {
        // 将命令字母前后添加空格，然后按空格分�?
        QString processed = path;
        processed.replace(QRegularExpression("([a-zA-Z])"), " \\1 ");
        QStringList tokens = processed.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
        return tokens;
    }

    QPointF SvgPathParser::createPoint(double x, double y, bool relative, double &currentX, double &currentY)
    {
        if (relative)
        {
            currentX += x;
            currentY += y;
        }
        else
        {
            currentX = x;
            currentY = y;
        }
        return QPointF(currentX, currentY);
    }

    QList<QPointF> SvgPathParser::parseArc(const QPointF &startPoint, double rx, double ry, double xRotation,
                                           bool largeArcFlag, bool sweepFlag, const QPointF &endPoint)
    {
        QList<QPointF> points;

        // 如果半径�?，直接返回起点和终点
        if (rx <= 0 || ry <= 0)
        {
            points.append(startPoint);
            points.append(endPoint);
            return points;
        }

        // 将角度转换为弧度
        double phi = xRotation * PI / 180.0;

        // 计算中点
        double dx = (startPoint.x() - endPoint.x()) / 2.0;
        double dy = (startPoint.y() - endPoint.y()) / 2.0;

        // 旋转坐标�?
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

        // 计算起始角度和角度增�?
        double startAngle = getAngle(1.0, 0.0, (x1 - cx1) / rx, (y1 - cy1) / ry);
        double deltaAngle = getAngle((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);

        // 规范化角�?
        while (startAngle < 0)
            startAngle += 2 * PI;
        while (startAngle >= 2 * PI)
            startAngle -= 2 * PI;

        // 根据sweepFlag调整角度增量
        if (sweepFlag)
        {
            if (deltaAngle < 0)
                deltaAngle += 2 * PI;
        }
        else
        {
            if (deltaAngle > 0)
                deltaAngle -= 2 * PI;
        }

        // 将弧度转换为角度
        double startAngleDeg = startAngle * 180.0 / PI;
        double deltaAngleDeg = deltaAngle * 180.0 / PI;

        // 计算圆弧上的�?
        points = calcArcPoints(cx, cy, rx, ry, startAngleDeg, deltaAngleDeg, xRotation);

        return points;
    }

    QList<QPointF> SvgPathParser::calcArcPoints(double cx, double cy, double rx, double ry,
                                                double startAngle, double deltaAngle, double xRotation)
    {
        QList<QPointF> points;
        const int splitCount = 32; // 分割�?2�?
        double step = deltaAngle / splitCount;

        double phi = xRotation * PI / 180.0;

        for (int i = 0; i <= splitCount; i++)
        {
            double theta = (startAngle + i * step) * PI / 180.0;
            double cosTheta = cos(theta);
            double sinTheta = sin(theta);

            double x = cos(phi) * rx * cosTheta - sin(phi) * ry * sinTheta + cx;
            double y = sin(phi) * rx * cosTheta + cos(phi) * ry * sinTheta + cy;

            points.append(QPointF(x, y));
        }

        return points;
    }

    double SvgPathParser::getAngle(double x1, double y1, double x2, double y2)
    {
        // 计算向量点积和叉�?
        double dot = x1 * x2 + y1 * y2;
        double cross = x1 * y2 - y1 * x2;

        // 计算角度
        double angle = atan2(cross, dot);
        return angle;
    }

} // namespace EasyKiConverter
