#include "EasyedaImporter.h"
#include <qDebug>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>

namespace EasyKiConverter
{

    EasyedaImporter::EasyedaImporter(QObject *parent)
        : QObject(parent)
    {
    }

    EasyedaImporter::~EasyedaImporter()
    {
    }

    QSharedPointer<SymbolData> EasyedaImporter::importSymbolData(const QJsonObject &cadData)
    {

        auto symbolData = QSharedPointer<SymbolData>::create();

        // 导入顶层字段（result 对象的根字段�?

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
        if (cadData.contains("dataStr"))
        {
            QJsonObject dataStr = cadData["dataStr"].toObject();
            if (dataStr.contains("head"))
            {
                QJsonObject head = dataStr["head"].toObject();

                // 编辑器信�?
                info.editorVersion = head["editorVersion"].toString();

                // 项目信息
                info.puuid = head["puuid"].toString();
                info.utime = head["utime"].toVariant().toLongLong();
                info.importFlag = head["importFlag"].toBool(false);
                info.hasIdFlag = head["hasIdFlag"].toBool(false);

                if (head.contains("c_para"))
                {
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

            // �?lcsc 字段获取数据手册 URL
            if (cadData.contains("lcsc"))
            {
                QJsonObject lcsc = cadData["lcsc"].toObject();
                info.datasheet = lcsc["url"].toString();
            }
        }

        symbolData->setInfo(info);

        // 导入边界�?
        if (cadData.contains("dataStr"))
        {
            QJsonObject dataStr = cadData["dataStr"].toObject();
            SymbolBBox symbolBbox;
            if (dataStr.contains("BBox"))
            {
                // �?dataStr.BBox 中读取边界框数据
                QJsonObject bbox = dataStr["BBox"].toObject();
                symbolBbox.x = bbox["x"].toDouble();
                symbolBbox.y = bbox["y"].toDouble();
                symbolBbox.width = bbox["width"].toDouble();
                symbolBbox.height = bbox["height"].toDouble();
                // qDebug() << "Symbol BBox from dataStr.BBox - x:" << symbolBbox.x << "y:" << symbolBbox.y
                // << "width:" << symbolBbox.width << "height:" << symbolBbox.height;
            }
            else if (dataStr.contains("head"))
            {
                // 如果没有 BBox，使�?head.x �?head.y 作为中心点，width �?height 设为 0
                QJsonObject head = dataStr["head"].toObject();
                symbolBbox.x = head["x"].toDouble();
                symbolBbox.y = head["y"].toDouble();
                symbolBbox.width = 0;
                symbolBbox.height = 0;
                qWarning() << "Symbol BBox not found in dataStr, using head.x/y as center point";
            }
            symbolData->setBbox(symbolBbox);
        }

        // 导入几何数据（引脚、矩形、圆、圆弧、多边形、路径、文本等�?
        qDebug() << "=== EasyedaImporter::importSymbolData - Starting geometry data import ===";
        if (cadData.contains("dataStr"))
        {
            QJsonObject dataStr = cadData["dataStr"].toObject();
            qDebug() << "dataStr found, keys:" << dataStr.keys();

            qDebug() << "Checking for subparts field in cadData...";
            qDebug() << "cadData contains subparts:" << cadData.contains("subparts");

            // 检查是否有子部分（多部分符号）
            if (cadData.contains("subparts") && cadData["subparts"].isArray())
            {
                QJsonArray subparts = cadData["subparts"].toArray();
                qDebug() << "Found subparts field in cadData, size:" << subparts.size();

                // 如果 subparts 数组为空，则按单部分符号处理
                if (subparts.isEmpty())
                {
                    qDebug() << "Subparts array is empty, treating as single-part symbol";
                }
                else
                {
                    qDebug() << "Found" << subparts.size() << "subparts in symbol";

                    // 导入每个子部�?
                    for (int i = 0; i < subparts.size(); ++i)
                    {
                        QJsonObject subpart = subparts[i].toObject();
                        if (subpart.contains("dataStr"))
                        {
                            QJsonObject subpartDataStr = subpart["dataStr"].toObject();

                            SymbolPart part;
                            part.unitNumber = i;
                            qDebug() << "Importing subpart" << i << "from" << subpart["title"].toString();

                            // 导入子部分的图形元素
                            if (subpartDataStr.contains("shape"))
                            {
                                QJsonArray shapes = subpartDataStr["shape"].toArray();

                                for (const QJsonValue &shapeValue : shapes)
                                {
                                    QString shapeString = shapeValue.toString();
                                    QStringList parts = shapeString.split("~");

                                    if (parts.isEmpty())
                                        continue;

                                    QString designator = parts[0];

                                    if (designator == "P")
                                    {
                                        // 导入引脚
                                        SymbolPin pin = importPinData(shapeString);
                                        part.pins.append(pin);
                                    }
                                    else if (designator == "R")
                                    {
                                        // 导入矩形
                                        SymbolRectangle rectangle = importRectangleData(shapeString);
                                        part.rectangles.append(rectangle);
                                    }
                                    else if (designator == "C")
                                    {
                                        // 导入�?
                                        SymbolCircle circle = importCircleData(shapeString);
                                        part.circles.append(circle);
                                    }
                                    else if (designator == "A")
                                    {
                                        // 导入圆弧
                                        SymbolArc arc = importArcData(shapeString);
                                        part.arcs.append(arc);
                                    }
                                    else if (designator == "PL")
                                    {
                                        // 导入多段�?
                                        SymbolPolyline polyline = importPolylineData(shapeString);
                                        part.polylines.append(polyline);
                                    }
                                    else if (designator == "PG")
                                    {
                                        // 导入多边�?
                                        SymbolPolygon polygon = importPolygonData(shapeString);
                                        part.polygons.append(polygon);
                                    }
                                    else if (designator == "PT")
                                    {
                                        // 导入路径
                                        SymbolPath path = importPathData(shapeString);
                                        part.paths.append(path);
                                    }
                                    else if (designator == "T")
                                    {
                                        // 导入文本元素
                                        SymbolText text = importTextData(shapeString);
                                        part.texts.append(text);
                                    }
                                    else if (designator == "E")
                                    {
                                        // 导入椭圆
                                        SymbolEllipse ellipse = importEllipseData(shapeString);
                                        part.ellipses.append(ellipse);
                                    }
                                }
                            }

                            // 添加部分到符号数�?
                            symbolData->addPart(part);
                            qDebug() << "Imported subpart" << i << "with" << part.pins.size() << "pins," << part.rectangles.size() << "rectangles";
                        }
                    }

                    // 如果 subparts 不为空，则跳过单部分符号的处�?
                    if (!subparts.isEmpty())
                    {
                        qDebug() << "Multi-part symbol processed, skipping single-part symbol processing";
                        return symbolData;
                    }
                }
            }
            else
            {
                qDebug() << "No subparts field found in cadData, processing as single-part symbol";
            }

            // 单部分符号：导入主符号的图形元素
            qDebug() << "=== Starting single-part symbol processing ===";
            if (dataStr.contains("shape"))
            {
                QJsonArray shapes = dataStr["shape"].toArray();
                qDebug() << "=== Symbol Shape Parsing ===";
                qDebug() << "Total shapes in dataStr.shape:" << shapes.size();

                for (const QJsonValue &shapeValue : shapes)
                {
                    QString shapeString = shapeValue.toString();
                    QStringList parts = shapeString.split("~");

                    if (parts.isEmpty())
                        continue;

                    QString designator = parts[0];
                    qDebug() << "Processing shape with designator:" << designator;

                    if (designator == "P")
                    {
                        qDebug() << "  -> Processing pin data...";
                        // 导入引脚
                        SymbolPin pin = importPinData(shapeString);
                        qDebug() << "  -> Pin parsed, name:" << pin.name.text;
                        symbolData->addPin(pin);
                        qDebug() << "  -> Added pin:" << pin.name.text;
                    }
                    else if (designator == "R")
                    {
                        // 导入矩形
                        SymbolRectangle rectangle = importRectangleData(shapeString);
                        symbolData->addRectangle(rectangle);
                        qDebug() << "  -> Added rectangle";
                    }
                    else if (designator == "C")
                    {
                        // 导入�?
                        SymbolCircle circle = importCircleData(shapeString);
                        symbolData->addCircle(circle);
                        qDebug() << "  -> Added circle";
                    }
                    else if (designator == "A")
                    {
                        // 导入圆弧
                        SymbolArc arc = importArcData(shapeString);
                        symbolData->addArc(arc);
                        qDebug() << "  -> Added arc";
                    }
                    else if (designator == "PL")
                    {
                        // 导入多段�?
                        SymbolPolyline polyline = importPolylineData(shapeString);
                        symbolData->addPolyline(polyline);
                        qDebug() << "  -> Added polyline";
                    }
                    else if (designator == "PG")
                    {
                        // 导入多边�?
                        SymbolPolygon polygon = importPolygonData(shapeString);
                        symbolData->addPolygon(polygon);
                        qDebug() << "  -> Added polygon";
                    }
                    else if (designator == "PT")
                    {
                        // 导入路径
                        SymbolPath path = importPathData(shapeString);
                        symbolData->addPath(path);
                        qDebug() << "  -> Added path";
                    }
                    else if (designator == "T")
                    {
                        // 导入文本元素
                        SymbolText text = importTextData(shapeString);
                        symbolData->addText(text);
                        qDebug() << "  -> Added text:" << text.text;
                    }
                    else if (designator == "E")
                    {
                        // 导入椭圆
                        SymbolEllipse ellipse = importEllipseData(shapeString);
                        symbolData->addEllipse(ellipse);
                        qDebug() << "  -> Added ellipse";
                    }
                    else
                    {
                        qDebug() << "  -> Unknown designator:" << designator;
                    }
                }

                qDebug() << "Symbol shape parsing completed";
                qDebug() << "Final counts - Pins:" << symbolData->pins().size()
                         << "Rectangles:" << symbolData->rectangles().size()
                         << "Circles:" << symbolData->circles().size();
                qDebug() << "============================";
            }
            else
            {
                qDebug() << "WARNING: dataStr does not contain 'shape' field!";
                qDebug() << "Available keys in dataStr:" << dataStr.keys();
            }
        }

        return symbolData;
    }
    QSharedPointer<FootprintData> EasyedaImporter::importFootprintData(const QJsonObject &cadData)
    {
        auto footprintData = QSharedPointer<FootprintData>::create();

        // 导入封装信息（从 packageDetail 中获取）
        if (cadData.contains("packageDetail"))
        {
            QJsonObject packageDetail = cadData["packageDetail"].toObject();

            // packageDetail 顶层字段
            FootprintInfo info;
            info.uuid = packageDetail["uuid"].toString();
            info.docType = QString::number(packageDetail["docType"].toInt());
            info.datastrid = packageDetail["datastrid"].toString();
            info.writable = packageDetail["writable"].toBool(false);
            info.updateTime = packageDetail["updateTime"].toVariant().toLongLong();

            if (packageDetail.contains("dataStr"))
            {
                QJsonObject dataStr = packageDetail["dataStr"].toObject();

                // �?dataStr.head 中获取封装信�?
                if (dataStr.contains("head"))
                {
                    QJsonObject head = dataStr["head"].toObject();

                    // 编辑器信�?
                    info.editorVersion = head["editorVersion"].toString();

                    // 项目信息
                    info.puuid = head["puuid"].toString();
                    info.utime = head["utime"].toVariant().toLongLong();
                    info.importFlag = head["importFlag"].toBool(false);
                    info.hasIdFlag = head["hasIdFlag"].toBool(false);
                    info.newgId = head["newgId"].toBool(false);

                    if (head.contains("c_para"))
                    {
                        QJsonObject c_para = head["c_para"].toObject();
                        info.name = c_para["package"].toString();

                        // 判断是否�?SMD
                        bool isSmd = cadData.contains("SMT") && cadData["SMT"].toBool();
                        info.type = isSmd ? "smd" : "tht";

                        // 获取 3D 模型名称
                        if (c_para.contains("3DModel"))
                        {
                            info.model3DName = c_para["3DModel"].toString();
                        }

                        // 附加参数
                        info.link = c_para["link"].toString();
                        info.contributor = c_para["Contributor"].toString();
                        info.uuid3d = head["uuid_3d"].toString();
                    }

                    // 保存画布信息
                    if (dataStr.contains("canvas"))
                    {
                        info.canvas = dataStr["canvas"].toString();
                    }

                    // 保存层定�?
                    if (dataStr.contains("layers"))
                    {
                        QJsonArray layersArray = dataStr["layers"].toArray();
                        QStringList layers;
                        for (const QJsonValue &layerValue : layersArray)
                        {
                            layers.append(layerValue.toString());
                        }
                        info.layers = layers.join("\n");
                    }

                    // 保存对象可见�?
                    if (dataStr.contains("objects"))
                    {
                        QJsonArray objectsArray = dataStr["objects"].toArray();
                        QStringList objects;
                        for (const QJsonValue &objectValue : objectsArray)
                        {
                            objects.append(objectValue.toString());
                        }
                        info.objects = objects.join("\n");
                    }
                }

                // 导入边界框（包围盒）
                if (dataStr.contains("head"))
                {
                    QJsonObject head = dataStr["head"].toObject();
                    FootprintBBox footprintBbox;
                    if (dataStr.contains("BBox"))
                    {
                        QJsonObject bbox = dataStr["BBox"].toObject();
                        footprintBbox.x = bbox["x"].toDouble();
                        footprintBbox.y = bbox["y"].toDouble();
                        footprintBbox.width = bbox["width"].toDouble();
                        footprintBbox.height = bbox["height"].toDouble();
                    }
                    else
                    {
                        // 如果没有 BBox，使�?head.x �?head.y 作为中心�?
                        footprintBbox.x = head["x"].toDouble();
                        footprintBbox.y = head["y"].toDouble();
                        footprintBbox.width = 0;
                        footprintBbox.height = 0;
                    }
                    footprintData->setBbox(footprintBbox);
                }

                // 导入几何数据（焊盘、走线、孔等）
                if (dataStr.contains("shape"))
                {
                    QJsonArray shapes = dataStr["shape"].toArray();
                    // qDebug() << "Found" << shapes.size() << "shapes in footprint";

                    for (const QJsonValue &shapeValue : shapes)
                    {
                        QString shapeString = shapeValue.toString();
                        QStringList parts = shapeString.split("~");

                        if (parts.isEmpty())
                            continue;

                        QString designator = parts[0];

                        if (designator == "PAD")
                        {
                            // 导入焊盘
                            FootprintPad pad = importPadData(shapeString);
                            footprintData->addPad(pad);
                            // qDebug() << "Imported pad:" << pad.number;
                        }
                        else if (designator == "TRACK")
                        {
                            // 导入走线
                            FootprintTrack track = importTrackData(shapeString);
                            footprintData->addTrack(track);
                            // qDebug() << "Imported track";
                        }
                        else if (designator == "HOLE")
                        {
                            // 导入�?
                            FootprintHole hole = importHoleData(shapeString);
                            footprintData->addHole(hole);
                            // qDebug() << "Imported hole";
                        }
                        else if (designator == "CIRCLE")
                        {
                            // 导入�?
                            FootprintCircle circle = importFootprintCircleData(shapeString);
                            footprintData->addCircle(circle);
                            // qDebug() << "Imported circle";
                        }
                        else if (designator == "ARC")
                        {
                            // 导入圆弧
                            FootprintArc arc = importFootprintArcData(shapeString);
                            footprintData->addArc(arc);
                            // qDebug() << "Imported arc";
                        }
                        else if (designator == "RECT")
                        {
                            // 导入矩形
                            FootprintRectangle rectangle = importFootprintRectangleData(shapeString);
                            footprintData->addRectangle(rectangle);
                            // qDebug() << "Imported rectangle";
                        }
                        else if (designator == "TEXT")
                        {
                            // 导入文本
                            FootprintText text = importFootprintTextData(shapeString);
                            footprintData->addText(text);
                            // qDebug() << "Imported text:" << text.text;
                        }
                        else if (designator == "POLYLINE" || designator == "PL")
                        {
                            // 导入多段线（可能是丝印）
                            FootprintTrack track = importTrackData(shapeString); // 复用 track 格式
                            footprintData->addTrack(track);
                            // qDebug() << "Imported polyline (layer" << track.layerId << ")";
                        }
                        else if (designator == "POLYGON" || designator == "PG")
                        {
                            // 导入多边形（可能是丝印）
                            FootprintTrack track = importTrackData(shapeString); // 复用 track 格式
                            footprintData->addTrack(track);
                            // qDebug() << "Imported polygon (layer" << track.layerId << ")";
                        }
                        else if (designator == "PATH" || designator == "PT")
                        {
                            // 导入路径（可能是丝印�?
                            FootprintTrack track = importTrackData(shapeString); // 复用 track 格式
                            footprintData->addTrack(track);
                            // qDebug() << "Imported path (layer" << track.layerId << ")";
                        }
                        else if (designator == "SVGNODE")
                        {
                            // 导入 SVGNODE（可能是 3D 模型或外形轮廓）
                            // qDebug() << "Found SVGNODE, parsing...";
                            importSvgNodeData(shapeString, footprintData);
                        }
                        else if (designator == "SOLIDREGION")
                        {
                            // 导入实体填充区域（禁止布线区�?
                            FootprintSolidRegion solidRegion = importSolidRegionData(shapeString);
                            footprintData->addSolidRegion(solidRegion);
                            // qDebug() << "Imported solid region";
                        }
                    }

                    // 导入层定�?
                    if (dataStr.contains("layers"))
                    {
                        QJsonArray layers = dataStr["layers"].toArray();
                        // qDebug() << "Found" << layers.size() << "layers";
                        for (const QJsonValue &layerValue : layers)
                        {
                            LayerDefinition layer = parseLayerDefinition(layerValue.toString());
                            footprintData->addLayer(layer);
                        }
                    }

                    // 导入对象可见性配�?
                    if (dataStr.contains("objects"))
                    {
                        QJsonArray objects = dataStr["objects"].toArray();
                        // qDebug() << "Found" << objects.size() << "object visibility settings";
                        for (const QJsonValue &objectValue : objects)
                        {
                            ObjectVisibility visibility = parseObjectVisibility(objectValue.toString());
                            footprintData->addObjectVisibility(visibility);
                        }
                    }

                    // 修正类型判断：检查焊盘是否有�?
                    bool hasHole = false;
                    for (const FootprintPad &pad : footprintData->pads())
                    {
                        if (pad.holeRadius > 0 || pad.holeLength > 0)
                        {
                            hasHole = true;
                            break;
                        }
                    }
                    if (hasHole)
                    {
                        info.type = "tht";
                        // qDebug() << "Corrected type to THT (through-hole) because pads have holes";
                    }
                }

                footprintData->setInfo(info);
            }
        }

        return footprintData;
    }

    SymbolPin EasyedaImporter::importPinData(const QString &pinData)
    {
        SymbolPin pin;
        QList<QStringList> segments = parsePinDataString(pinData);

        if (segments.size() >= 7)
        {
            // �?0: 引脚设置（跳过第一个元�?"P"�?
            QStringList settings = segments[0];
            if (settings.size() >= 8)
            { // 需要至�?个元素（包括"P"�?
                pin.settings.isDisplayed = (settings[1] == "show");
                pin.settings.type = static_cast<PinType>(settings[2].toInt());
                // 注意：settings[3] �?spicePinNumber（如 6, 17, H10），这只是引脚的顺序索引
                // 真正�?BGA 引脚编号�?Segment 5 的索�?4 中（�?U6, U17, U16�?
                pin.settings.spicePinNumber = settings[3];
                pin.settings.posX = settings[4].toDouble();
                pin.settings.posY = settings[5].toDouble();
                pin.settings.rotation = settings[6].toInt();
                pin.settings.id = settings[7];
                pin.settings.isLocked = (settings.size() > 8 ? stringToBool(settings[8]) : false);
            }

            // �?1: pinDot
            if (segments[1].size() >= 2)
            {
                pin.pinDot.dotX = segments[1][0].toDouble();
                pin.pinDot.dotY = segments[1][1].toDouble();
            }

            // �?2: pinPath
            if (segments[2].size() >= 2)
            {
                pin.pinPath.path = segments[2][0];
                pin.pinPath.color = segments[2][1];
            }

            // �?3: name
            if (segments[3].size() >= 8)
            {
                pin.name.isDisplayed = (segments[3][0] == "show");
                pin.name.posX = segments[3][1].toDouble();
                pin.name.posY = segments[3][2].toDouble();
                pin.name.rotation = segments[3][3].toInt();
                pin.name.text = segments[3][4];
                pin.name.textAnchor = segments[3][5];
                pin.name.font = segments[3][6];
                pin.name.fontSize = segments[3][7].toDouble();
            }

            // �?4: dot (SymbolPinDotBis) - 这里包含圆圈显示标志�?BGA 引脚编号
            // 格式: 1~555~929~0~U17~start~~~#0000FF
            // �?~ 分割�? ["1", "555", "929", "0", "U17", "start", "", "", "#0000FF"]
            // 索引 0: dotShow (是否显示圆圈�?=显示�?=不显�?
            // 索引 1-2: dotX, dotY (圆圈坐标)
            // 索引 4: BGA 引脚编号
            if (segments[4].size() >= 5)
            {
                // 调试：输出段 4 的数�?
                qDebug() << "Pin Segment 4 data:" << segments[4];
                qDebug() << "  Segment 4[0] (dotShow):" << segments[4][0];
                
                // 修复：始终不显示圆圈
                pin.dot.isDisplayed = false;
                
                // 设置圆圈坐标（虽然不显示，但仍然保存坐标信息�?
                pin.dot.circleX = segments[4][1].toDouble();
                pin.dot.circleY = segments[4][2].toDouble();
                
                qDebug() << "  dot.isDisplayed set to: false (always hide)";

                // 检�?spicePinNumber 是否已经�?BGA 引脚编号（包含字母）
                bool isAlreadyBGA = false;
                for (QChar c : pin.settings.spicePinNumber)
                {
                    if (c.isLetter())
                    {
                        isAlreadyBGA = true;
                        break;
                    }
                }

                // 如果 spicePinNumber 只是纯数字，尝试�?Segment 4 提取 BGA 引脚编号
                if (!isAlreadyBGA)
                {
                    // 提取 BGA 引脚编号（在�?4 的索�?4 中，�?"U6", "U17", "U16"�?
                    QString bgaPinNumber = segments[4][4];
                    // 使用 BGA 引脚编号替换 spicePinNumber
                    if (!bgaPinNumber.isEmpty())
                    {
                        pin.settings.spicePinNumber = bgaPinNumber;
                        qDebug() << "BGA Pin Number extracted from Segment 4:" << bgaPinNumber << "for pin" << pin.name.text << "(replaced spicePinNumber)";
                    }
                }
            }

            // �?5: 跳过（不使用�?

            // �?6: clock
            if (segments[6].size() >= 2)
            {
                pin.clock.isDisplayed = (segments[6][0] == "show");
                pin.clock.path = segments[6][1];
            }
        }

        return pin;
    }

    SymbolRectangle EasyedaImporter::importRectangleData(const QString &rectangleData)
    {
        SymbolRectangle rectangle;
        QStringList fields = parseDataString(rectangleData);

        // qDebug() << "=== Rectangle Parsing Debug ===";
        // qDebug() << "Raw data:" << rectangleData;
        // qDebug() << "Fields count:" << fields.size();
        // qDebug() << "Fields:" << fields;

        // 跳过第一个字段（类型标识 'R'），从索�?1 开始读�?
        if (fields.size() >= 13)
        {
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

            // qDebug() << "Parsed - posX:" << rectangle.posX << "posY:" << rectangle.posY
            // << "width:" << rectangle.width << "height:" << rectangle.height;
        }

        return rectangle;
    }

    SymbolCircle EasyedaImporter::importCircleData(const QString &circleData)
    {
        SymbolCircle circle;
        QStringList fields = parseDataString(circleData);

        if (fields.size() >= 10)
        {
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

    SymbolArc EasyedaImporter::importArcData(const QString &arcData)
    {
        SymbolArc arc;
        QStringList fields = parseDataString(arcData);

        if (fields.size() >= 8)
        {
            // 路径数据需要特殊处�?
            // arc.path = fields[0].split(",");
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

    SymbolEllipse EasyedaImporter::importEllipseData(const QString &ellipseData)
    {
        SymbolEllipse ellipse;
        QStringList fields = parseDataString(ellipseData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 10)
        {
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

    SymbolPolyline EasyedaImporter::importPolylineData(const QString &polylineData)
    {
        SymbolPolyline polyline;
        QStringList fields = parseDataString(polylineData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 8)
        {
            polyline.points = fields[1];
            polyline.strokeColor = fields[2];
            polyline.strokeWidth = fields[3].toDouble();
            polyline.strokeStyle = fields[4];
            polyline.fillColor = stringToBool(fields[5]);
            polyline.id = fields[6];
            polyline.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;

            // qDebug() << "=== Polyline Debug ===";
            // qDebug() << "Raw fillColor field:" << fields[5];
            // qDebug() << "Parsed fillColor:" << polyline.fillColor;
            // qDebug() << "Points:" << polyline.points;
        }

        return polyline;
    }

    SymbolPolygon EasyedaImporter::importPolygonData(const QString &polygonData)
    {
        SymbolPolygon polygon;
        QStringList fields = parseDataString(polygonData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 8)
        {
            polygon.points = fields[1];
            polygon.strokeColor = fields[2];
            polygon.strokeWidth = fields[3].toDouble();
            polygon.strokeStyle = fields[4];
            polygon.fillColor = stringToBool(fields[5]);
            polygon.id = fields[6];
            polygon.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;

            // qDebug() << "=== Polygon Debug ===";
            // qDebug() << "Raw fillColor field:" << fields[5];
            // qDebug() << "Parsed fillColor:" << polygon.fillColor;
            // qDebug() << "Points:" << polygon.points;
        }

        return polygon;
    }

    SymbolPath EasyedaImporter::importPathData(const QString &pathData)
    {
        SymbolPath path;
        QStringList fields = parseDataString(pathData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 8)
        {
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

    SymbolText EasyedaImporter::importTextData(const QString &textData)
    {
        SymbolText text;
        QStringList fields = parseDataString(textData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        // EasyEDA文本元素格式参考lckiconverter实现
        // ["TEXT", id, x, y, rotate, text, styleName, mayLocked]
        if (fields.size() >= 15)
        {
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

    FootprintPad EasyedaImporter::importPadData(const QString &padData)
    {
        FootprintPad pad;
        QStringList fields = parseDataString(padData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 17)
        {
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

    FootprintTrack EasyedaImporter::importTrackData(const QString &trackData)
    {
        FootprintTrack track;
        QStringList fields = parseDataString(trackData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 6)
        {
            track.strokeWidth = fields[1].toDouble();
            track.layerId = fields[2].toInt();
            track.net = fields[3];
            track.points = fields[4];
            track.id = fields[5];
            track.isLocked = fields.size() > 6 ? stringToBool(fields[6]) : false;
        }

        return track;
    }

    FootprintHole EasyedaImporter::importHoleData(const QString &holeData)
    {
        FootprintHole hole;
        QStringList fields = parseDataString(holeData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 5)
        {
            hole.centerX = fields[1].toDouble();
            hole.centerY = fields[2].toDouble();
            hole.radius = fields[3].toDouble();
            hole.id = fields[4];
            hole.isLocked = fields.size() > 5 ? stringToBool(fields[5]) : false;
        }

        return hole;
    }

    FootprintCircle EasyedaImporter::importFootprintCircleData(const QString &circleData)
    {
        FootprintCircle circle;
        QStringList fields = parseDataString(circleData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 7)
        {
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

    FootprintRectangle EasyedaImporter::importFootprintRectangleData(const QString &rectangleData)
    {
        FootprintRectangle rectangle;
        QStringList fields = parseDataString(rectangleData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        // 格式: RECT~x~y~width~height~layerId~id~strokeWidth~isLocked~fillColor~strokeStyle~...
        if (fields.size() >= 8)
        {
            rectangle.x = fields[1].toDouble();
            rectangle.y = fields[2].toDouble();
            rectangle.width = fields[3].toDouble();
            rectangle.height = fields[4].toDouble();
            rectangle.layerId = fields[5].toInt(); // layerId 在第 6 个字�?
            rectangle.id = fields[6];
            rectangle.strokeWidth = fields[7].toDouble(); // strokeWidth 在第 8 个字�?
            rectangle.isLocked = fields.size() > 8 ? stringToBool(fields[8]) : false;
        }

        return rectangle;
    }

    FootprintArc EasyedaImporter::importFootprintArcData(const QString &arcData)
    {
        FootprintArc arc;
        QStringList fields = parseDataString(arcData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 7)
        {
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

    FootprintText EasyedaImporter::importFootprintTextData(const QString &textData)
    {
        FootprintText text;
        QStringList fields = parseDataString(textData);

        // 跳过第一个字段（设计器），从第二个字段开始解�?
        if (fields.size() >= 14)
        {
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

    QStringList EasyedaImporter::parseDataString(const QString &data) const
    {
        // 使用 Qt::KeepEmptyParts 保留空字段，这样可以正确处理连续�?~~
        return data.split("~", Qt::KeepEmptyParts);
    }

    QList<QStringList> EasyedaImporter::parsePinDataString(const QString &pinData) const
    {
        QList<QStringList> result;

        // 只分割顶层的 "^^"，保留嵌套的 "^^"
        // 策略：遇�?^^ 时分割，^^ 标志着新段落的开�?
        QStringList segments;
        int start = 0;
        int pos = 0;

        while (pos < pinData.length())
        {
            // 检查是否遇�?^^
            if (pinData[pos] == '^' && pos + 1 < pinData.length() && pinData[pos + 1] == '^')
            {
                segments.append(pinData.mid(start, pos - start));
                pos += 2; // 跳过 ^^
                start = pos;
                continue;
            }

            pos++;
        }

        // 添加最后一个段
        if (start < pinData.length())
        {
            segments.append(pinData.mid(start));
        }

        // 对每个段�?~ 分割
        for (const QString &segment : segments)
        {
            QStringList subSegments = segment.split("~", Qt::KeepEmptyParts);
            result.append(subSegments);
        }

        return result;
    }

    FootprintSolidRegion EasyedaImporter::importSolidRegionData(const QString &solidRegionData)
    {
        FootprintSolidRegion region;
        QStringList fields = parseDataString(solidRegionData);

        // SOLIDREGION~layerId~~path~fillStyle~id~~~~isLocked
        // 示例: SOLIDREGION~99~~M 3984.1457 2975.3938 L 4046.5657 2975.3938 L 4046.5657 3024.6063 L 3984.1457 3024.6063 Z~solid~gge55~~~~0
        if (fields.size() >= 6)
        {
            region.layerId = fields[1].toInt();
            region.path = fields[3]; // 路径数据（如 "M x y L x y Z"�?
            region.fillStyle = fields[4];
            region.id = fields[5];
            region.isLocked = fields.size() > 7 ? stringToBool(fields[7]) : false;

            // 判断是否为禁止布线区（通常�?ComponentShapeLayer，ID=99�?
            region.isKeepOut = (region.layerId == 99);

            // qDebug() << "Imported solid region - Layer:" << region.layerId
            // << "IsKeepOut:" << region.isKeepOut
            // << "Path length:" << region.path.length();
        }

        return region;
    }

    void EasyedaImporter::importSvgNodeData(const QString &svgNodeData, QSharedPointer<FootprintData> footprintData)
    {
        QStringList parts = svgNodeData.split("~");
        if (parts.size() < 2)
        {
            qWarning() << "Invalid SVGNODE data format";
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(parts[1].toUtf8());
        if (!doc.isObject())
        {
            qWarning() << "Failed to parse SVGNODE JSON";
            return;
        }

        QJsonObject root = doc.object();
        if (!root.contains("attrs"))
        {
            qWarning() << "SVGNODE missing attrs";
            return;
        }

        QJsonObject attrs = root["attrs"].toObject();

        // 检查是否为外形轮廓（c_etype == "outline3D"�?
        if (attrs.contains("c_etype") && attrs["c_etype"].toString() == "outline3D")
        {
            // 这是外形轮廓，但同时也包�?3D 模型�?UUID
            // 先提�?3D 模型�?UUID（从 SVGNODE attrs.uuid�?
            Model3DData model3D;
            if (attrs.contains("uuid"))
            {
                QString modelUuid = attrs["uuid"].toString();
                model3D.setUuid(modelUuid);
                model3D.setName(attrs.contains("title") ? attrs["title"].toString() : "");

                // 解析平移
                if (attrs.contains("c_origin"))
                {
                    QString c_origin = attrs["c_origin"].toString();
                    QStringList originParts = c_origin.split(",");
                    if (originParts.size() >= 2)
                    {
                        Model3DBase translation;
                        translation.x = originParts[0].toDouble();
                        translation.y = originParts[1].toDouble();
                        translation.z = attrs.contains("z") ? attrs["z"].toDouble() : 0.0;
                        model3D.setTranslation(translation);
                    }
                }

                // 解析旋转
                if (attrs.contains("c_rotation"))
                {
                    QString c_rotation = attrs["c_rotation"].toString();
                    QStringList rotationParts = c_rotation.split(",");
                    if (rotationParts.size() >= 3)
                    {
                        Model3DBase rotation;
                        rotation.x = rotationParts[0].toDouble();
                        rotation.y = rotationParts[1].toDouble();
                        rotation.z = rotationParts[2].toDouble();
                        model3D.setRotation(rotation);
                    }
                }

                footprintData->setModel3D(model3D);
                // qDebug() << "3D model info (from SVGNODE) - Name:" << model3D.name() << "UUID:" << model3D.uuid();
            }

            // 然后解析外形轮廓
            FootprintOutline outline;
            outline.id = attrs["id"].toString();
            outline.strokeWidth = attrs.contains("c_width") ? attrs["c_width"].toString().toDouble() : 0.0;
            outline.isLocked = false;

            // 获取�?ID
            if (attrs.contains("layerid"))
            {
                outline.layerId = attrs["layerid"].toString().toInt();
            }
            else
            {
                outline.layerId = 19; // 默认�?3DModel �?
            }

            // 解析 SVG 路径
            if (root.contains("childNodes"))
            {
                QJsonArray childNodes = root["childNodes"].toArray();
                for (const QJsonValue &child : childNodes)
                {
                    QJsonObject childObj = child.toObject();
                    if (childObj.contains("attrs"))
                    {
                        QJsonObject childAttrs = childObj["attrs"].toObject();
                        if (childAttrs.contains("points"))
                        {
                            outline.path = childAttrs["points"].toString();
                            break; // 只取第一个路�?
                        }
                    }
                }
            }

            footprintData->addOutline(outline);
            // qDebug() << "Imported outline - Layer:" << outline.layerId
            // << "Path length:" << outline.path.length();
        }
        else
        {
            // 这是 3D 模型（非 outline3D 类型�?
            Model3DData model3D;
            model3D.setName(attrs.contains("title") ? attrs["title"].toString() : "");
            model3D.setUuid(attrs.contains("uuid") ? attrs["uuid"].toString() : "");

            // qDebug() << "3D model info - Name:" << model3D.name() << "UUID:" << model3D.uuid();

            // 解析平移
            if (attrs.contains("c_origin"))
            {
                QString c_origin = attrs["c_origin"].toString();
                QStringList originParts = c_origin.split(",");
                if (originParts.size() >= 2)
                {
                    Model3DBase translation;
                    translation.x = originParts[0].toDouble();
                    translation.y = originParts[1].toDouble();
                    translation.z = attrs.contains("z") ? attrs["z"].toDouble() : 0.0;
                    model3D.setTranslation(translation);
                    // qDebug() << "3D model translation:" << translation.x << translation.y << translation.z;
                }
            }

            // 解析旋转
            if (attrs.contains("c_rotation"))
            {
                QString c_rotation = attrs["c_rotation"].toString();
                QStringList rotationParts = c_rotation.split(",");
                if (rotationParts.size() >= 3)
                {
                    Model3DBase rotation;
                    rotation.x = rotationParts[0].toDouble();
                    rotation.y = rotationParts[1].toDouble();
                    rotation.z = rotationParts[2].toDouble();
                    model3D.setRotation(rotation);
                    // qDebug() << "3D model rotation:" << rotation.x << rotation.y << rotation.z;
                }
            }

            footprintData->setModel3D(model3D);
            // qDebug() << "Imported 3D model - Name:" << model3D.name() << "UUID:" << model3D.uuid();
        }
    }

    LayerDefinition EasyedaImporter::parseLayerDefinition(const QString &layerString)
    {
        LayerDefinition layer;
        // 格式: layerId~name~color~isVisible~isUsedForManufacturing~~expansion
        // 示例: 1~TopLayer~#FF0000~true~true~~
        QStringList fields = layerString.split("~");

        if (fields.size() >= 5)
        {
            layer.layerId = fields[0].toInt();
            layer.name = fields[1];
            layer.color = fields[2];
            layer.isVisible = (fields[3] == "true");
            layer.isUsedForManufacturing = (fields[4] == "true");

            // 解析扩展值（如阻焊层扩展�?
            if (fields.size() >= 7)
            {
                layer.expansion = fields[6].toDouble();
            }

            // qDebug() << "Parsed layer - ID:" << layer.layerId
            // << "Name:" << layer.name
            // << "Color:" << layer.color;
        }

        return layer;
    }

    ObjectVisibility EasyedaImporter::parseObjectVisibility(const QString &objectString)
    {
        ObjectVisibility visibility;
        // 格式: objectType~isEnabled~isVisible
        // 示例: Text~true~false
        QStringList fields = objectString.split("~");

        if (fields.size() >= 3)
        {
            visibility.objectType = fields[0];
            visibility.isEnabled = (fields[1] == "true");
            visibility.isVisible = (fields[2] == "true");

            // qDebug() << "Parsed object visibility - Type:" << visibility.objectType
            // << "Enabled:" << visibility.isEnabled
            // << "Visible:" << visibility.isVisible;
        }

        return visibility;
    }

    bool EasyedaImporter::stringToBool(const QString &str) const
    {
        // 空值�?、false、none、transparent 视为 false
        if (str.isEmpty() || str == "0" || str.toLower() == "false" ||
            str.toLower() == "none" || str.toLower() == "transparent")
        {
            return false;
        }
        // 1、true、show 或任何其他值（包括颜色值）视为 true
        return true;
    }
} // namespace EasyKiConverter
