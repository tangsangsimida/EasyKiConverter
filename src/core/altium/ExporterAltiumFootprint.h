#pragma once

#include "core/interfaces/IFootprintExporter.h"
#include "core/ir/FootprintIR.h"
#include "models/AltiumPcbComponent.h"
#include "writers/AltiumPcbLibWriter.h"

namespace EasyKiConverter {

/**
 * @brief Altium 封装导出器
 * @details 实现 IFootprintExporter 接口，将 FootprintComponentIR 转换为 Altium PcbLib 格式
 */
class ExporterAltiumFootprint : public IFootprintExporter {
public:
    /**
     * @brief 获取封装库文件扩展名
     * @return ".PcbLib"
     */
    QString libraryFileExtension() const override {
        return QStringLiteral(".PcbLib");
    }

    /**
     * @brief 输出是否为目录结构
     * @return false（Altium 封装库为单个 .PcbLib 文件）
     */
    bool isDirectoryOutput() const override {
        return false;
    }

    /**
     * @brief 导出单个封装到 PcbLib 文件
     */
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;

    /**
     * @brief 导出封装库（多个封装）
     */
    bool exportFootprintLibrary(const QList<IR::FootprintComponentIR>& footprints,
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
     * @brief FootprintComponentIR → AltiumPcbComponent 转换
     */
    AltiumPcbComponent convertFootprint(const IR::FootprintComponentIR& data, const QString& model3DPath = QString());

    /**
     * @brief FootprintPadIR → AltiumPcbPad 转换
     */
    AltiumPcbPad convertPad(const IR::FootprintPadIR& pad);

    /**
     * @brief FootprintTrackIR → AltiumPcbTrack 转换
     */
    AltiumPcbTrack convertTrack(const IR::FootprintTrackIR& track);

    /**
     * @brief FootprintCircleIR → AltiumPcbArc 转换
     */
    AltiumPcbArc convertCircle(const IR::FootprintCircleIR& circle);

    /**
     * @brief FootprintArcIR → AltiumPcbArc 转换
     */
    AltiumPcbArc convertArc(const IR::FootprintArcIR& arc);

    /**
     * @brief FootprintRectangleIR → AltiumPcbFill 转换
     */
    AltiumPcbFill convertRectangle(const IR::FootprintRectangleIR& rect);

    /**
     * @brief FootprintTextIR → AltiumPcbText 转换
     */
    AltiumPcbText convertText(const IR::FootprintTextIR& text);

    /**
     * @brief FootprintRegionIR → AltiumPcbRegion 转换
     */
    AltiumPcbRegion convertSolidRegion(const IR::FootprintRegionIR& region);

    /**
     * @brief 加载 STEP 文件数据
     */
    QByteArray loadStepData(const QString& stepPath);

    AltiumPcbLibWriter m_writer;
};

}  // namespace EasyKiConverter
