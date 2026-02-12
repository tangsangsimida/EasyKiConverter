#include "EasyedaImporter.h"

#include "EasyedaFootprintImporter.h"
#include "EasyedaSymbolImporter.h"


namespace EasyKiConverter {

EasyedaImporter::EasyedaImporter(QObject* parent) : QObject(parent) {}

EasyedaImporter::~EasyedaImporter() {}

QSharedPointer<SymbolData> EasyedaImporter::importSymbolData(const QJsonObject& cadData) {
    EasyedaSymbolImporter importer;
    return importer.importSymbolData(cadData);
}

QSharedPointer<FootprintData> EasyedaImporter::importFootprintData(const QJsonObject& cadData) {
    EasyedaFootprintImporter importer;
    return importer.importFootprintData(cadData);
}

}  // namespace EasyKiConverter
