#pragma once

#include "core/interfaces/ISymbolExporter.h"

namespace EasyKiConverter {

/**
 * @brief Xpedition 符号库导出器
 * @details 将统一符号 IR 写入 Xpedition ASCII 符号文件并打包为 ZIP。
 */
class ExporterXpeditionSymbol : public ISymbolExporter {
public:
    QString libraryFileExtension() const override;
    QStringList diagnostics() const override;
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

private:
    QString symbolFileName(const IR::SymbolComponentIR& symbol, int partIndex) const;
    QByteArray symbolFile(const IR::SymbolComponentIR& symbol, int partIndex) const;
    QStringList m_diagnostics;
};

}  // namespace EasyKiConverter
