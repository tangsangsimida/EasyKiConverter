#ifndef FOOTPRINTDATASERIALIZER_H
#define FOOTPRINTDATASERIALIZER_H

#include "FootprintData.h"

#include <QJsonObject>

namespace EasyKiConverter {

class FootprintDataSerializer {
public:
    static QJsonObject toJson(const FootprintData& data);
    static bool fromJson(FootprintData& data, const QJsonObject& json);

    // Helpers for internal structs
    static QJsonObject toJson(const FootprintInfo& info);
    static bool fromJson(FootprintInfo& info, const QJsonObject& json);

    static QJsonObject toJson(const FootprintBBox& bbox);
    static bool fromJson(FootprintBBox& bbox, const QJsonObject& json);

    static QJsonObject toJson(const FootprintPad& pad);
    static bool fromJson(FootprintPad& pad, const QJsonObject& json);

    static QJsonObject toJson(const FootprintTrack& track);
    static bool fromJson(FootprintTrack& track, const QJsonObject& json);

    static QJsonObject toJson(const FootprintHole& hole);
    static bool fromJson(FootprintHole& hole, const QJsonObject& json);

    static QJsonObject toJson(const FootprintCircle& circle);
    static bool fromJson(FootprintCircle& circle, const QJsonObject& json);

    static QJsonObject toJson(const FootprintRectangle& rect);
    static bool fromJson(FootprintRectangle& rect, const QJsonObject& json);

    static QJsonObject toJson(const FootprintArc& arc);
    static bool fromJson(FootprintArc& arc, const QJsonObject& json);

    static QJsonObject toJson(const FootprintText& text);
    static bool fromJson(FootprintText& text, const QJsonObject& json);

    static QJsonObject toJson(const FootprintSolidRegion& region);
    static bool fromJson(FootprintSolidRegion& region, const QJsonObject& json);

    static QJsonObject toJson(const FootprintOutline& outline);
    static bool fromJson(FootprintOutline& outline, const QJsonObject& json);

    static QJsonObject toJson(const LayerDefinition& layer);
    static bool fromJson(LayerDefinition& layer, const QJsonObject& json);

    static QJsonObject toJson(const ObjectVisibility& visibility);
    static bool fromJson(ObjectVisibility& visibility, const QJsonObject& json);
};

}  // namespace EasyKiConverter

#endif  // FOOTPRINTDATASERIALIZER_H
