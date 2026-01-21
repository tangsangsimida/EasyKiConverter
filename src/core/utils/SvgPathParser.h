#ifndef SVGPATHPARSER_H
#define SVGPATHPARSER_H

#include <QString>
#include <QPointF>
#include <QList>

namespace EasyKiConverter {

/**
 * @brief SVG路径解析器
 *
 * 参考lckiconverter的实现，用于解析SVG路径字符串并转换为点列表
 */
class SvgPathParser
{
public:
    /**
     * @brief 解析SVG路径字符串
     *
     * @param path SVG路径字符串
     * @return QList<QPointF> 解析后的点列表
     */
    static QList<QPointF> parsePath(const QString &path);

private:
    /**
     * @brief 分割SVG路径字符串
     *
     * @param path 路径字符串
     * @return QStringList 分割后的标记列表
     */
    static QStringList splitPath(const QString &path);

    /**
     * @brief 创建SVG点
     *
     * @param x X坐标
     * @param y Y坐标
     * @param relative 是否为相对坐标
     * @param currentX 当前X坐标
     * @param currentY 当前Y坐标
     * @return QPointF 创建的点
     */
    static QPointF createPoint(double x, double y, bool relative, double &currentX, double &currentY);

    /**
     * @brief 解析圆弧命令
     *
     * @param startPoint 起始点
     * @param rx X轴半径
     * @param ry Y轴半径
     * @param xRotation X轴旋转角度
     * @param largeArcFlag 大弧标志
     * @param sweepFlag 扫描标志
     * @param endPoint 终点
     * @return QList<QPointF> 圆弧上的点列表
     */
    static QList<QPointF> parseArc(const QPointF &startPoint, double rx, double ry, double xRotation,
                                   bool largeArcFlag, bool sweepFlag, const QPointF &endPoint);

    /**
     * @brief 计算圆弧上的点
     *
     * @param cx 圆心X坐标
     * @param cy 圆心Y坐标
     * @param rx X轴半径
     * @param ry Y轴半径
     * @param startAngle 起始角度
     * @param deltaAngle 角度增量
     * @param xRotation X轴旋转角度
     * @return QList<QPointF> 圆弧上的点列表
     */
    static QList<QPointF> calcArcPoints(double cx, double cy, double rx, double ry,
                                       double startAngle, double deltaAngle, double xRotation);

    /**
     * @brief 计算角度
     *
     * @param x1 第一个点的X坐标
     * @param y1 第一个点的Y坐标
     * @param x2 第二个点的X坐标
     * @param y2 第二个点的Y坐标
     * @return double 角度（弧度）
     */
    static double getAngle(double x1, double y1, double x2, double y2);
};

} // namespace EasyKiConverter

#endif // SVGPATHPARSER_H