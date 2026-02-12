#include "FootprintData.h"

#include "FootprintDataSerializer.h"


namespace EasyKiConverter {

FootprintData::FootprintData() {}

QJsonObject FootprintData::toJson() const {
    return FootprintDataSerializer::toJson(*this);
}

bool FootprintData::fromJson(const QJsonObject& json) {
    return FootprintDataSerializer::fromJson(*this, json);
}

bool FootprintData::isValid() const {
    // 检查基本信
    if (m_info.name.isEmpty()) {
        return false;
    }

    // 至少要有一个焊
    if (m_pads.isEmpty()) {
        return false;
    }

    return true;
}

QString FootprintData::validate() const {
    if (m_info.name.isEmpty()) {
        return "Footprint name is empty";
    }

    if (m_pads.isEmpty()) {
        return "Footprint must have at least one pad";
    }

    // 检查焊
    for (int i = 0; i < m_pads.size(); ++i) {
        const FootprintPad& pad = m_pads[i];
        if (pad.number.isEmpty()) {
            return QString("Pad %1 has empty number").arg(i);
        }
    }

    return QString();  // 返回空字符串表示验证通过
}

void FootprintData::clear() {
    m_info = FootprintInfo();
    m_bbox = FootprintBBox();
    m_pads.clear();
    m_tracks.clear();
    m_holes.clear();
    m_circles.clear();
    m_rectangles.clear();
    m_arcs.clear();
    m_texts.clear();
    m_solidRegions.clear();
    m_outlines.clear();
    m_layers.clear();
    m_objectVisibilities.clear();
}

}  // namespace EasyKiConverter
