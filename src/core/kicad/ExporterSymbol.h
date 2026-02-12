#ifndef EXPORTERSYMBOL_H
#define EXPORTERSYMBOL_H

#include "models/SymbolData.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTextStream>

namespace EasyKiConverter {

/**
 * @brief KiCad 符号导出器类
 *
 * 用于EasyEDA 符号数据导出KiCad 符号库格
     */
class ExporterSymbol : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
         *
     * @param parent 父对象
         */
    explicit ExporterSymbol(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ExporterSymbol() override;

    /**
     * @brief 导出符号库KiCad 格式
     *
     * @param symbolData 符号数据
     * @param filePath 输出文件路径
     * @return bool 是否成功
     */
    bool exportSymbol(const SymbolData& symbolData, const QString& filePath);

    /**
     * @brief 导出多个符号KiCad 符号
         *
     * @param symbols 符号列表
     * @param libName 库名称
         * @param filePath 输出文件路径
     * @param appendMode 是否使用追加模式（默认true）
         * @param updateMode
     * 是否使用更新模式（默false）。如果为 true，则替换已存在的符号
     * @return bool 是否成功
     */
    bool exportSymbolLibrary(const QList<SymbolData>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false);

private:
    /**
     * @brief 生成 KiCad 符号
         *
     * @param libName 库名称
         * @return QString 头部文本
     */
    QString generateHeader(const QString& libName) const;

    /**
     * @brief 生成 KiCad 符号内容
     *
     * @param symbolData 符号数据
     * @param libName 库名称（用于 Footprint 前缀
         * @return QString 符号内容
     */
    QString generateSymbolContent(const SymbolData& symbolData, const QString& libName) const;

    /**
     * @brief 生成 KiCad 引脚
     *
     * @param pin 引脚数据
     * @param bbox 边界
         * @return QString 引脚文本
     */
    QString generatePin(const SymbolPin& pin, const SymbolBBox& bbox) const;

    /**
     * @brief 生成 KiCad 矩形
     *
     * @param rect 矩形数据
     * @return QString 矩形文本
     */
    QString generateRectangle(const SymbolRectangle& rect) const;

    /**
     * @brief 生成 KiCad 引脚
         *
     * @param circle 圆数
         * @return QString 圆文
         */
    QString generateCircle(const SymbolCircle& circle) const;

    /**
     * @brief 生成 KiCad 圆弧
     *
     * @param arc 圆弧数据
     * @return QString 圆弧文本
     */
    QString generateArc(const SymbolArc& arc) const;

    /**
     * @brief 生成 KiCad 椭圆
     *
     * @param ellipse 椭圆数据
     * @return QString 椭圆文本
     */
    QString generateEllipse(const SymbolEllipse& ellipse) const;

    /**
     * @brief 生成 KiCad 多边
         *
     * @param polygon 多边形数
         * @return QString 多边形文
         */
    QString generatePolygon(const SymbolPolygon& polygon) const;

    /**
     * @brief 生成 KiCad 多段
         *
     * @param polyline 多段线数
         * @return QString 多段线文
         */
    QString generatePolyline(const SymbolPolyline& polyline) const;

    /**
     * @brief 生成 KiCad 路径
     *
     * @param path 路径数据
     * @return QString 路径文本
     */
    QString generatePath(const SymbolPath& path) const;

    /**
     * @brief 生成 KiCad 文本
     *
     * @param text 文本数据
     * @return QString 文本文本
     */
    QString generateText(const SymbolText& text) const;

    /**
     * @brief 生成 KiCad 子符号（用于多部分符号）
     *
     * @param symbolData 符号数据
     * @param part 部分数据
     * @param symbolName 符号名称
     * @param libName 库名称
         * @param centerX 符号中心X坐标
     * @param centerY 符号中心Y坐标
     * @return QString 子符号文
         */
    QString generateSubSymbol(const SymbolData& symbolData,
                              const SymbolPart& part,
                              const QString& symbolName,
                              const QString& libName,
                              double centerX,
                              double centerY) const;

    /**
     * @brief 生成 KiCad 子符号（用于单部分符号）
     *
     * @param symbolData 符号数据
     * @param symbolName 符号名称
     * @param libName 库名称
         * @param centerX 符号中心X坐标
     * @param centerY 符号中心Y坐标
     * @return QString 子符号文
         */
    QString generateSubSymbol(const SymbolData& symbolData,
                              const QString& symbolName,
                              const QString& libName,
                              double centerX,
                              double centerY) const;

    /**
     * @brief 将像素转换为 mil
     *
     * @param px 像素
         * @return double mil 值
         */
    double pxToMil(double px) const;

    /**
     * @brief 将像素转换为毫米
     *
     * @param px 像素
         * @return double 毫米
         */
    double pxToMm(double px) const;

    /**
     * @brief 将引脚类型转换为 KiCad 引脚类型
     *
     * @param pinType EasyEDA 引脚类型
     * @return QString KiCad 引脚类型
     */
    QString pinTypeToKicad(PinType pinType) const;

    /**
     * @brief 将引脚样式转换为 KiCad 引脚样式
     *
     * @param pinStyle EasyEDA 引脚样式
     * @return QString KiCad 引脚样式
     */
    QString pinStyleToKicad(PinStyle pinStyle) const;

    /**
     * @brief 将引脚方向转换为 KiCad 引脚方向
     *
     * @param rotation 旋转角度
     * @return QString KiCad 引脚方向
     */
    QString rotationToKicadOrientation(int rotation) const;

    /**
     * @brief 计算子部分的边界框
     *
     * @param part 子部分数据
     * @return SymbolBBox 子部分的边界框
     */
    SymbolBBox calculatePartBBox(const SymbolPart& part) const;

private:
    mutable SymbolBBox m_currentBBox;  // 当前处理的边界框
};

}  // namespace EasyKiConverter

#endif  // EXPORTERSYMBOL_H
