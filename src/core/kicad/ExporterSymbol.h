#ifndef EXPORTERSYMBOL_H
#define EXPORTERSYMBOL_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QTextStream>
#include "src/models/SymbolData.h"

namespace EasyKiConverter {

/**
 * @brief KiCad 符号导出器类
 *
 * 用于将 EasyEDA 符号数据导出为 KiCad 符号库格式
 */
class ExporterSymbol : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief KiCad 版本枚举
     */
    enum class KicadVersion {
        V5 = 5,
        V6 = 6
    };
    Q_ENUM(KicadVersion)

    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit ExporterSymbol(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ExporterSymbol() override;

    /**
     * @brief 导出符号为 KiCad 格式
     *
     * @param symbolData 符号数据
     * @param filePath 输出文件路径
     * @param version KiCad 版本
     * @return bool 是否成功
     */
    bool exportSymbol(const SymbolData &symbolData, const QString &filePath, KicadVersion version = KicadVersion::V6);

    /**
     * @brief 导出多个符号为 KiCad 符号库
     *
     * @param symbols 符号列表
     * @param libName 库名称
     * @param filePath 输出文件路径
     * @param version KiCad 版本
     * @return bool 是否成功
     */
    bool exportSymbolLibrary(const QList<SymbolData> &symbols, const QString &libName, const QString &filePath, KicadVersion version = KicadVersion::V6);

    /**
     * @brief 设置 KiCad 版本
     *
     * @param version KiCad 版本
     */
    void setKicadVersion(KicadVersion version);

    /**
     * @brief 获取 KiCad 版本
     *
     * @return KicadVersion KiCad 版本
     */
    KicadVersion getKicadVersion() const;

private:
    /**
     * @brief 生成 KiCad 符号头
     *
     * @param libName 库名称
     * @param version KiCad 版本
     * @return QString 头部文本
     */
    QString generateHeader(const QString &libName, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 符号内容
     *
     * @param symbolData 符号数据
     * @param libName 库名称（用于 Footprint 前缀）
     * @param version KiCad 版本
     * @return QString 符号内容
     */
    QString generateSymbolContent(const SymbolData &symbolData, const QString &libName, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 引脚
     *
     * @param pin 引脚数据
     * @param bbox 边界框
     * @param version KiCad 版本
     * @return QString 引脚文本
     */
    QString generatePin(const SymbolPin &pin, const SymbolBBox &bbox, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 矩形
     *
     * @param rect 矩形数据
     * @param version KiCad 版本
     * @return QString 矩形文本
     */
    QString generateRectangle(const SymbolRectangle &rect, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 圆
     *
     * @param circle 圆数据
     * @param version KiCad 版本
     * @return QString 圆文本
     */
    QString generateCircle(const SymbolCircle &circle, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 圆弧
     *
     * @param arc 圆弧数据
     * @param version KiCad 版本
     * @return QString 圆弧文本
     */
    QString generateArc(const SymbolArc &arc, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 椭圆
     *
     * @param ellipse 椭圆数据
     * @param version KiCad 版本
     * @return QString 椭圆文本
     */
    QString generateEllipse(const SymbolEllipse &ellipse, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 多边形
     *
     * @param polygon 多边形数据
     * @param version KiCad 版本
     * @return QString 多边形文本
     */
    QString generatePolygon(const SymbolPolygon &polygon, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 多段线
     *
     * @param polyline 多段线数据
     * @param version KiCad 版本
     * @return QString 多段线文本
     */
    QString generatePolyline(const SymbolPolyline &polyline, KicadVersion version) const;

    /**
     * @brief 生成 KiCad 路径
     *
     * @param path 路径数据
     * @param version KiCad 版本
     * @return QString 路径文本
     */
    QString generatePath(const SymbolPath &path, KicadVersion version) const;

    /**
     * @brief 将像素转换为 mil
     *
     * @param px 像素值
     * @return double mil 值
     */
    double pxToMil(double px) const;

    /**
     * @brief 将像素转换为毫米
     *
     * @param px 像素值
     * @return double 毫米值
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
     * @brief 根据引脚名称智能推断电气类型
     *
     * @param pinName 引脚名称
     * @return PinType 推断的引脚电气类型
     */
    PinType inferPinType(const QString &pinName) const;

private:
    KicadVersion m_kicadVersion;
    mutable SymbolBBox m_currentBBox;  // 当前处理的边界框
};

} // namespace EasyKiConverter

#endif // EXPORTERSYMBOL_H