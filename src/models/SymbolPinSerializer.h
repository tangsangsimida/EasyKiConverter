#pragma once

#include "SymbolData.h"

namespace EasyKiConverter {

/**
 * @brief Pin类型相关的JSON序列化工具
 *
 * 负责SymbolPin及其子类型的序列化：
 * - SymbolPinSettings (引脚设置)
 * - SymbolPinDot (引脚点)
 * - SymbolPinPath (引脚路径)
 * - SymbolPinName (引脚名称)
 * - SymbolPinDotBis
 * - SymbolPinClock
 * - SymbolPin
 */
class SymbolPinSerializer {
public:
    static QJsonObject toJson(const SymbolPinSettings& settings);
    static bool fromJson(SymbolPinSettings& settings, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinDot& dot);
    static bool fromJson(SymbolPinDot& dot, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinPath& path);
    static bool fromJson(SymbolPinPath& path, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinName& name);
    static bool fromJson(SymbolPinName& name, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinDotBis& dot);
    static bool fromJson(SymbolPinDotBis& dot, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPinClock& clock);
    static bool fromJson(SymbolPinClock& clock, const QJsonObject& json);

    static QJsonObject toJson(const SymbolPin& pin);
    static bool fromJson(SymbolPin& pin, const QJsonObject& json);
};

}  // namespace EasyKiConverter