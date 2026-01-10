#include "GeometryUtils.h"
#include <cmath>
#include <limits>
#include <QDebug>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace EasyKiConverter {

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
    if (arcDistance < std::numeric_limits<double>::epsilon()) {
        qWarning() << "Arc distance is too small, cannot calculate center";
        return QPointF(startX, startY);
    }

    double mX = (startX + endX) / 2.0;
    double mY = (startY + endY) / 2.0;

    double u = (endX - startX) / arcDistance;
    double v = (endY - startY) / arcDistance;

    double hSquared = radius * radius - (arcDistance * arcDistance) / 4.0;

    // 检查 h² 是否为负数（半径太小）
    if (hSquared < 0) {
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
    if (flagLargeArc) {
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
    if (std::isnan(dim)) {
        qWarning() << "convertToMm: Input is NaN, returning 0.0";
        return 0.0;
    }

    if (std::isinf(dim)) {
        qWarning() << "convertToMm: Input is infinite, returning 0.0";
        return 0.0;
    }

    // 执行单位转换
    double result = dim * 10.0 * 0.0254;

    // 检查结果是否合理（大多数元器件尺寸不会超过1米）
    constexpr double MAX_REASONABLE_SIZE = 1000.0; // 1米 = 1000mm
    if (std::abs(result) > MAX_REASONABLE_SIZE) {
        qWarning() << QString("convertToMm: Converted value (%1mm) exceeds reasonable range, "
                           "original value: %2").arg(result).arg(dim);
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
    while (angle < 0.0) {
        angle += 2.0 * M_PI;
    }
    while (angle >= 2.0 * M_PI) {
        angle -= 2.0 * M_PI;
    }
    return angle;
}

} // namespace EasyKiConverter