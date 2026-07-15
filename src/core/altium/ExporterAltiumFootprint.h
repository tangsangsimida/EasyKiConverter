#pragma once

#include "core/interfaces/IFootprintExporter.h"
#include "models/AltiumPcbComponent.h"
#include "writers/AltiumPcbLibWriter.h"

namespace EasyKiConverter {

/**
 * @brief Altium 封装导出器
 * @details 实现 IFootprintExporter 接口，将 FootprintData 转换为 Altium PcbLib 格式
 */
class ExporterAltiumFootprint : public IFootprintExporter {
public:
    /**
     * @brief 导出单个封装到 PcbLib 文件
     */
    bool exportFootprint(const FootprintData& footprintData,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 导出封装库（多个封装）
     */
    bool exportFootprintLibrary(const QList<FootprintData>& footprints,
                                const QString& libName,
                                const QString& filePath,
                                bool preferWrl = true,
                                bool exportStep = false,
                                const QString& libraryDescription = QString(),
                                const QString& libraryKeywords = QString(),
                                bool useAbsolutePaths = false,
                                const QString& model3DBaseDir = QString()) override;

private:
    /**
     * @brief FootprintData → AltiumPcbComponent 转换
     */
    AltiumPcbComponent convertFootprint(const FootprintData& data,
                                         const QString& model3DPath = QString());

    /**
     * @brief FootprintPad → AltiumPcbPad 转换
     */
    AltiumPcbPad convertPad(const FootprintPad& pad);

    /**
     * @brief FootprintTrack → AltiumPcbTrack 转换
     */
    AltiumPcbTrack convertTrack(const FootprintTrack& track);

    /**
     * @brief FootprintCircle → AltiumPcbArc 转换
     */
    AltiumPcbArc convertCircle(const FootprintCircle& circle);

    /**
     * @brief FootprintArc → AltiumPcbArc 转换
     */
    AltiumPcbArc convertArc(const FootprintArc& arc);

    /**
     * @brief FootprintRectangle → AltiumPcbFill 转换
     */
    AltiumPcbFill convertRectangle(const FootprintRectangle& rect);

    /**
     * @brief FootprintText → AltiumPcbText 转换
     */
    AltiumPcbText convertText(const FootprintText& text);

    /**
     * @brief FootprintSolidRegion → AltiumPcbRegion 转换
     */
    AltiumPcbRegion convertSolidRegion(const FootprintSolidRegion& region);

    /**
     * @brief 加载 STEP 文件数据
     */
    QByteArray loadStepData(const QString& stepPath);

    AltiumPcbLibWriter m_writer;
};

}  // namespace EasyKiConverter
