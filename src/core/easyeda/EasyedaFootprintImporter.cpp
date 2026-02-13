#include "EasyedaFootprintImporter.h"

#include "EasyedaUtils.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>


namespace EasyKiConverter {

EasyedaFootprintImporter::EasyedaFootprintImporter() {}

QSharedPointer<FootprintData> EasyedaFootprintImporter::importFootprintData(const QJsonObject& cadData) {
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

FootprintPad EasyedaFootprintImporter::importPadData(const QString& padData) {
    FootprintPad pad;
    QStringList fields = EasyedaUtils::parseDataString(padData);

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
        pad.isPlated = EasyedaUtils::stringToBool(fields[15]);
        pad.isLocked = EasyedaUtils::stringToBool(fields[16]);
    }

    return pad;
}

FootprintTrack EasyedaFootprintImporter::importTrackData(const QString& trackData) {
    FootprintTrack track;
    QStringList fields = EasyedaUtils::parseDataString(trackData);

    if (fields.size() >= 6) {
        track.strokeWidth = fields[1].toDouble();
        track.layerId = fields[2].toInt();
        track.net = fields[3];
        track.points = fields[4];
        track.id = fields[5];
        track.isLocked = fields.size() > 6 ? EasyedaUtils::stringToBool(fields[6]) : false;
    }

    return track;
}

FootprintHole EasyedaFootprintImporter::importHoleData(const QString& holeData) {
    FootprintHole hole;
    QStringList fields = EasyedaUtils::parseDataString(holeData);

    if (fields.size() >= 5) {
        hole.centerX = fields[1].toDouble();
        hole.centerY = fields[2].toDouble();
        hole.radius = fields[3].toDouble();
        hole.id = fields[4];
        hole.isLocked = fields.size() > 5 ? EasyedaUtils::stringToBool(fields[5]) : false;
    }

    return hole;
}

FootprintCircle EasyedaFootprintImporter::importFootprintCircleData(const QString& circleData) {
    FootprintCircle circle;
    QStringList fields = EasyedaUtils::parseDataString(circleData);

    if (fields.size() >= 7) {
        circle.cx = fields[1].toDouble();
        circle.cy = fields[2].toDouble();
        circle.radius = fields[3].toDouble();
        circle.strokeWidth = fields[4].toDouble();
        circle.layerId = fields[5].toInt();
        circle.id = fields[6];
        circle.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return circle;
}

FootprintRectangle EasyedaFootprintImporter::importFootprintRectangleData(const QString& rectangleData) {
    FootprintRectangle rectangle;
    QStringList fields = EasyedaUtils::parseDataString(rectangleData);

    if (fields.size() >= 8) {
        rectangle.x = fields[1].toDouble();
        rectangle.y = fields[2].toDouble();
        rectangle.width = fields[3].toDouble();
        rectangle.height = fields[4].toDouble();
        rectangle.layerId = fields[5].toInt();
        rectangle.id = fields[6];
        rectangle.strokeWidth = fields[7].toDouble();
        rectangle.isLocked = fields.size() > 8 ? EasyedaUtils::stringToBool(fields[8]) : false;
    }

    return rectangle;
}

FootprintArc EasyedaFootprintImporter::importFootprintArcData(const QString& arcData) {
    FootprintArc arc;
    QStringList fields = EasyedaUtils::parseDataString(arcData);

    if (fields.size() >= 7) {
        arc.strokeWidth = fields[1].toDouble();
        arc.layerId = fields[2].toInt();
        arc.net = fields[3];
        arc.path = fields[4];
        arc.helperDots = fields[5];
        arc.id = fields[6];
        arc.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
    }

    return arc;
}

FootprintText EasyedaFootprintImporter::importFootprintTextData(const QString& textData) {
    FootprintText text;
    QStringList fields = EasyedaUtils::parseDataString(textData);

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
        text.isDisplayed = EasyedaUtils::stringToBool(fields[12]);
        text.id = fields[13];
        text.isLocked = fields.size() > 14 ? EasyedaUtils::stringToBool(fields[14]) : false;
    }

    return text;
}

FootprintSolidRegion EasyedaFootprintImporter::importSolidRegionData(const QString& solidRegionData) {
    FootprintSolidRegion region;
    QStringList fields = EasyedaUtils::parseDataString(solidRegionData);

    if (fields.size() >= 6) {
        region.layerId = fields[1].toInt();
        region.path = fields[3];
        region.fillStyle = fields[4];
        region.id = fields[5];
        region.isLocked = fields.size() > 7 ? EasyedaUtils::stringToBool(fields[7]) : false;
        region.isKeepOut = (region.layerId == 99);
    }

    return region;
}

void EasyedaFootprintImporter::importSvgNodeData(const QString& svgNodeData,
                                                 QSharedPointer<FootprintData> footprintData) {
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
            outline.layerId = 19;  // 默认3DModel 层
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

LayerDefinition EasyedaFootprintImporter::parseLayerDefinition(const QString& layerString) {
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

ObjectVisibility EasyedaFootprintImporter::parseObjectVisibility(const QString& objectString) {
    ObjectVisibility visibility;
    QStringList fields = objectString.split("~");

    if (fields.size() >= 3) {
        visibility.objectType = fields[0];
        visibility.isEnabled = (fields[1] == "true");
        visibility.isVisible = (fields[2] == "true");
    }

    return visibility;
}

}  // namespace EasyKiConverter
