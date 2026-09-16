#pragma once

#include "core/interfaces/IFootprintExporter.h"

namespace EasyKiConverter {

/**
 * @brief Xpedition 封装库导出器
 * @details 将统一封装 IR 写入 Pads/Cell HKP 文件并打包为 ZIP。
 */
class ExporterXpeditionFootprint : public IFootprintExporter {
public:
    QString libraryFileExtension() const override;
    bool isDirectoryOutput() const override;
    QStringList diagnostics() const override;
    bool exportFootprint(const IR::FootprintComponentIR& footprint,
                         const QString& filePath,
                         const QString& model3DPath = QString()) override;
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
    QByteArray padstackFile(const IR::FootprintComponentIR& footprint) const;
    QByteArray cellFile(const IR::FootprintComponentIR& footprint) const;
    QString safeName(QString name) const;
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
