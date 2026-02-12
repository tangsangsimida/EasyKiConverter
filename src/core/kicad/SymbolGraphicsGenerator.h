#ifndef SYMBOLGRAPHICSGENERATOR_H
#define SYMBOLGRAPHICSGENERATOR_H

#include "models/SymbolData.h"

#include <QString>

namespace EasyKiConverter {

/**
 * @brief 符号图形元素生成器
 *
 * 从 ExporterSymbol 中提取的图形生成函数，
 * 负责将各类符号图形元素转换为 KiCad 格式文本。
 */
class SymbolGraphicsGenerator {
public:
    SymbolGraphicsGenerator() = default;

    /**
     * @brief 设置当前边界框（用于坐标偏移计算）
     */
    void setCurrentBBox(const SymbolBBox& bbox) {
        m_currentBBox = bbox;
    }
    const SymbolBBox& currentBBox() const {
        return m_currentBBox;
    }

    // 批量生成函数
    QString generateDrawings(const SymbolData& data) const;
    QString generateDrawings(const SymbolPart& part) const;
    QString generatePins(const QList<SymbolPin>& pins, const SymbolBBox& bbox) const;

    // 单个图形元素生成函数
    QString generatePin(const SymbolPin& pin, const SymbolBBox& bbox) const;
    QString generateRectangle(const SymbolRectangle& rect) const;
    QString generateCircle(const SymbolCircle& circle) const;
    QString generateArc(const SymbolArc& arc) const;
    QString generateEllipse(const SymbolEllipse& ellipse) const;
    QString generatePolygon(const SymbolPolygon& polygon) const;
    QString generatePolyline(const SymbolPolyline& polyline) const;
    QString generatePath(const SymbolPath& path) const;
    QString generateText(const SymbolText& text) const;

    // 单位转换
    double pxToMil(double px) const;
    double pxToMm(double px) const;

    // 类型映射
    QString pinTypeToKicad(PinType pinType) const;
    QString pinStyleToKicad(PinStyle pinStyle) const;
    QString rotationToKicadOrientation(int rotation) const;

    // 边界框计算
    SymbolBBox calculatePartBBox(const SymbolPart& part) const;

private:
    SymbolBBox m_currentBBox;
};

}  // namespace EasyKiConverter

#endif  // SYMBOLGRAPHICSGENERATOR_H
