#ifndef THUMBNAILGENERATOR_H
#define THUMBNAILGENERATOR_H

#include "models/ComponentData.h"

#include <QImage>
#include <QSharedPointer>

namespace EasyKiConverter {

class ThumbnailGenerator {
public:
    static QImage generateThumbnail(const QSharedPointer<ComponentData>& data, int size = 128);
    static QImage generateSymbolThumbnail(const QSharedPointer<SymbolData>& symbolData, int size);
    static QImage generateFootprintThumbnail(const QSharedPointer<FootprintData>& footprintData, int size);

private:
    static void drawSymbol(QPainter& painter, const QSharedPointer<SymbolData>& symbolData, const QRect& targetRect);
    static void drawFootprint(QPainter& painter,
                              const QSharedPointer<FootprintData>& footprintData,
                              const QRect& targetRect);
    static QRectF calculateSymbolBoundingBox(const QSharedPointer<SymbolData>& symbolData);
    static QRectF calculateFootprintBoundingBox(const QSharedPointer<FootprintData>& footprintData);
};

}  // namespace EasyKiConverter

#endif  // THUMBNAILGENERATOR_H
