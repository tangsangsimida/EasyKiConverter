#include "SymbolData.h"

#include "SymbolDataSerializer.h"

namespace EasyKiConverter {

SymbolData::SymbolData() {}

QJsonObject SymbolData::toJson() const {
    return SymbolDataSerializer::toJson(*this);
}

bool SymbolData::fromJson(const QJsonObject& json) {
    return SymbolDataSerializer::fromJson(*this, json);
}

bool SymbolData::isValid() const {
    // 检查基本字
    if (m_bbox.x == 0.0 && m_bbox.y == 0.0) {
        return false;
    }

    return true;
}

QString SymbolData::validate() const {
    // 验证符号信息
    if (m_info.name.isEmpty()) {
        return "Symbol name is empty";
    }

    // 验证边界
    if (m_bbox.x == 0.0 && m_bbox.y == 0.0) {
        return "Symbol bbox is empty";
    }

    // 验证引脚
    for (int i = 0; i < m_pins.size(); ++i) {
        const SymbolPin& pin = m_pins[i];
        if (pin.settings.spicePinNumber.isEmpty()) {
            return QString("Pin %1 has empty number").arg(i);
        }
    }

    return QString();  // 空字符串表示验证通过
}

void SymbolData::clear() {
    m_info = SymbolInfo();
    m_bbox = SymbolBBox();
    m_pins.clear();
    m_rectangles.clear();
    m_circles.clear();
    m_arcs.clear();
    m_ellipses.clear();
    m_polylines.clear();
    m_polygons.clear();
    m_paths.clear();
    m_texts.clear();
    m_parts.clear();
}

}  // namespace EasyKiConverter
