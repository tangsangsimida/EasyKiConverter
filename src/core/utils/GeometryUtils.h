#ifndef GEOMETRYUTILS_H
#define GEOMETRYUTILS_H

#include <QPointF>
#include <cmath>

namespace EasyKiConverter {

/**
 * @brief 几何计算工具类
 *
 * 提供各种几何计算函数，用于处理符号和封装的几何元素
 */
class GeometryUtils
{
public:
    /**
     * @brief 计算圆弧中间点坐标
     *
     * @param centerX 圆心 X 坐标
     * @param centerY 圆心 Y 坐标
     * @param radius 半径
     * @param angleStart 起始角度（弧度）
     * @param angleEnd 结束角度（弧度）
     * @return QPointF 中间点坐标
     */
    static QPointF getMiddleArcPos(double centerX, double centerY,
                                   double radius,
                                   double angleStart, double angleEnd);

    /**
     * @brief 计算圆弧中心点坐标
     *
     * @param startX 起点 X 坐标
     * @param startY 起点 Y 坐标
     * @param endX 终点 X 坐标
     * @param endY 终点 Y 坐标
     * @param rotationDirection 旋转方向（1: 顺时针, -1: 逆时针）
     * @param radius 半径
     * @return QPointF 圆心坐标
     */
    static QPointF getArcCenter(double startX, double startY,
                               double endX, double endY,
                               int rotationDirection, double radius);

    /**
     * @brief 计算圆弧结束角度
     *
     * @param centerX 圆心 X 坐标
     * @param endX 终点 X 坐标
     * @param radius 半径
     * @param flagLargeArc 是否为大圆弧
     * @return double 结束角度（弧度）
     */
    static double getArcAngleEnd(double centerX, double endX,
                                double radius, bool flagLargeArc);

    /**
     * @brief 将像素单位转换为 mil（千分之一英寸）
     *
     * @param dim 像素值
     * @return int 转换后的 mil 值
     */
    static int pxToMil(double dim);

    /**
     * @brief 将像素单位转换为毫米
     *
     * @param dim 像素值
     * @return double 转换后的毫米值
     */
    static double pxToMm(double dim);

    /**
     * @brief 将 EasyEDA 单位转换为毫米
     *
     * EasyEDA 使用的单位是 mil 的 10 倍，需要转换为 mm
     *
     * @param dim EasyEDA 单位值
     * @return double 转换后的毫米值
     */
    static double convertToMm(double dim);

    /**
     * @brief 计算两点之间的距离
     *
     * @param x1 第一个点的 X 坐标
     * @param y1 第一个点的 Y 坐标
     * @param x2 第二个点的 X 坐标
     * @param y2 第二个点的 Y 坐标
     * @return double 两点之间的距离
     */
    static double distance(double x1, double y1, double x2, double y2);

    /**
     * @brief 将角度从度转换为弧度
     *
     * @param degrees 角度（度）
     * @return double 角度（弧度）
     */
    static double degreesToRadians(double degrees);

    /**
     * @brief 将角度从弧度转换为度
     *
     * @param radians 角度（弧度）
     * @return double 角度（度）
     */
    static double radiansToDegrees(double radians);

    /**
     * @brief 规范化角度到 [0, 2π) 范围
     *
     * @param angle 角度（弧度）
     * @return double 规范化后的角度（弧度）
     */
    static double normalizeAngle(double angle);

    /**
     * @brief 计算 SVG 椭圆弧的圆心和角度范围
     *
     * 基于 SVG 规范的椭圆弧转换算法
     * https://www.w3.org/TR/SVG11/implnote.html#ArcConversionEndpointToCenter
     *
     * @param startX 起点 X 坐标
     * @param startY 起点 Y 坐标
     * @param radiusX X 半径
     * @param radiusY Y 半径
     * @param angle 旋转角度（度）
     * @param largeArcFlag 大圆弧标志（true=大圆弧，false=小圆弧）
     * @param sweepFlag 扫描标志（true=顺时针，false=逆时针）
     * @param endX 终点 X 坐标
     * @param endY 终点 Y 坐标
     * @param centerX 输出：圆心 X 坐标
     * @param centerY 输出：圆心 Y 坐标
     * @param angleExtent 输出：角度范围（度）
     */
    static void computeArc(double startX, double startY,
                          double radiusX, double radiusY,
                          double angle,
                          bool largeArcFlag, bool sweepFlag,
                          double endX, double endY,
                          double &centerX, double &centerY,
                          double &angleExtent);
};

} // namespace EasyKiConverter

#endif // GEOMETRYUTILS_H