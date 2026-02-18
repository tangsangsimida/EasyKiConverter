#include "EasyedaSymbolImporter.h"

#include "EasyedaUtils.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>

namespace EasyKiConverter {

EasyedaSymbolImporter::EasyedaSymbolImporter() {}

QSharedPointer<SymbolData> EasyedaSymbolImporter::importSymbolData(const QJsonObject& cadData) {
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

SymbolPin EasyedaSymbolImporter::importPinData(const QString& pinData) {
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
            pin.settings.isLocked = (settings.size() > 8 ? EasyedaUtils::stringToBool(settings[8]) : false);
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

SymbolRectangle EasyedaSymbolImporter::importRectangleData(const QString& rectangleData) {
    SymbolRectangle rectangle;
    QStringList fields = EasyedaUtils::parseDataString(rectangleData);

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
        rectangle.isLocked = EasyedaUtils::stringToBool(fields[12]);
    }

    return rectangle;
}

SymbolCircle EasyedaSymbolImporter::importCircleData(const QString& circleData) {
    SymbolCircle circle;
    QStringList fields = EasyedaUtils::parseDataString(circleData);

    if (fields.size() >= 10) {
        circle.centerX = fields[0].toDouble();
        circle.centerY = fields[1].toDouble();
        circle.radius = fields[2].toDouble();
        circle.strokeColor = fields[3];
        circle.strokeWidth = fields[4].toDouble();
        circle.strokeStyle = fields[5];
        circle.fillColor = EasyedaUtils::stringToBool(fields[6]);
        circle.id = fields[7];
        circle.isLocked = EasyedaUtils::stringToBool(fields[8]);
    }

    return circle;
}

SymbolArc EasyedaSymbolImporter::importArcData(const QString& arcData) {
    SymbolArc arc;
    QStringList fields = EasyedaUtils::parseDataString(arcData);

    if (fields.size() >= 8) {
        arc.helperDots = fields[1];
        arc.strokeColor = fields[2];
        arc.strokeWidth = fields[3].toDouble();
        arc.strokeStyle = fields[4];
        arc.fillColor = EasyedaUtils::stringToBool(fields[5]);
        arc.id = fields[6];
        arc.isLocked = EasyedaUtils::stringToBool(fields[7]);
    }

    return arc;
}

SymbolEllipse EasyedaSymbolImporter::importEllipseData(const QString& ellipseData) {
    SymbolEllipse ellipse;
    QStringList fields = EasyedaUtils::parseDataString(ellipseData);

    if (fields.size() >= 10) {
        ellipse.centerX = fields[1].toDouble();
        ellipse.centerY = fields[2].toDouble();
        ellipse.radiusX = fields[3].toDouble();
        ellipse.radiusY = fields[4].toDouble();
        ellipse.strokeColor = fields[5];
        ellipse.strokeWidth = fields[6].toDouble();
        ellipse.strokeStyle = fields[7];
        ellipse.fillColor = EasyedaUtils::stringToBool(fields[8]);
        ellipse.id = fields[9];
        ellipse.isLocked = fields.size() > 10 ? EasyedaUtils::stringToBool(fields[10]) : false;
    }

    return ellipse;
}

SymbolPolyline EasyedaSymbolImporter::importPolylineData(const QString& polylineData) {
    SymbolPolyline polyline;
    QStringList fields = EasyedaUtils::parseDataString(polylineData);

    if (fields.size() >= 8) {
        polyline.points = fields[1];
        polyline.strokeColor = fields[2];
        polyline.strokeWidth = fields[3].toDouble();
        polyline.strokeStyle = fields[4];
        polyline.fillColor = EasyedaUtils::stringToBool(fields[5]);
        polyline.id = fields[6];
        polyline.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return polyline;
}

SymbolPolygon EasyedaSymbolImporter::importPolygonData(const QString& polygonData) {
    SymbolPolygon polygon;
    QStringList fields = EasyedaUtils::parseDataString(polygonData);

    if (fields.size() >= 8) {
        polygon.points = fields[1];
        polygon.strokeColor = fields[2];
        polygon.strokeWidth = fields[3].toDouble();
        polygon.strokeStyle = fields[4];
        polygon.fillColor = EasyedaUtils::stringToBool(fields[5]);
        polygon.id = fields[6];
        polygon.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return polygon;
}

SymbolPath EasyedaSymbolImporter::importPathData(const QString& pathData) {
    SymbolPath path;
    QStringList fields = EasyedaUtils::parseDataString(pathData);

    if (fields.size() >= 8) {
        path.paths = fields[1];
        path.strokeColor = fields[2];
        path.strokeWidth = fields[3].toDouble();
        path.strokeStyle = fields[4];
        path.fillColor = EasyedaUtils::stringToBool(fields[5]);
        path.id = fields[6];
        path.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return path;
}

SymbolText EasyedaSymbolImporter::importTextData(const QString& textData) {
    SymbolText text;
    QStringList fields = EasyedaUtils::parseDataString(textData);

    if (fields.size() >= 15) {
        text.mark = fields[0];
        text.posX = fields[1].toDouble();
        text.posY = fields[2].toDouble();
        text.rotation = fields[3].toInt();
        text.color = fields[4];
        text.font = fields[5];
        text.textSize = fields[6].toDouble();
        text.bold = EasyedaUtils::stringToBool(fields[7]);
        text.italic = fields[8];
        text.baseline = fields[9];
        text.type = fields[10];
        text.text = fields[11];
        text.visible = EasyedaUtils::stringToBool(fields[12]);
        text.anchor = fields[13];
        text.id = fields[14];
        text.isLocked = fields.size() > 15 ? EasyedaUtils::stringToBool(fields[15]) : false;
    }

    return text;
}

QList<QStringList> EasyedaSymbolImporter::parsePinDataString(const QString& pinData) const {
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

}  // namespace EasyKiConverter
