#pragma once

#include "core/interfaces/ISymbolExporter.h"
#include "core/ir/SymbolIR.h"
#include "models/AltiumSchComponent.h"
#include "writers/AltiumSchLibWriter.h"

namespace EasyKiConverter {

/**
 * @brief Altium 符号导出器
 * @details 实现 ISymbolExporter 接口，将 SymbolComponentIR 转换为 Altium SchLib 格式
 */
class ExporterAltiumSymbol : public ISymbolExporter {
public:
    /**
     * @brief 获取符号库文件扩展名
     * @return ".SchLib"
     */
    QString libraryFileExtension() const override {
        return QStringLiteral(".SchLib");
    }

    /**
     * @brief 导出单个符号到 SchLib 文件
     */
    bool exportSymbol(const IR::SymbolComponentIR& symbol, const QString& filePath) override;

    /**
     * @brief 导出符号库（多个符号合并到一个 SchLib 文件）
     */
    bool exportSymbolLibrary(const QList<IR::SymbolComponentIR>& symbols,
                             const QString& libName,
                             const QString& filePath,
                             bool appendMode = true,
                             bool updateMode = false,
                             const QString& libraryDescription = QString()) override;

private:
    /**
     * @brief SymbolComponentIR → AltiumSchComponent 转换
     */
    AltiumSchComponent convertSymbol(const IR::SymbolComponentIR& data);

    /**
     * @brief SymbolPinIR → AltiumSchPin 转换
     */
    AltiumSchPin convertPin(const IR::SymbolPinIR& pin);

    /**
     * @brief SymbolRectangleIR → AltiumSchRectangle 转换
     */
    AltiumSchRectangle convertRectangle(const IR::SymbolRectangleIR& rect);

    /**
     * @brief SymbolCircleIR → AltiumSchEllipse 转换
     */
    AltiumSchEllipse convertCircle(const IR::SymbolCircleIR& circle);

    /**
     * @brief SymbolArcIR → AltiumSchArc 转换
     */
    AltiumSchArc convertArc(const IR::SymbolArcIR& arc);

    /**
     * @brief SymbolPolygonIR → AltiumSchPolygon 转换
     */
    AltiumSchPolygon convertPolygon(const IR::SymbolPolygonIR& polygon);

    /**
     * @brief SymbolPolylineIR → AltiumSchPolyline 转换
     */
    AltiumSchPolyline convertPolyline(const IR::SymbolPolylineIR& polyline);

    /**
     * @brief SymbolPathIR → AltiumSchPath 转换
     */
    AltiumSchPath convertPath(const IR::SymbolPathIR& path);

    /**
     * @brief SymbolTextIR → AltiumSchText 转换
     */
    AltiumSchText convertText(const IR::SymbolTextIR& text);

    /**
     * @brief SymbolEllipseIR → AltiumSchEllipse 转换
     */
    AltiumSchEllipse convertEllipse(const IR::SymbolEllipseIR& ellipse);

    /**
     * @brief 将符号图元坐标归一化到原点
     */
    void centerComponent(AltiumSchComponent& component);

    AltiumSchLibWriter m_writer;
};

}  // namespace EasyKiConverter
