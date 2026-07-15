#pragma once

#include "core/interfaces/ISymbolExporter.h"
#include "models/AltiumSchComponent.h"
#include "writers/AltiumSchLibWriter.h"

namespace EasyKiConverter {

/**
 * @brief Altium 符号导出器
 * @details 实现 ISymbolExporter 接口，将 SymbolData 转换为 Altium SchLib 格式
 */
class ExporterAltiumSymbol : public ISymbolExporter {
public:
    /**
     * @brief 导出单个符号到 SchLib 文件
     */
    bool exportSymbol(const SymbolData& symbolData, const QString& filePath) override;

    /**
     * @brief 导出符号库（多个符号合并到一个 SchLib 文件）
     */
    bool exportSymbolLibrary(const QList<SymbolData>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

private:
    /**
     * @brief SymbolData → AltiumSchComponent 转换
     */
    AltiumSchComponent convertSymbol(const SymbolData& data);

    /**
     * @brief SymbolPin → AltiumSchPin 转换
     */
    AltiumSchPin convertPin(const SymbolPin& pin);

    /**
     * @brief SymbolRectangle → AltiumSchRectangle 转换
     */
    AltiumSchRectangle convertRectangle(const SymbolRectangle& rect);

    /**
     * @brief SymbolCircle → AltiumSchEllipse 转换
     */
    AltiumSchEllipse convertCircle(const SymbolCircle& circle);

    /**
     * @brief SymbolArc → AltiumSchArc 转换
     */
    AltiumSchArc convertArc(const SymbolArc& arc);

    /**
     * @brief SymbolPolygon → AltiumSchPolygon 转换
     */
    AltiumSchPolygon convertPolygon(const SymbolPolygon& polygon);

    /**
     * @brief SymbolPolyline → AltiumSchPolyline 转换
     */
    AltiumSchPolyline convertPolyline(const SymbolPolyline& polyline);

    /**
     * @brief SymbolPath → AltiumSchPath 转换
     */
    AltiumSchPath convertPath(const SymbolPath& path);

    /**
     * @brief SymbolText → AltiumSchText 转换
     */
    AltiumSchText convertText(const SymbolText& text);

    /**
     * @brief SymbolEllipse → AltiumSchEllipse 转换
     */
    AltiumSchEllipse convertEllipse(const SymbolEllipse& ellipse);

    AltiumSchLibWriter m_writer;
};

}  // namespace EasyKiConverter
