#include "EasyedaImporter.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace EasyKiConverter {

EasyedaImporter::EasyedaImporter(QObject* parent) : QObject(parent) {}

EasyedaImporter::~EasyedaImporter() {}

QSharedPointer<SymbolData> EasyedaImporter::importSymbolData(const QJsonObject& cadData) {
    auto symbolData = QSharedPointer<SymbolData>::create();

    SymbolInfo info;
    info.uuid = cadData["uuid"].toString();
    info.title = cadData["title"].toString();
    info.description = cadData["description"].toString();
    info.docType = QString::number(cadData["docType"].toInt());
    info.type = QString::number(cadData["type"].toInt());
    info.thumb = cadData["thumb"].toString();
    info.datastrid = cadData["datastrid"].toString();
    info.jlcOnSale = cadData["jlcOnSale"].toBool(false);
    info.writable = cadData["writable"].toBool(false);
    info.isFavorite = cadData["isFavorite"].toBool(false);
    info.verify = cadData["verify"].toBool(false);
    info.smt = cadData["SMT"].toBool(false);
    info.updateTime = cadData["updateTime"].toVariant().toLongLong();
    info.updatedAt = cadData["updated_at"].toString();

    // 导入符号信息（从 dataStr.head.c_para 中获取）
    if (cadData.contains("dataStr")) {
        QJsonObject dataStr = cadData["dataStr"].toObject();
        if (dataStr.contains("head")) {
            QJsonObject head = dataStr["head"].toObject();

            info.editorVersion = head["editorVersion"].toString();

            // 项目信息
            info.puuid = head["puuid"].toString();
            info.utime = head["utime"].toVariant().toLongLong();
            info.importFlag = head["importFlag"].toBool(false);
            info.hasIdFlag = head["hasIdFlag"].toBool(false);

            if (head.contains("c_para")) {
                QJsonObject c_para = head["c_para"].toObject();
                info.name = c_para["name"].toString();
                info.prefix = c_para["pre"].toString();
                info.package = c_para["package"].toString();
                info.manufacturer = c_para["Manufacturer"].toString();
                info.lcscId = c_para["Supplier Part"].toString();
                info.jlcId = c_para["JLCPCB Part Class"].toString();

                // 附加参数
                info.timeStamp = c_para["timeStamp"].toString();
                info.subpartNo = c_para["subpart_no"].toString();
                info.supplierPart = c_para["Supplier Part"].toString();
                info.supplier = c_para["Supplier"].toString();
                info.manufacturerPart = c_para["Manufacturer Part"].toString();
                info.jlcpcbPartClass = c_para["JLCPCB Part Class"].toString();
            }
        }

        if (cadData.contains("lcsc")) {
            QJsonObject lcsc = cadData["lcsc"].toObject();
            info.datasheet = lcsc["url"].toString();
        }
    }

    symbolData->setInfo(info);

    if (cadData.contains("dataStr")) {
        QJsonObject dataStr = cadData["dataStr"].toObject();
        SymbolBBox symbolBbox;
        if (dataStr.contains("BBox")) {
            QJsonObject bbox = dataStr["BBox"].toObject();
            symbolBbox.x = bbox["x"].toDouble();
            symbolBbox.y = bbox["y"].toDouble();
            symbolBbox.width = bbox["width"].toDouble();
            symbolBbox.height = bbox["height"].toDouble();
        } else if (dataStr.contains("head")) {
            QJsonObject head = dataStr["head"].toObject();
            symbolBbox.x = head["x"].toDouble();
            symbolBbox.y = head["y"].toDouble();
            symbolBbox.width = 0;
            symbolBbox.height = 0;
            qWarning() << "Symbol BBox not found in dataStr, using head.x/y as center point";
        }
        symbolData->setBbox(symbolBbox);
    }

    qDebug() << "=== EasyedaImporter::importSymbolData - Starting geometry data import ===";
    if (cadData.contains("dataStr")) {
        QJsonObject dataStr = cadData["dataStr"].toObject();
        qDebug() << "dataStr found, keys:" << dataStr.keys();

        qDebug() << "Checking for subparts field in cadData...";
        qDebug() << "cadData contains subparts:" << cadData.contains("subparts");

        // 检查是否有子部分（多部分符号）
        if (cadData.contains("subparts") && cadData["subparts"].isArray()) {
            QJsonArray subparts = cadData["subparts"].toArray();
            qDebug() << "Found subparts field in cadData, size:" << subparts.size();

            // 如果 subparts 数组为空，则按单部分符号处理
            if (subparts.isEmpty()) {
                qDebug() << "Subparts array is empty, treating as single-part symbol";
            } else {
                qDebug() << "Found" << subparts.size() << "subparts in symbol";

                for (int i = 0; i < subparts.size(); ++i) {
                    QJsonObject subpart = subparts[i].toObject();
                    if (subpart.contains("dataStr")) {
                        QJsonObject subpartDataStr = subpart["dataStr"].toObject();

                        SymbolPart part;
                        part.unitNumber = i;

                        // 设置子部分的坐标原点（从 EasyEDA head 字段）
                        if (subpartDataStr.contains("head")) {
                            QJsonObject head = subpartDataStr["head"].toObject();
                            part.originX = head["x"].toDouble(0.0);
                            part.originY = head["y"].toDouble(0.0);
                            qDebug() << "Subpart" << i << "origin: (" << part.originX << "," << part.originY << ")";
                        }

                        qDebug() << "Importing subpart" << i << "from" << subpart["title"].toString();

                        // 导入子部分的图形元素
                        if (subpartDataStr.contains("shape")) {
                            QJsonArray shapes = subpartDataStr["shape"].toArray();

                            for (const QJsonValue& shapeValue : shapes) {
                                QString shapeString = shapeValue.toString();
                                QStringList parts = shapeString.split("~");

                                if (parts.isEmpty())
                                    continue;

                                QString designator = parts[0];

                                if (designator == "P") {
                                    // 导入引脚
                                    SymbolPin pin = importPinData(shapeString);
                                    part.pins.append(pin);
                                } else if (designator == "R") {
                                    // 导入矩形
                                    SymbolRectangle rectangle = importRectangleData(shapeString);
                                    part.rectangles.append(rectangle);
                                } else if (designator == "C") {
                                    SymbolCircle circle = importCircleData(shapeString);
                                    part.circles.append(circle);
                                } else if (designator == "A") {
                                    SymbolArc arc = importArcData(shapeString);
                                    part.arcs.append(arc);
                                } else if (designator == "PL") {
                                    SymbolPolyline polyline = importPolylineData(shapeString);
                                    part.polylines.append(polyline);
                                } else if (designator == "PG") {
                                    SymbolPolygon polygon = importPolygonData(shapeString);
                                    part.polygons.append(polygon);
                                } else if (designator == "PT") {
                                    SymbolPath path = importPathData(shapeString);
                                    part.paths.append(path);
                                } else if (designator == "T") {
                                    SymbolText text = importTextData(shapeString);
                                    part.texts.append(text);
                                } else if (designator == "E") {
                                    SymbolEllipse ellipse = importEllipseData(shapeString);
                                    part.ellipses.append(ellipse);
                                }
                            }
                        }

                        symbolData->addPart(part);
                        qDebug() << "Imported subpart" << i << "with" << part.pins.size() << "pins,"
                                 << part.rectangles.size() << "rectangles";
                    }
                }

                if (!subparts.isEmpty()) {
                    qDebug() << "Multi-part symbol processed, skipping single-part symbol processing";
                    return symbolData;
                }
            }
        } else {
            qDebug() << "No subparts field found in cadData, processing as single-part symbol";
        }

        // 单部分符号：导入主符号的图形元素
        qDebug() << "=== Starting single-part symbol processing ===";
        if (dataStr.contains("shape")) {
            QJsonArray shapes = dataStr["shape"].toArray();
            qDebug() << "=== Symbol Shape Parsing ===";
            qDebug() << "Total shapes in dataStr.shape:" << shapes.size();

            for (const QJsonValue& shapeValue : shapes) {
                QString shapeString = shapeValue.toString();
                QStringList parts = shapeString.split("~");

                if (parts.isEmpty())
                    continue;

                QString designator = parts[0];
                qDebug() << "Processing shape with designator:" << designator;

                if (designator == "P") {
                    qDebug() << "  -> Processing pin data...";
                    // 导入引脚
                    SymbolPin pin = importPinData(shapeString);
                    qDebug() << "  -> Pin parsed, name:" << pin.name.text;
                    symbolData->addPin(pin);
                    qDebug() << "  -> Added pin:" << pin.name.text;
                } else if (designator == "R") {
                    // 导入矩形
                    SymbolRectangle rectangle = importRectangleData(shapeString);
                    symbolData->addRectangle(rectangle);
                    qDebug() << "  -> Added rectangle";
                } else if (designator == "C") {
                    SymbolCircle circle = importCircleData(shapeString);
                    symbolData->addCircle(circle);
                    qDebug() << "  -> Added circle";
                } else if (designator == "A") {
                    SymbolArc arc = importArcData(shapeString);
                    symbolData->addArc(arc);
                    qDebug() << "  -> Added arc";
                } else if (designator == "PL") {
                    SymbolPolyline polyline = importPolylineData(shapeString);
                    symbolData->addPolyline(polyline);
                    qDebug() << "  -> Added polyline";
                } else if (designator == "PG") {
                    SymbolPolygon polygon = importPolygonData(shapeString);
                    symbolData->addPolygon(polygon);
                    qDebug() << "  -> Added polygon";
                } else if (designator == "PT") {
                    SymbolPath path = importPathData(shapeString);
                    symbolData->addPath(path);
                    qDebug() << "  -> Added path";
                } else if (designator == "T") {
                    SymbolText text = importTextData(shapeString);
                    symbolData->addText(text);
                    qDebug() << "  -> Added text:" << text.text;
                } else if (designator == "E") {
                    SymbolEllipse ellipse = importEllipseData(shapeString);
                    symbolData->addEllipse(ellipse);
                    qDebug() << "  -> Added ellipse";
                } else {
                    qDebug() << "  -> Unknown designator:" << designator;
                }
            }

            qDebug() << "Symbol shape parsing completed";
            qDebug() << "Final counts - Pins:" << symbolData->pins().size()
                     << "Rectangles:" << symbolData->rectangles().size() << "Circles:" << symbolData->circles().size();
            qDebug() << "============================";
        } else {
            qDebug() << "WARNING: dataStr does not contain 'shape' field!";
            qDebug() << "Available keys in dataStr:" << dataStr.keys();
        }
    }

    return symbolData;
}
QSharedPointer<FootprintData> EasyedaImporter::importFootprintData(const QJsonObject& cadData) {
    auto footprintData = QSharedPointer<FootprintData>::create();

    // 导入封装信息（从 packageDetail 中获取）
    if (cadData.contains("packageDetail")) {
        QJsonObject packageDetail = cadData["packageDetail"].toObject();

        // packageDetail 顶层字段
        FootprintInfo info;
        info.uuid = packageDetail["uuid"].toString();
        info.docType = QString::number(packageDetail["docType"].toInt());
        info.datastrid = packageDetail["datastrid"].toString();
        info.writable = packageDetail["writable"].toBool(false);
        info.updateTime = packageDetail["updateTime"].toVariant().toLongLong();

        if (packageDetail.contains("dataStr")) {
            QJsonObject dataStr = packageDetail["dataStr"].toObject();

            if (dataStr.contains("head")) {
                QJsonObject head = dataStr["head"].toObject();

                info.editorVersion = head["editorVersion"].toString();

                info.puuid = head["puuid"].toString();
                info.utime = head["utime"].toVariant().toLongLong();
                info.importFlag = head["importFlag"].toBool(false);
                info.hasIdFlag = head["hasIdFlag"].toBool(false);
                info.newgId = head["newgId"].toBool(false);

                if (head.contains("c_para")) {
                    QJsonObject c_para = head["c_para"].toObject();
                    info.name = c_para["package"].toString();

                    bool isSmd = cadData.contains("SMT") && cadData["SMT"].toBool();
                    info.type = isSmd ? "smd" : "tht";

                    if (c_para.contains("3DModel")) {
                        info.model3DName = c_para["3DModel"].toString();
                    }

                    // 附加参数
                    info.link = c_para["link"].toString();
                    info.contributor = c_para["Contributor"].toString();
                    info.uuid3d = head["uuid_3d"].toString();
                }

                // 保存画布信息
                if (dataStr.contains("canvas")) {
                    info.canvas = dataStr["canvas"].toString();
                }

                if (dataStr.contains("layers")) {
                    QJsonArray layersArray = dataStr["layers"].toArray();
                    QStringList layers;
                    for (const QJsonValue& layerValue : layersArray) {
                        layers.append(layerValue.toString());
                    }
                    info.layers = layers.join("\n");
                }

                if (dataStr.contains("objects")) {
                    QJsonArray objectsArray = dataStr["objects"].toArray();
                    QStringList objects;
                    for (const QJsonValue& objectValue : objectsArray) {
                        objects.append(objectValue.toString());
                    }
                    info.objects = objects.join("\n");
                }
            }

            // 导入边界框（包围盒）
            if (dataStr.contains("head")) {
                QJsonObject head = dataStr["head"].toObject();
                FootprintBBox footprintBbox;
                if (dataStr.contains("BBox")) {
                    QJsonObject bbox = dataStr["BBox"].toObject();
                    footprintBbox.x = bbox["x"].toDouble();
                    footprintBbox.y = bbox["y"].toDouble();
                    footprintBbox.width = bbox["width"].toDouble();
                    footprintBbox.height = bbox["height"].toDouble();
                } else {
                    footprintBbox.x = head["x"].toDouble();
                    footprintBbox.y = head["y"].toDouble();
                    footprintBbox.width = 0;
                    footprintBbox.height = 0;
                }
                footprintData->setBbox(footprintBbox);
            }

            // 导入几何数据（焊盘、走线、孔等）
            if (dataStr.contains("shape")) {
                QJsonArray shapes = dataStr["shape"].toArray();

                for (const QJsonValue& shapeValue : shapes) {
                    QString shapeString = shapeValue.toString();
                    QStringList parts = shapeString.split("~");

                    if (parts.isEmpty())
                        continue;

                    QString designator = parts[0];

                    if (designator == "PAD") {
                        FootprintPad pad = importPadData(shapeString);
                        footprintData->addPad(pad);
                    } else if (designator == "TRACK") {
                        FootprintTrack track = importTrackData(shapeString);
                        footprintData->addTrack(track);
                    } else if (designator == "HOLE") {
                        FootprintHole hole = importHoleData(shapeString);
                        footprintData->addHole(hole);
                    } else if (designator == "CIRCLE") {
                        FootprintCircle circle = importFootprintCircleData(shapeString);
                        footprintData->addCircle(circle);
                    } else if (designator == "ARC") {
                        FootprintArc arc = importFootprintArcData(shapeString);
                        footprintData->addArc(arc);
                    } else if (designator == "RECT") {
                        FootprintRectangle rectangle = importFootprintRectangleData(shapeString);
                        footprintData->addRectangle(rectangle);
                    } else if (designator == "TEXT") {
                        FootprintText text = importFootprintTextData(shapeString);
                        footprintData->addText(text);
                    } else if (designator == "POLYLINE" || designator == "PL") {
                        FootprintTrack track = importTrackData(shapeString);
                        footprintData->addTrack(track);
                    } else if (designator == "POLYGON" || designator == "PG") {
                        FootprintTrack track = importTrackData(shapeString);
                        footprintData->addTrack(track);
                    } else if (designator == "PATH" || designator == "PT") {
                        FootprintTrack track = importTrackData(shapeString);
                        footprintData->addTrack(track);
                    } else if (designator == "SVGNODE") {
                        importSvgNodeData(shapeString, footprintData);
                    } else if (designator == "SOLIDREGION") {
                        FootprintSolidRegion solidRegion = importSolidRegionData(shapeString);
                        footprintData->addSolidRegion(solidRegion);
                    }
                }

                if (dataStr.contains("layers")) {
                    QJsonArray layers = dataStr["layers"].toArray();
                    for (const QJsonValue& layerValue : layers) {
                        LayerDefinition layer = parseLayerDefinition(layerValue.toString());
                        footprintData->addLayer(layer);
                    }
                }

                if (dataStr.contains("objects")) {
                    QJsonArray objects = dataStr["objects"].toArray();
                    for (const QJsonValue& objectValue : objects) {
                        ObjectVisibility visibility = parseObjectVisibility(objectValue.toString());
                        footprintData->addObjectVisibility(visibility);
                    }
                }

                bool hasHole = false;
                for (const FootprintPad& pad : footprintData->pads()) {
                    if (pad.holeRadius > 0 || pad.holeLength > 0) {
                        hasHole = true;
                        break;
                    }
                }
                if (hasHole) {
                    info.type = "tht";
                }
            }

            footprintData->setInfo(info);
        }
    }

    return footprintData;
}

SymbolPin EasyedaImporter::importPinData(const QString& pinData) {
    SymbolPin pin;
    QList<QStringList> segments = parsePinDataString(pinData);

    if (segments.size() >= 7) {
        QStringList settings = segments[0];
        if (settings.size() >= 8) {
            pin.settings.isDisplayed = (settings[1] == "show");
            pin.settings.type = static_cast<PinType>(settings[2].toInt());
            pin.settings.spicePinNumber = settings[3];
            pin.settings.posX = settings[4].toDouble();
            pin.settings.posY = settings[5].toDouble();
            pin.settings.rotation = settings[6].toInt();
            pin.settings.id = settings[7];
            pin.settings.isLocked = (settings.size() > 8 ? stringToBool(settings[8]) : false);
        }

        if (segments[1].size() >= 2) {
            pin.pinDot.dotX = segments[1][0].toDouble();
            pin.pinDot.dotY = segments[1][1].toDouble();
        }

        if (segments[2].size() >= 2) {
            pin.pinPath.path = segments[2][0];
            pin.pinPath.color = segments[2][1];
        }

        if (segments[3].size() >= 8) {
            pin.name.isDisplayed = (segments[3][0] == "show");
            pin.name.posX = segments[3][1].toDouble();
            pin.name.posY = segments[3][2].toDouble();
            pin.name.rotation = segments[3][3].toInt();
            pin.name.text = segments[3][4];
            pin.name.textAnchor = segments[3][5];
            pin.name.font = segments[3][6];
            pin.name.fontSize = segments[3][7].toDouble();
        }

        if (segments[4].size() >= 5) {
            qDebug() << "Pin Segment 4 data:" << segments[4];
            qDebug() << "  Segment 4[0] (dotShow):" << segments[4][0];

            pin.dot.isDisplayed = false;

            pin.dot.circleX = segments[4][1].toDouble();
            pin.dot.circleY = segments[4][2].toDouble();

            qDebug() << "  dot.isDisplayed set to: false (always hide)";

            QString pinNumberDisplayText = segments[4][4];
            if (!pinNumberDisplayText.isEmpty()) {
                pin.settings.spicePinNumber = pinNumberDisplayText;
                qDebug() << "Pin Number Display Text extracted from Segment 4:" << pinNumberDisplayText << "for pin"
                         << pin.name.text << "(replaced spicePinNumber)";
            }
        }

        if (segments[6].size() >= 2) {
            pin.clock.isDisplayed = (segments[6][0] == "show");
            pin.clock.path = segments[6][1];
        }
    }

    return pin;
}

SymbolRectangle EasyedaImporter::importRectangleData(const QString& rectangleData) {
    SymbolRectangle rectangle;
    QStringList fields = parseDataString(rectangleData);

    if (fields.size() >= 13) {
        rectangle.posX = fields[1].toDouble();
        rectangle.posY = fields[2].toDouble();
        rectangle.rx = fields[3].isEmpty() ? 0.0 : fields[3].toDouble();
        rectangle.ry = fields[4].isEmpty() ? 0.0 : fields[4].toDouble();
        rectangle.width = fields[5].toDouble();
        rectangle.height = fields[6].toDouble();
        rectangle.strokeColor = fields[7];
        rectangle.strokeWidth = fields[8].toDouble();
        rectangle.strokeStyle = fields[9];
        rectangle.fillColor = fields[10];
        rectangle.id = fields[11];
        rectangle.isLocked = stringToBool(fields[12]);
    }

    return rectangle;
}

SymbolCircle EasyedaImporter::importCircleData(const QString& circleData) {
    SymbolCircle circle;
    QStringList fields = parseDataString(circleData);

    if (fields.size() >= 10) {
        circle.centerX = fields[0].toDouble();
        circle.centerY = fields[1].toDouble();
        circle.radius = fields[2].toDouble();
        circle.strokeColor = fields[3];
        circle.strokeWidth = fields[4].toDouble();
        circle.strokeStyle = fields[5];
        circle.fillColor = stringToBool(fields[6]);
        circle.id = fields[7];
        circle.isLocked = stringToBool(fields[8]);
    }

    return circle;
}

SymbolArc EasyedaImporter::importArcData(const QString& arcData) {
    SymbolArc arc;
    QStringList fields = parseDataString(arcData);

    if (fields.size() >= 8) {
        arc.helperDots = fields[1];
        arc.strokeColor = fields[2];
        arc.strokeWidth = fields[3].toDouble();
        arc.strokeStyle = fields[4];
        arc.fillColor = stringToBool(fields[5]);
        arc.id = fields[6];
        arc.isLocked = stringToBool(fields[7]);
    }

    return arc;
}

SymbolEllipse EasyedaImporter::importEllipseData(const QString& ellipseData) {
    SymbolEllipse ellipse;
    QStringList fields = parseDataString(ellipseData);

    if (fields.size() >= 10) {
        ellipse.centerX = fields[1].toDouble();
        ellipse.centerY = fields[2].toDouble();
        ellipse.radiusX = fields[3].toDouble();
        ellipse.radiusY = fields[4].toDouble();
        ellipse.strokeColor = fields[5];
        ellipse.strokeWidth = fields[6].toDouble();
        ellipse.strokeStyle = fields[7];
        ellipse.fillColor = stringToBool(fields[8]);
        ellipse.id = fields[9];
        ellipse.isLocked = fields.size() > 10 ? stringToBool(fields[10]) : false;
    }

    return ellipse;
}

SymbolPolyline EasyedaImporter::importPolylineData(const QString& polylineData) {
    SymbolPolyline polyline;
    QStringList fields = parseDataString(polylineData);

    if (fields.size() >= 8) {
        polyline.points = fields[1];
        polyline.strokeColor = fields[2];
        polyline.strokeWidth = fields[3].toDouble();
        polyline.strokeStyle = fields[4];
        polyline.fillColor = stringToBool(fields[5]);
        polyline.id = fields[6];
        polyline.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;
    }

    return polyline;
}

SymbolPolygon EasyedaImporter::importPolygonData(const QString& polygonData) {
    SymbolPolygon polygon;
    QStringList fields = parseDataString(polygonData);

    if (fields.size() >= 8) {
        polygon.points = fields[1];
        polygon.strokeColor = fields[2];
        polygon.strokeWidth = fields[3].toDouble();
        polygon.strokeStyle = fields[4];
        polygon.fillColor = stringToBool(fields[5]);
        polygon.id = fields[6];
        polygon.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;
    }

    return polygon;
}

SymbolPath EasyedaImporter::importPathData(const QString& pathData) {
    SymbolPath path;
    QStringList fields = parseDataString(pathData);

    if (fields.size() >= 8) {
        path.paths = fields[1];
        path.strokeColor = fields[2];
        path.strokeWidth = fields[3].toDouble();
        path.strokeStyle = fields[4];
        path.fillColor = stringToBool(fields[5]);
        path.id = fields[6];
        path.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;
    }

    return path;
}

SymbolText EasyedaImporter::importTextData(const QString& textData) {
    SymbolText text;
    QStringList fields = parseDataString(textData);

    if (fields.size() >= 15) {
        text.mark = fields[0];
        text.posX = fields[1].toDouble();
        text.posY = fields[2].toDouble();
        text.rotation = fields[3].toInt();
        text.color = fields[4];
        text.font = fields[5];
        text.textSize = fields[6].toDouble();
        text.bold = stringToBool(fields[7]);
        text.italic = fields[8];
        text.baseline = fields[9];
        text.type = fields[10];
        text.text = fields[11];
        text.visible = stringToBool(fields[12]);
        text.anchor = fields[13];
        text.id = fields[14];
        text.isLocked = fields.size() > 15 ? stringToBool(fields[15]) : false;
    }

    return text;
}

FootprintPad EasyedaImporter::importPadData(const QString& padData) {
    FootprintPad pad;
    QStringList fields = parseDataString(padData);

    if (fields.size() >= 17) {
        pad.shape = fields[1];
        pad.centerX = fields[2].toDouble();
        pad.centerY = fields[3].toDouble();
        pad.width = fields[4].toDouble();
        pad.height = fields[5].toDouble();
        pad.layerId = fields[6].toInt();
        pad.net = fields[7];
        pad.number = fields[8];
        pad.holeRadius = fields[9].toDouble();
        pad.points = fields[10];
        pad.rotation = fields[11].toDouble();
        pad.id = fields[12];
        pad.holeLength = fields[13].toDouble();
        pad.holePoint = fields[14];
        pad.isPlated = stringToBool(fields[15]);
        pad.isLocked = stringToBool(fields[16]);
    }

    return pad;
}

FootprintTrack EasyedaImporter::importTrackData(const QString& trackData) {
    FootprintTrack track;
    QStringList fields = parseDataString(trackData);

    if (fields.size() >= 6) {
        track.strokeWidth = fields[1].toDouble();
        track.layerId = fields[2].toInt();
        track.net = fields[3];
        track.points = fields[4];
        track.id = fields[5];
        track.isLocked = fields.size() > 6 ? stringToBool(fields[6]) : false;
    }

    return track;
}

FootprintHole EasyedaImporter::importHoleData(const QString& holeData) {
    FootprintHole hole;
    QStringList fields = parseDataString(holeData);

    if (fields.size() >= 5) {
        hole.centerX = fields[1].toDouble();
        hole.centerY = fields[2].toDouble();
        hole.radius = fields[3].toDouble();
        hole.id = fields[4];
        hole.isLocked = fields.size() > 5 ? stringToBool(fields[5]) : false;
    }

    return hole;
}

FootprintCircle EasyedaImporter::importFootprintCircleData(const QString& circleData) {
    FootprintCircle circle;
    QStringList fields = parseDataString(circleData);

    if (fields.size() >= 7) {
        circle.cx = fields[1].toDouble();
        circle.cy = fields[2].toDouble();
        circle.radius = fields[3].toDouble();
        circle.strokeWidth = fields[4].toDouble();
        circle.layerId = fields[5].toInt();
        circle.id = fields[6];
        circle.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;
    }

    return circle;
}

FootprintRectangle EasyedaImporter::importFootprintRectangleData(const QString& rectangleData) {
    FootprintRectangle rectangle;
    QStringList fields = parseDataString(rectangleData);

    if (fields.size() >= 8) {
        rectangle.x = fields[1].toDouble();
        rectangle.y = fields[2].toDouble();
        rectangle.width = fields[3].toDouble();
        rectangle.height = fields[4].toDouble();
        rectangle.layerId = fields[5].toInt();
        rectangle.id = fields[6];
        rectangle.strokeWidth = fields[7].toDouble();
        rectangle.isLocked = fields.size() > 8 ? stringToBool(fields[8]) : false;
    }

    return rectangle;
}

FootprintArc EasyedaImporter::importFootprintArcData(const QString& arcData) {
    FootprintArc arc;
    QStringList fields = parseDataString(arcData);

    if (fields.size() >= 7) {
        arc.strokeWidth = fields[1].toDouble();
        arc.layerId = fields[2].toInt();
        arc.net = fields[3];
        arc.path = fields[4];
        arc.helperDots = fields[5];
        arc.id = fields[6];
        arc.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;
    }

    return arc;
}

FootprintText EasyedaImporter::importFootprintTextData(const QString& textData) {
    FootprintText text;
    QStringList fields = parseDataString(textData);

    if (fields.size() >= 14) {
        text.type = fields[1];
        text.centerX = fields[2].toDouble();
        text.centerY = fields[3].toDouble();
        text.strokeWidth = fields[4].toDouble();
        text.rotation = fields[5].toInt();
        text.mirror = fields[6];
        text.layerId = fields[7].toInt();
        text.net = fields[8];
        text.fontSize = fields[9].toDouble();
        text.text = fields[10];
        text.textPath = fields[11];
        text.isDisplayed = stringToBool(fields[12]);
        text.id = fields[13];
        text.isLocked = fields.size() > 14 ? stringToBool(fields[14]) : false;
    }

    return text;
}

QStringList EasyedaImporter::parseDataString(const QString& data) const {
    return data.split("~", Qt::KeepEmptyParts);
}

QList<QStringList> EasyedaImporter::parsePinDataString(const QString& pinData) const {
    QList<QStringList> result;

    QStringList segments;
    int start = 0;
    int pos = 0;

    while (pos < pinData.length()) {
        if (pinData[pos] == '^' && pos + 1 < pinData.length() && pinData[pos + 1] == '^') {
            segments.append(pinData.mid(start, pos - start));
            pos += 2;
            start = pos;
            continue;
        }

        pos++;
    }

    if (start < pinData.length()) {
        segments.append(pinData.mid(start));
    }

    for (const QString& segment : segments) {
        QStringList subSegments = segment.split("~", Qt::KeepEmptyParts);
        result.append(subSegments);
    }

    return result;
}

FootprintSolidRegion EasyedaImporter::importSolidRegionData(const QString& solidRegionData) {
    FootprintSolidRegion region;
    QStringList fields = parseDataString(solidRegionData);

    if (fields.size() >= 6) {
        region.layerId = fields[1].toInt();
        region.path = fields[3];
        region.fillStyle = fields[4];
        region.id = fields[5];
        region.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;
        region.isKeepOut = (region.layerId == 99);
    }

    return region;
}

void EasyedaImporter::importSvgNodeData(const QString& svgNodeData, QSharedPointer<FootprintData> footprintData) {
    if (!footprintData) {
        qWarning() << "importSvgNodeData called with null footprintData";
        return;
    }

    QStringList parts = svgNodeData.split("~");
    if (parts.size() < 2) {
        qWarning() << "Invalid SVGNODE data format";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(parts[1].toUtf8());
    if (!doc.isObject()) {
        qWarning() << "Failed to parse SVGNODE JSON";
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("attrs")) {
        qWarning() << "SVGNODE missing attrs";
        return;
    }

    QJsonObject attrs = root["attrs"].toObject();

    if (attrs.contains("c_etype") && attrs["c_etype"].toString() == "outline3D") {
        Model3DData model3D;
        if (attrs.contains("uuid")) {
            QString modelUuid = attrs["uuid"].toString();
            model3D.setUuid(modelUuid);
            model3D.setName(attrs.contains("title") ? attrs["title"].toString() : "");

            if (attrs.contains("c_origin")) {
                QString c_origin = attrs["c_origin"].toString();
                QStringList originParts = c_origin.split(",");
                if (originParts.size() >= 2) {
                    Model3DBase translation;
                    double originX = originParts[0].toDouble();
                    double originY = originParts[1].toDouble();

                    if (qAbs(originX) < 1000 && qAbs(originY) < 1000) {
                        double bboxCenterX = footprintData->bbox().x + footprintData->bbox().width / 2.0;
                        double bboxCenterY = footprintData->bbox().y + footprintData->bbox().height / 2.0;

                        translation.x = bboxCenterX;
                        translation.y = bboxCenterY;
                    } else {
                        translation.x = originX;
                        translation.y = originY;
                    }

                    translation.z = attrs.contains("z") ? attrs["z"].toDouble() : 0.0;
                    model3D.setTranslation(translation);
                }
            }

            if (attrs.contains("c_rotation")) {
                QString c_rotation = attrs["c_rotation"].toString();
                QStringList rotationParts = c_rotation.split(",");
                if (rotationParts.size() >= 3) {
                    Model3DBase rotation;
                    rotation.x = rotationParts[0].toDouble();
                    rotation.y = rotationParts[1].toDouble();
                    rotation.z = rotationParts[2].toDouble();
                    model3D.setRotation(rotation);
                }
            }

            footprintData->setModel3D(model3D);
        }

        FootprintOutline outline;
        outline.id = attrs["id"].toString();
        outline.strokeWidth = attrs.contains("c_width") ? attrs["c_width"].toString().toDouble() : 0.0;
        outline.isLocked = false;

        if (attrs.contains("layerid")) {
            outline.layerId = attrs["layerid"].toString().toInt();
        } else {
            outline.layerId = 19;  // 默认�?3DModel �?
        }

        if (root.contains("childNodes")) {
            QJsonArray childNodes = root["childNodes"].toArray();
            for (const QJsonValue& child : childNodes) {
                QJsonObject childObj = child.toObject();
                if (childObj.contains("attrs")) {
                    QJsonObject childAttrs = childObj["attrs"].toObject();
                    if (childAttrs.contains("points")) {
                        outline.path = childAttrs["points"].toString();
                        break;
                    }
                }
            }
        }

        footprintData->addOutline(outline);
    } else {
        Model3DData model3D;
        model3D.setName(attrs.contains("title") ? attrs["title"].toString() : "");
        model3D.setUuid(attrs.contains("uuid") ? attrs["uuid"].toString() : "");

        if (attrs.contains("c_origin")) {
            QString c_origin = attrs["c_origin"].toString();
            QStringList originParts = c_origin.split(",");
            if (originParts.size() >= 2) {
                Model3DBase translation;
                translation.x = originParts[0].toDouble();
                translation.y = originParts[1].toDouble();
                translation.z = attrs.contains("z") ? attrs["z"].toDouble() : 0.0;
                model3D.setTranslation(translation);
            }
        }

        if (attrs.contains("c_rotation")) {
            QString c_rotation = attrs["c_rotation"].toString();
            QStringList rotationParts = c_rotation.split(",");
            if (rotationParts.size() >= 3) {
                Model3DBase rotation;
                rotation.x = rotationParts[0].toDouble();
                rotation.y = rotationParts[1].toDouble();
                rotation.z = rotationParts[2].toDouble();
                model3D.setRotation(rotation);
            }
        }

        footprintData->setModel3D(model3D);
    }
}

LayerDefinition EasyedaImporter::parseLayerDefinition(const QString& layerString) {
    LayerDefinition layer;
    QStringList fields = layerString.split("~");

    if (fields.size() >= 5) {
        layer.layerId = fields[0].toInt();
        layer.name = fields[1];
        layer.color = fields[2];
        layer.isVisible = (fields[3] == "true");
        layer.isUsedForManufacturing = (fields[4] == "true");

        if (fields.size() >= 7) {
            layer.expansion = fields[6].toDouble();
        }
    }

    return layer;
}

ObjectVisibility EasyedaImporter::parseObjectVisibility(const QString& objectString) {
    ObjectVisibility visibility;
    QStringList fields = objectString.split("~");

    if (fields.size() >= 3) {
        visibility.objectType = fields[0];
        visibility.isEnabled = (fields[1] == "true");
        visibility.isVisible = (fields[2] == "true");
    }

    return visibility;
}

bool EasyedaImporter::stringToBool(const QString& str) const {
    if (str.isEmpty() || str == "0" || str.toLower() == "false" || str.toLower() == "none" ||
        str.toLower() == "transparent") {
        return false;
    }
    return true;
}
}  // namespace EasyKiConverter
