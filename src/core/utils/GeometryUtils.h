#ifndef GEOMETRYUTILS_H
#define GEOMETRYUTILS_H

#include <QPointF>
#include <cmath>

namespace EasyKiConverter
{

    /**
     * @brief 几何计算工具�?
     *
     * 提供各种几何计算函数，用于处理符号和封装的几何元�?
     */
    class GeometryUtils
    {
    public:
        /**
         * @brief 计算圆弧中间点坐�?
         *
         * @param centerX 圆心 X 坐标
         * @param centerY 圆心 Y 坐标
         * @param radius 半径
         * @param angleStart 起始角度（弧度）
         * @param angleEnd 结束角度（弧度）
         * @return QPointF 中间点坐�?
         */
        static QPointF getMiddleArcPos(double centerX, double centerY,
                                       double radius,
                                       double angleStart, double angleEnd);

        /**
         * @brief 计算圆弧中心点坐�?
         *
         * @param startX 起点 X 坐标
         * @param startY 起点 Y 坐标
         * @param endX 终点 X 坐标
         * @param endY 终点 Y 坐标
         * @param rotationDirection 旋转方向�?: 顺时�? -1: 逆时针）
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
         * @brief 将像素单位转换为 mil（千分之一英寸�?
         *
         * @param dim 像素�?
         * @return int 转换后的 mil �?
         */
        static int pxToMil(double dim);

        /**
         * @brief 将像素单位转换为毫米
         *
         * @param dim 像素�?
         * @return double 转换后的毫米�?
         */
        static double pxToMm(double dim);

        /**
         * @brief �?EasyEDA 单位转换为毫�?
         *
         * EasyEDA 使用的单位是 mil �?10 倍，需要转换为 mm
         *
         * @param dim EasyEDA 单位�?
         * @return double 转换后的毫米�?
         */
        static double convertToMm(double dim);

        /**
         * @brief 计算两点之间的距�?
         *
         * @param x1 第一个点�?X 坐标
         * @param y1 第一个点�?Y 坐标
         * @param x2 第二个点�?X 坐标
         * @param y2 第二个点�?Y 坐标
         * @return double 两点之间的距�?
         */
        static double distance(double x1, double y1, double x2, double y2);

        /**
         * @brief 将角度从度转换为弧度
         *
         * @param degrees 角度（度�?
         * @return double 角度（弧度）
         */
        static double degreesToRadians(double degrees);

        /**
         * @brief 将角度从弧度转换为度
         *
         * @param radians 角度（弧度）
         * @return double 角度（度�?
         */
        static double radiansToDegrees(double radians);

        /**
         * @brief 规范化角度到 [0, 2π) 范围
         *
         * @param angle 角度（弧度）
         * @return double 规范化后的角度（弧度�?
         */
        static double normalizeAngle(double angle);

        /**
         * @brief 计算 SVG 椭圆弧的圆心和角度范�?
         *
         * 基于 SVG 规范的椭圆弧转换算法
         * https://www.w3.org/TR/SVG11/implnote.html#ArcConversionEndpointToCenter
         *
         * @param startX 起点 X 坐标
         * @param startY 起点 Y 坐标
         * @param radiusX X 半径
         * @param radiusY Y 半径
         * @param angle 旋转角度（度�?
         * @param largeArcFlag 大圆弧标志（true=大圆弧，false=小圆弧）
         * @param sweepFlag 扫描标志（true=顺时针，false=逆时针）
         * @param endX 终点 X 坐标
         * @param endY 终点 Y 坐标
         * @param centerX 输出：圆�?X 坐标
         * @param centerY 输出：圆�?Y 坐标
         * @param angleExtent 输出：角度范围（度）
         */
        static void computeArc(double startX, double startY,
                               double radiusX, double radiusY,
                               double angle,
                               bool largeArcFlag, bool sweepFlag,
                               double endX, double endY,
                               double &centerX, double &centerY,
                               double &angleExtent);

        /**
         * @brief SVG 点结构体
         */
        struct SvgPoint
        {
            double x;
            double y;

            SvgPoint(double x = 0.0, double y = 0.0) : x(x), y(y) {}
        };

        /**
         * @brief SVG 弧计算结果结构体
         */
        struct SvgArcResult
        {
            double cx;         ///< 圆心 X 坐标
            double cy;         ///< 圆心 Y 坐标
            double rx;         ///< X 半径
            double ry;         ///< Y 半径
            double startAngle; ///< 起始角度（度�?
            double deltaAngle; ///< 角度增量（度�?
            double xRotate;    ///< X 轴旋转角度（度）
            SvgPoint startPt;  ///< 起始�?
            SvgPoint endPt;    ///< 终点

            SvgArcResult() : cx(0), cy(0), rx(0), ry(0), startAngle(0), deltaAngle(0), xRotate(0) {}
        };

        /**
         * @brief SVG 弧端点计算结�?
         */
        struct SvgArcEndpoints
        {
            double x1; ///< 起点 X
            double y1; ///< 起点 Y
            double x2; ///< 终点 X
            double y2; ///< 终点 Y

            SvgArcEndpoints() : x1(0), y1(0), x2(0), y2(0) {}
        };

        /**
         * @brief 解析 SVG 弧参�?
         *
         * 参�?lckiconverter �?svg_solve_arc 函数
         * https://www.w3.org/TR/SVG/implnote.html#ArcConversionEndpointToCenter
         *
         * @param param SVG 弧参数字符串（如 "M x1 y1 A rx ry phi fa fs x2 y2"�?
         * @return SvgArcResult 弧计算结�?
         */
        static SvgArcResult solveSvgArc(const QString &param);

        /**
         * @brief 计算 SVG 弧的端点
         *
         * @param arc 弧参�?
         * @return SvgArcEndpoints 弧端�?
         */
        static SvgArcEndpoints calcSvgArc(const SvgArcResult &arc);

        /**
         * @brief �?SVG 弧转换为路径点集
         *
         * @param arc 弧参�?
         * @param includeStart 是否包含起始�?
         * @return QList<SvgPoint> 路径点集
         */
        static QList<SvgPoint> arcToPath(const SvgArcResult &arc, bool includeStart = false);

        /**
         * @brief 解析 SVG 路径字符串为点集
         *
         * 支持 M、L、A、C、H、V、Z 命令
         *
         * @param path SVG 路径字符�?
         * @return QList<SvgPoint> 点集
         */
        static QList<SvgPoint> parseSvgPath(const QString &path);

        /**
         * @brief 将贝塞尔曲线转换为多段线
         *
         * @param startX 起点 X
         * @param startY 起点 Y
         * @param cp1X 控制�? X
         * @param cp1Y 控制�? Y
         * @param cp2X 控制�? X
         * @param cp2Y 控制�? Y
         * @param endX 终点 X
         * @param endY 终点 Y
         * @param segments 分段数（默认 16�?
         * @return QList<SvgPoint> 多段线点�?
         */
        static QList<SvgPoint> bezierToPolyline(double startX, double startY,
                                                double cp1X, double cp1Y,
                                                double cp2X, double cp2Y,
                                                double endX, double endY,
                                                int segments = 16);

        /**
         * @brief 使用 floor 而非 round 进行坐标取整
         *
         * �?lckiconverter 保持一致，使用 floor 向下取整
         *
         * @param px 像素�?
         * @return double 取整后的毫米值（保留2位小数）
         */
        static double pxToMmFloor(double px);

        /**
         * @brief 检查字符串是否为纯 ASCII
         *
         * @param str 待检查的字符�?
         * @return bool 是否为纯 ASCII
         */
        static bool isASCII(const QString &str);

        /**
         * @brief 计算两个向量之间的角度（弧度�?
         *
         * @param x1 第一个向量的 X 分量
         * @param y1 第一个向量的 Y 分量
         * @param x2 第二个向量的 X 分量
         * @param y2 第二个向量的 Y 分量
         * @return double 角度（弧度）
         */
        static double getAngle(double x1, double y1, double x2, double y2);
    };

} // namespace EasyKiConverter

#endif // GEOMETRYUTILS_H
