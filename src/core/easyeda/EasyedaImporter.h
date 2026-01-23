#ifndef EASYEDAIMPORTER_H
#define EASYEDAIMPORTER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include "models/SymbolData.h"
#include "models/FootprintData.h"

namespace EasyKiConverter
{

    /**
     * @brief EasyEDA 数据导入器类
     *
     * 用于�?EasyEDA API 响应中导入符号、封装和 3D 模型数据
     */
    class EasyedaImporter : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函�?
         *
         * @param parent 父对�?
         */
        explicit EasyedaImporter(QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~EasyedaImporter() override;

        /**
         * @brief 导入符号数据
         *
         * @param cadData CAD 数据（来�?EasyEDA API�?
         * @return QSharedPointer<SymbolData> 符号数据
         */
        QSharedPointer<SymbolData> importSymbolData(const QJsonObject &cadData);

        /**
         * @brief 导入封装数据
         *
         * @param cadData CAD 数据（来�?EasyEDA API�?
         * @return QSharedPointer<FootprintData> 封装数据
         */
        QSharedPointer<FootprintData> importFootprintData(const QJsonObject &cadData);

        /**
         * @brief 导入引脚数据
         *
         * @param pinData 引脚数据字符�?
         * @return SymbolPin 引脚数据
         */
        SymbolPin importPinData(const QString &pinData);

        /**
         * @brief 导入矩形数据
         *
         * @param rectangleData 矩形数据字符�?
         * @return SymbolRectangle 矩形数据
         */
        SymbolRectangle importRectangleData(const QString &rectangleData);

        /**
         * @brief 导入圆数�?
         *
         * @param circleData 圆数据字符串
         * @return SymbolCircle 圆数�?
         */
        SymbolCircle importCircleData(const QString &circleData);

        /**
         * @brief 导入圆弧数据
         *
         * @param arcData 圆弧数据字符�?
         * @return SymbolArc 圆弧数据
         */
        SymbolArc importArcData(const QString &arcData);

        /**
         * @brief 导入椭圆数据
         *
         * @param ellipseData 椭圆数据字符�?
         * @return SymbolEllipse 椭圆数据
         */
        SymbolEllipse importEllipseData(const QString &ellipseData);

        /**
         * @brief 导入多段线数�?
         *
         * @param polylineData 多段线数据字符串
         * @return SymbolPolyline 多段线数�?
         */
        SymbolPolyline importPolylineData(const QString &polylineData);

        /**
         * @brief 导入多边形数�?
         *
         * @param polygonData 多边形数据字符串
         * @return SymbolPolygon 多边形数�?
         */
        SymbolPolygon importPolygonData(const QString &polygonData);

        /**
         * @brief 导入路径数据
         *
         * @param pathData 路径数据字符�?
         * @return SymbolPath 路径数据
         */
        SymbolPath importPathData(const QString &pathData);

        /**
         * @brief 导入文本数据
         *
         * @param textData 文本数据字符�?
         * @return SymbolText 文本数据
         */
        SymbolText importTextData(const QString &textData);

        /**
         * @brief 导入焊盘数据
         *
         * @param padData 焊盘数据字符�?
         * @return FootprintPad 焊盘数据
         */
        FootprintPad importPadData(const QString &padData);

        /**
         * @brief 导入走线数据
         *
         * @param trackData 走线数据字符�?
         * @return FootprintTrack 走线数据
         */
        FootprintTrack importTrackData(const QString &trackData);

        /**
         * @brief 导入孔数�?
         *
         * @param holeData 孔数据字符串
         * @return FootprintHole 孔数�?
         */
        FootprintHole importHoleData(const QString &holeData);

        /**
         * @brief 导入圆数据（封装�?
         *
         * @param circleData 圆数据字符串
         * @return FootprintCircle 圆数�?
         */
        FootprintCircle importFootprintCircleData(const QString &circleData);

        /**
         * @brief 导入矩形数据（封装）
         *
         * @param rectangleData 矩形数据字符�?
         * @return FootprintRectangle 矩形数据
         */
        FootprintRectangle importFootprintRectangleData(const QString &rectangleData);

        /**
         * @brief 导入圆弧数据（封装）
         *
         * @param arcData 圆弧数据字符�?
         * @return FootprintArc 圆弧数据
         */
        FootprintArc importFootprintArcData(const QString &arcData);

        /**
         * @brief 导入文本数据（封装）
         *
         * @param textData 文本数据字符�?
         * @return FootprintText 文本数据
         */
        FootprintText importFootprintTextData(const QString &textData);

        /**
         * @brief 导入实体填充区域数据
         *
         * @param solidRegionData 实体填充区域数据字符�?
         * @return FootprintSolidRegion 实体填充区域数据
         */
        FootprintSolidRegion importSolidRegionData(const QString &solidRegionData);

        /**
         * @brief 解析 SVGNODE 数据（区�?3D 模型和外形轮廓）
         *
         * @param svgNodeData SVGNODE 数据字符�?
         * @param footprintData 封装数据指针
         */
        void importSvgNodeData(const QString &svgNodeData, QSharedPointer<FootprintData> footprintData);

        /**
         * @brief 解析层定义数�?
         *
         * @param layerString 层定义字符串
         * @return LayerDefinition 层定义数�?
         */
        LayerDefinition parseLayerDefinition(const QString &layerString);

        /**
         * @brief 解析对象可见性配�?
         *
         * @param objectString 对象可见性字符串
         * @return ObjectVisibility 对象可见性数�?
         */
        ObjectVisibility parseObjectVisibility(const QString &objectString);

    private:
        /**
         * @brief 解析字符串数�?
         *
         * @param data 数据字符�?
         * @return QStringList 解析后的字符串列�?
         */
        QStringList parseDataString(const QString &data) const;

        /**
         * @brief 解析引脚数据字符�?
         *
         * @param pinData 引脚数据字符�?
         * @return QList<QStringList> 解析后的引脚数据
         */
        QList<QStringList> parsePinDataString(const QString &pinData) const;

        /**
         * @brief 将字符串转换为布尔�?
         *
         * @param str 字符�?
         * @return bool 布尔�?
         */
        bool stringToBool(const QString &str) const;
    };

} // namespace EasyKiConverter

#endif // EASYEDAIMPORTER_H
