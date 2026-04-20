#pragma once

#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QJsonObject>
#include <QString>

namespace EasyKiConverter {

struct CadParseResult {
    QString componentId;
    QJsonObject resultData;
    ComponentData componentData;
    QString errorMessage;
    bool success = false;
};

struct CadFetchTaskResult {
    QString componentId;
    CadParseResult parsed;
    QString errorMessage;
    bool success = false;
};

class CadDataLoader {
public:
    static CadParseResult parseCadPayload(const QString& componentId, const QJsonObject& rawData);
    static CadFetchTaskResult fetchAndParseCadData(const QString& componentId);
};

}  // namespace EasyKiConverter
