#ifndef EASYEDAFOOTPRINTIMPORTER_H
#define EASYEDAFOOTPRINTIMPORTER_H

#include "models/FootprintData.h"

#include <QJsonObject>
#include <QSharedPointer>
#include <QStringList>

namespace EasyKiConverter {

class EasyedaFootprintImporter {
public:
    EasyedaFootprintImporter();

    QSharedPointer<FootprintData> importFootprintData(const QJsonObject& cadData);

private:
    FootprintPad importPadData(const QString& padData);
    FootprintTrack importTrackData(const QString& trackData);
    FootprintHole importHoleData(const QString& holeData);
    FootprintCircle importFootprintCircleData(const QString& circleData);
    FootprintRectangle importFootprintRectangleData(const QString& rectangleData);
    FootprintArc importFootprintArcData(const QString& arcData);
    FootprintText importFootprintTextData(const QString& textData);
    FootprintSolidRegion importSolidRegionData(const QString& solidRegionData);
    void importSvgNodeData(const QString& svgNodeData, QSharedPointer<FootprintData> footprintData);

    LayerDefinition parseLayerDefinition(const QString& layerString);
    ObjectVisibility parseObjectVisibility(const QString& objectString);
};

}  // namespace EasyKiConverter

#endif  // EASYEDAFOOTPRINTIMPORTER_H
