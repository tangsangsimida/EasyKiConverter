#include "SymbolPinSerializer.h"

#include "SymbolData.h"

#include <QJsonArray>

namespace EasyKiConverter {

// ==================== SymbolPinSettings ====================

QJsonObject SymbolPinSerializer::toJson(const SymbolPinSettings& settings) {
    QJsonObject json;
    json["is_displayed"] = settings.isDisplayed;
    json["type"] = static_cast<int>(settings.type);
    json["spice_pin_number"] = settings.spicePinNumber;
    json["pos_x"] = settings.posX;
    json["pos_y"] = settings.posY;
    json["rotation"] = settings.rotation;
    json["id"] = settings.id;
    json["is_locked"] = settings.isLocked;
    return json;
}

bool SymbolPinSerializer::fromJson(SymbolPinSettings& settings, const QJsonObject& json) {
    if (!json.contains("type") || !json.contains("spice_pin_number")) {
        return false;
    }
    settings.isDisplayed = json["is_displayed"].toBool(true);
    settings.type = static_cast<PinType>(json["type"].toInt(0));
    settings.spicePinNumber = json["spice_pin_number"].toString();
    settings.posX = json["pos_x"].toDouble(0.0);
    settings.posY = json["pos_y"].toDouble(0.0);
    settings.rotation = json["rotation"].toInt(0);
    settings.id = json["id"].toString();
    settings.isLocked = json["is_locked"].toBool(false);
    return true;
}

// ==================== SymbolPinDot ====================

QJsonObject SymbolPinSerializer::toJson(const SymbolPinDot& dot) {
    QJsonObject json;
    json["dot_x"] = dot.dotX;
    json["dot_y"] = dot.dotY;
    return json;
}

bool SymbolPinSerializer::fromJson(SymbolPinDot& dot, const QJsonObject& json) {
    dot.dotX = json["dot_x"].toDouble(0.0);
    dot.dotY = json["dot_y"].toDouble(0.0);
    return true;
}

// ==================== SymbolPinPath ====================

QJsonObject SymbolPinSerializer::toJson(const SymbolPinPath& path) {
    QJsonObject json;
    json["path"] = path.path;
    json["color"] = path.color;
    return json;
}

bool SymbolPinSerializer::fromJson(SymbolPinPath& path, const QJsonObject& json) {
    if (!json.contains("path")) {
        return false;
    }
    path.path = json["path"].toString();
    path.color = json["color"].toString();
    return true;
}

// ==================== SymbolPinName ====================

QJsonObject SymbolPinSerializer::toJson(const SymbolPinName& name) {
    QJsonObject json;
    json["is_displayed"] = name.isDisplayed;
    json["pos_x"] = name.posX;
    json["pos_y"] = name.posY;
    json["rotation"] = name.rotation;
    json["text"] = name.text;
    json["text_anchor"] = name.textAnchor;
    json["font"] = name.font;
    json["font_size"] = name.fontSize;
    return json;
}

bool SymbolPinSerializer::fromJson(SymbolPinName& name, const QJsonObject& json) {
    if (!json.contains("text")) {
        return false;
    }
    name.isDisplayed = json["is_displayed"].toBool(true);
    name.posX = json["pos_x"].toDouble(0.0);
    name.posY = json["pos_y"].toDouble(0.0);
    name.rotation = json["rotation"].toInt(0);
    name.text = json["text"].toString();
    name.textAnchor = json["text_anchor"].toString();
    name.font = json["font"].toString();
    name.fontSize = json["font_size"].toDouble(7.0);
    return true;
}

// ==================== SymbolPinDotBis ====================

QJsonObject SymbolPinSerializer::toJson(const SymbolPinDotBis& dot) {
    QJsonObject json;
    json["is_displayed"] = dot.isDisplayed;
    json["circle_x"] = dot.circleX;
    json["circle_y"] = dot.circleY;
    return json;
}

bool SymbolPinSerializer::fromJson(SymbolPinDotBis& dot, const QJsonObject& json) {
    dot.isDisplayed = json["is_displayed"].toBool(false);
    dot.circleX = json["circle_x"].toDouble(0.0);
    dot.circleY = json["circle_y"].toDouble(0.0);
    return true;
}

// ==================== SymbolPinClock ====================

QJsonObject SymbolPinSerializer::toJson(const SymbolPinClock& clock) {
    QJsonObject json;
    json["is_displayed"] = clock.isDisplayed;
    json["path"] = clock.path;
    return json;
}

bool SymbolPinSerializer::fromJson(SymbolPinClock& clock, const QJsonObject& json) {
    clock.isDisplayed = json["is_displayed"].toBool(false);
    clock.path = json["path"].toString();
    return true;
}

// ==================== SymbolPin ====================

QJsonObject SymbolPinSerializer::toJson(const SymbolPin& pin) {
    QJsonObject json;
    json["settings"] = toJson(pin.settings);
    json["pin_dot"] = toJson(pin.pinDot);
    json["pin_path"] = toJson(pin.pinPath);
    json["name"] = toJson(pin.name);
    json["dot"] = toJson(pin.dot);
    json["clock"] = toJson(pin.clock);
    return json;
}

bool SymbolPinSerializer::fromJson(SymbolPin& pin, const QJsonObject& json) {
    if (!json.contains("settings") || !json.contains("name")) {
        return false;
    }
    if (!fromJson(pin.settings, json["settings"].toObject())) {
        return false;
    }
    fromJson(pin.pinDot, json["pin_dot"].toObject());
    fromJson(pin.pinPath, json["pin_path"].toObject());
    fromJson(pin.name, json["name"].toObject());
    fromJson(pin.dot, json["dot"].toObject());
    fromJson(pin.clock, json["clock"].toObject());
    return true;
}

}  // namespace EasyKiConverter