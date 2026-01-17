#ifndef EXPORTERFOOTPRINT_H
#define EXPORTERFOOTPRINT_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QTextStream>
#include "src/models/FootprintData.h"
#include "src/models/Model3DData.h"

namespace EasyKiConverter
{

    /**
     * @brief KiCad 封装导出器类
     *
     * 用于将 EasyEDA 封装数据导出为 KiCad 封装格式
     */
    class ExporterFootprint : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函数
         *
         * @param parent 父对象
         */
        explicit ExporterFootprint(QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~ExporterFootprint() override;

        /**
         * @brief 导出封装为 KiCad 格式
         *
         * @param footprintData 封装数据
         * @param filePath 输出文件路径
         * @return bool 是否成功
         */
        bool exportFootprint(const FootprintData &footprintData, const QString &filePath, const QString &model3DPath = QString());

        /**
         * @brief 导出多个封装为 KiCad 封装库
         *
         * @param footprints 封装列表
         * @param libName 库名称
         * @param filePath 输出文件路径
         * @return bool 是否成功
         */
        bool exportFootprintLibrary(const QList<FootprintData> &footprints, const QString &libName, const QString &filePath);

    private:
        /**
         * @brief 生成 KiCad 封装头
         *
         * @param libName 库名称
         * @return QString 头部文本
         */
        QString generateHeader(const QString &libName) const;

        /**
         * @brief 生成 KiCad 封装内容
         *
         * @param footprintData 封装数据
         * @return QString 封装内容
         */
        QString generateFootprintContent(const FootprintData &footprintData, const QString &model3DPath = QString()) const;

        /**
         * @brief 生成 KiCad 焊盘
         *
         * @param pad 焊盘数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString 焊盘文本
         */
        QString generatePad(const FootprintPad &pad, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 生成 KiCad 走线
         *
         * @param track 走线数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString 走线文本
         */
        QString generateTrack(const FootprintTrack &track, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 生成 KiCad 孔
         *
         * @param hole 孔数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString 孔文本
         */
        QString generateHole(const FootprintHole &hole, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 生成 KiCad 圆（封装）
         *
         * @param circle 圆数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString 圆文本
         */
        QString generateCircle(const FootprintCircle &circle, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 生成 KiCad 矩形（封装）
         *
         * @param rectangle 矩形数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString 矩形文本
         */
        QString generateRectangle(const FootprintRectangle &rectangle, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 生成 KiCad 圆弧（封装）
         *
         * @param arc 圆弧数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString 圆弧文本
         */
        QString generateArc(const FootprintArc &arc, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 生成 KiCad 文本（封装）
         *
         * @param text 文本数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString 文本文本
         */
        QString generateText(const FootprintText &text, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 生成 KiCad 3D 模型引用
         *
         * @param model3D 3D 模型数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @param model3DPath 3D 模型路径
         * @param fpType 封装类型（"smd" 或 "tht"）
         * @return QString 3D 模型引用文本
         */
        QString generateModel3D(const Model3DData &model3D, double bboxX = 0, double bboxY = 0, const QString &model3DPath = QString(), const QString &fpType = QString()) const;

        /**
         * @brief 生成 KiCad 实体填充区域
         *
         * @param region 实体填充区域数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString 实体填充区域文本
         */
        QString generateSolidRegion(const FootprintSolidRegion &region, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 从边界框自动生成 courtyard
         *
         * @param bbox 边界框数据
         * @param bboxX 边界框中心 X 偏移
         * @param bboxY 边界框中心 Y 偏移
         * @return QString courtyard 文本
         */
        QString generateCourtyardFromBBox(const FootprintBBox &bbox, double bboxX = 0, double bboxY = 0) const;

        /**
         * @brief 将像素转换为毫米
         *
         * @param px 像素值
         * @return double 毫米值
         */
        double pxToMm(double px) const;

        /**
         * @brief 将像素转换为毫米（带四舍五入）
         *
         * @param px 像素值
         * @return double 毫米值（四舍五入到 2 位小数）
         */
        double pxToMmRounded(double px) const;

        /**
         * @brief 将焊盘形状转换为 KiCad 焊盘形状
         *
         * @param shape EasyEDA 焊盘形状
         * @return QString KiCad 焊盘形状
         */
        QString padShapeToKicad(const QString &shape) const;

        /**
         * @brief 将焊盘类型转换为 KiCad 焊盘类型
         *
         * @param type 焊盘类型
         * @return QString KiCad 焊盘类型
         */
        QString padTypeToKicad(int layerId) const;

        /**
         * @brief 将焊盘层转换为 KiCad 层
         *
         * @param layerId 层 ID
         * @return QString KiCad 层
         */
        QString padLayersToKicad(int layerId) const;

        /**
         * @brief 将层 ID 转换为 KiCad 层名称
         *
         * @param layerId 层 ID
         * @return QString KiCad 层名称
         */
        QString layerIdToKicad(int layerId) const;
    };

} // namespace EasyKiConverter

#endif // EXPORTERFOOTPRINT_H
