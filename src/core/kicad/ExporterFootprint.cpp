#include "ExporterFootprint.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include "src/core/utils/GeometryUtils.h"

namespace EasyKiConverter {

ExporterFootprint::ExporterFootprint(QObject *parent)
    : QObject(parent)
    , m_kicadVersion(KicadVersion::V6)
{
}

ExporterFootprint::~ExporterFootprint()
{
}

void ExporterFootprint::setKicadVersion(KicadVersion version)
{
    m_kicadVersion = version;
}

ExporterFootprint::KicadVersion ExporterFootprint::getKicadVersion() const
{
    return m_kicadVersion;
}

bool ExporterFootprint::exportFootprint(const FootprintData &footprintData, const QString &filePath, const QString &model3DPath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // Generate footprint content with 3D model path
    QString content = generateFootprintContent(footprintData, model3DPath);

    out << content;
    file.flush();
    file.close();

    qDebug() << "Footprint exported to:" << filePath;
    return true;
}

bool ExporterFootprint::exportFootprintLibrary(const QList<FootprintData> &footprints, const QString &libName, const QString &filePath)
{
    qDebug() << "=== Export Footprint Library ===";
    qDebug() << "Library name:" << libName;
    qDebug() << "Output path:" << filePath;
    qDebug() << "Footprint count:" << footprints.count();

    // filePath 应该是指向 .pretty 文件夹的路径
    QDir libDir(filePath);

    // 如果文件夹不存在，创建它
    if (!libDir.exists()) {
        qDebug() << "Creating footprint library directory:" << filePath;
        if (!libDir.mkpath(".")) {
            qWarning() << "Failed to create footprint library directory:" << filePath;
            return false;
        }
    }

    // 读取现有的封装文件名
    QSet<QString> existingFootprintNames;
    QStringList existingFiles = libDir.entryList(QStringList("*.kicad_mod"), QDir::Files);
    for (const QString &fileName : existingFiles) {
        // 从文件名中提取封装名称（去掉 .kicad_mod 后缀）
        QString footprintName = fileName;
        footprintName.remove(".kicad_mod");
        existingFootprintNames.insert(footprintName);
        qDebug() << "Found existing footprint:" << footprintName;
    }

    qDebug() << "Existing footprints count:" << existingFootprintNames.count();

    // 导出每个封装
    int exportedCount = 0;
    int overwrittenCount = 0;
    for (const FootprintData &footprint : footprints) {
        QString footprintName = footprint.info().name;
        QString fileName = footprintName + ".kicad_mod";
        QString fullPath = libDir.filePath(fileName);

        bool exists = existingFootprintNames.contains(footprintName);
        if (exists) {
            qDebug() << "Overwriting existing footprint:" << footprintName;
            overwrittenCount++;
        } else {
            qDebug() << "Exporting new footprint:" << footprintName;
            exportedCount++;
        }

        // 生成封装内容
        QString content = generateFootprintContent(footprint);

        // 写入文件
        QFile file(fullPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to open file for writing:" << fullPath;
            continue;
        }

        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << content;
        file.close();

        qDebug() << "  Exported to:" << fullPath;
    }

    qDebug() << "Export complete - New:" << exportedCount << "Overwritten:" << overwrittenCount;
    qDebug() << "Footprint library exported to:" << filePath;

    return true;
}

QString ExporterFootprint::generateHeader(const QString &libName) const
{
    // KiCad 6.x format
    return QString("(kicad_pcb (version 20221018) (generator easyeda2kicad)\n"
                   "  (version 6)\n"
                   "  (generator \"EasyKiConverter\")\n"
                   "  (name \"%1\")\n\n").arg(libName);
}

QString ExporterFootprint::generateFootprintContent(const FootprintData &footprintData, const QString &model3DPath) const
{
    QString content;

    // KiCad 6.x format - use module syntax (compatible with KiCad 6+)
    content += QString("(module easyeda2kicad:%1 (layer F.Cu) (tedit 5DC5F6A4)\n").arg(footprintData.info().name);
    
    // 判断是否为通孔器件：如果有焊盘有孔（holeRadius > 0），则为通孔
    bool isThroughHole = false;
    for (const FootprintPad &pad : footprintData.pads()) {
        if (pad.holeRadius > 0) {
            isThroughHole = true;
            break;
        }
    }

    // 设置正确的属性
    // KiCad只允许以下属性值：through_hole、smd、virtual、board_only、exclude_from_pos_files、exclude_from_bom、allow_solder_mask_bridges
    if (isThroughHole) {
        content += "\t(attr through_hole)\n";
    } else {
        content += "\t(attr smd)\n";
    }

    // KiCad的attr属性只允许特定的值，不能随意添加自定义类型
    // 如果需要额外的属性，必须使用KiCad支持的属性名
    // 这里我们忽略EasyEDA的自定义类型，只使用through_hole或smd
    // 如果将来需要支持其他属性（如virtual、board_only等），需要根据实际需求添加

    // Calculate Y positions for reference and value text
    double yLow = 0;
    double yHigh = 0;
    if (!footprintData.pads().isEmpty()) {
        yLow = footprintData.pads().first().centerY;
        yHigh = footprintData.pads().first().centerY;
        for (const FootprintPad &pad : footprintData.pads()) {
            if (pad.centerY < yLow) yLow = pad.centerY;
            if (pad.centerY > yHigh) yHigh = pad.centerY;
        }
    }

    // Get bounding box center offset (use center as origin instead of top-left corner)
    double bboxX = footprintData.bbox().x + footprintData.bbox().width / 2;
    double bboxY = footprintData.bbox().y + footprintData.bbox().height / 2;

    // Reference text
    content += QString("\t(fp_text reference REF** (at 0 %1) (layer F.SilkS)\n").arg(pxToMm(yLow - bboxY - 4));
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    // Value text
    content += QString("\t(fp_text value %1 (at 0 %2) (layer F.Fab)\n").arg(footprintData.info().name).arg(pxToMm(yHigh - bboxY + 4));
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    // User reference (Fab layer)
    content += "\t(fp_text user %R (at 0 0) (layer F.Fab)\n";
    content += "\t\t(effects (font (size 1 1) (thickness 0.15)))\n";
    content += "\t)\n";

    // 检查丝印层是否只有文字
    bool silkScreenHasAnyGraphics = false;
    int silkScreenGraphicsCount = 0;

    // 检查丝印层是否有任何图形元素（tracks, circles, rectangles, arcs）
    // 使用 layerIdToKicad 函数来判断哪些层应该被视为丝印层
    auto isSilkScreenLayer = [this](int layerId) -> bool {
        QString kicadLayer = layerIdToKicad(layerId);
        return kicadLayer == "F.SilkS" || kicadLayer == "B.SilkS";
    };

    for (const FootprintTrack &track : footprintData.tracks()) {
        if (isSilkScreenLayer(track.layerId)) {
            silkScreenHasAnyGraphics = true;
            silkScreenGraphicsCount++;
        }
    }
    for (const FootprintCircle &circle : footprintData.circles()) {
        if (isSilkScreenLayer(circle.layerId)) {
            silkScreenHasAnyGraphics = true;
            silkScreenGraphicsCount++;
        }
    }
    for (const FootprintRectangle &rect : footprintData.rectangles()) {
        if (isSilkScreenLayer(rect.layerId)) {
            silkScreenHasAnyGraphics = true;
            silkScreenGraphicsCount++;
        }
    }
    for (const FootprintArc &arc : footprintData.arcs()) {
        if (isSilkScreenLayer(arc.layerId)) {
            silkScreenHasAnyGraphics = true;
            silkScreenGraphicsCount++;
        }
    }

    qDebug() << "Silkscreen check - Has graphics:" << silkScreenHasAnyGraphics << "Count:" << silkScreenGraphicsCount;

    // 检查 Fab 层有多少图形元素
    int fabGraphicsCount = 0;
    for (const FootprintTrack &track : footprintData.tracks()) {
        if (track.layerId == 13) fabGraphicsCount++;
    }
    for (const FootprintCircle &circle : footprintData.circles()) {
        if (circle.layerId == 13) fabGraphicsCount++;
    }
    for (const FootprintRectangle &rect : footprintData.rectangles()) {
        if (rect.layerId == 13) fabGraphicsCount++;
    }
    for (const FootprintArc &arc : footprintData.arcs()) {
        if (arc.layerId == 13) fabGraphicsCount++;
    }

    qDebug() << "Fab layer graphics count:" << fabGraphicsCount;

    // 生成丝印层图形元素
    for (const FootprintTrack &track : footprintData.tracks()) {
        if (isSilkScreenLayer(track.layerId)) {
            content += generateTrack(track, bboxX, bboxY);
        }
    }
    for (const FootprintCircle &circle : footprintData.circles()) {
        if (isSilkScreenLayer(circle.layerId)) {
            content += generateCircle(circle, bboxX, bboxY);
        }
    }
    for (const FootprintRectangle &rect : footprintData.rectangles()) {
        if (isSilkScreenLayer(rect.layerId)) {
            content += generateRectangle(rect, bboxX, bboxY);
        }
    }
    for (const FootprintArc &arc : footprintData.arcs()) {
        if (isSilkScreenLayer(arc.layerId)) {
            content += generateArc(arc, bboxX, bboxY);
        }
    }

    // 为 layer=100 (LeadShapeLayer) 的 CIRCLE 额外生成 F.SilkS 版本（引脚丝印标记）
    // 为 layer=101 (ComponentPolarityLayer) 的 CIRCLE 放大直径（极性标记）
    for (const FootprintCircle &circle : footprintData.circles()) {
        if (circle.layerId == 100) {
            // 复制 circle 并修改 layerId 为 3 (F.SilkS)
            FootprintCircle silkCircle = circle;
            silkCircle.layerId = 3;
            content += generateCircle(silkCircle, bboxX, bboxY);
            qDebug() << "  Duplicated circle from layer 100 (LeadShapeLayer) to F.SilkS";
        } else if (circle.layerId == 101) {
            // 极性标记圆，放大直径到 0.5mm
            FootprintCircle enlargedCircle = circle;
            enlargedCircle.radius = 0.25; // 半径 0.25mm，直径 0.5mm
            enlargedCircle.strokeWidth = 0.06; // 线宽 0.06mm
            content += generateCircle(enlargedCircle, bboxX, bboxY);
            qDebug() << "  Enlarged polarity circle from layer 101 to 0.5mm diameter";
        }
    }

    // 如果丝印层没有图形元素，将其他层的图形复制到 F.SilkS 层
    if (!silkScreenHasAnyGraphics) {
        qDebug() << "Silkscreen has no graphics, duplicating graphics from other layers to F.SilkS";

        int duplicatedCount = 0;

        // 检查所有层，打印有图形的层
        qDebug() << "=== Checking all layers for graphics ===";
        QSet<int> layersWithGraphics;
        for (const FootprintTrack &track : footprintData.tracks()) {
            if (track.layerId != 3 && track.layerId != 4 && !track.points.isEmpty()) {
                layersWithGraphics.insert(track.layerId);
            }
        }
        for (const FootprintCircle &circle : footprintData.circles()) {
            if (circle.layerId != 3 && circle.layerId != 4) {
                layersWithGraphics.insert(circle.layerId);
            }
        }
        for (const FootprintRectangle &rect : footprintData.rectangles()) {
            if (rect.layerId != 3 && rect.layerId != 4) {
                layersWithGraphics.insert(rect.layerId);
            }
        }
        for (const FootprintArc &arc : footprintData.arcs()) {
            if (arc.layerId != 3 && arc.layerId != 4 && !arc.path.isEmpty()) {
                layersWithGraphics.insert(arc.layerId);
            }
        }

        if (!layersWithGraphics.isEmpty()) {
            qDebug() << "Layers with graphics:";
            for (int layerId : layersWithGraphics) {
                QString layerName = layerIdToKicad(layerId);
                int trackCount = 0, circleCount = 0, rectCount = 0, arcCount = 0;
                
                // 统计每种图形的数量
                for (const FootprintTrack &track : footprintData.tracks()) {
                    if (track.layerId == layerId && !track.points.isEmpty()) trackCount++;
                }
                for (const FootprintCircle &circle : footprintData.circles()) {
                    if (circle.layerId == layerId) circleCount++;
                }
                for (const FootprintRectangle &rect : footprintData.rectangles()) {
                    if (rect.layerId == layerId) rectCount++;
                }
                for (const FootprintArc &arc : footprintData.arcs()) {
                    if (arc.layerId == layerId && !arc.path.isEmpty()) arcCount++;
                }
                
                int totalCount = trackCount + circleCount + rectCount + arcCount;
                qDebug() << "  Layer" << layerId << "(" << layerName << "):" 
                         << totalCount << "graphics"
                         << "[tracks:" << trackCount 
                         << " circles:" << circleCount 
                         << " rects:" << rectCount 
                         << " arcs:" << arcCount << "]";
            }
        } else {
            qDebug() << "No graphics found in any layer!";
        }
        qDebug() << "=== End layer check ===";

        // 判断一个层是否应该复制到丝印层（非铜层、非丝印层、非Mask层、非Paste层）
        auto shouldDuplicateLayer = [](int layerId) -> bool {
            // 不复制铜层（1, 2, 9, 10, 11）
            if (layerId >= 1 && layerId <= 2) return false;
            if (layerId >= 9 && layerId <= 11) return false;
            // 不复制丝印层（3, 4）
            if (layerId == 3 || layerId == 4) return false;
            // 不复制Paste层（5, 6）
            if (layerId == 5 || layerId == 6) return false;
            // 不复制Mask层（7, 8）
            if (layerId == 7 || layerId == 8) return false;
            // 不复制边缘切割层（14）
            if (layerId == 14) return false;
            // 其他层（如0, 12, 13, 15, 20, 21, 22, 100, 101等）都复制
            return true;
        };

        // 复制符合条件的层的 tracks（线条）
        for (const FootprintTrack &track : footprintData.tracks()) {
            if (shouldDuplicateLayer(track.layerId) && !track.points.isEmpty()) {
                FootprintTrack tempTrack = track;
                tempTrack.layerId = 3; // F.SilkS
                content += generateTrack(tempTrack, bboxX, bboxY);
                duplicatedCount++;
                qDebug() << "  Duplicated track from layer" << track.layerId << "to F.SilkS";
            }
        }
        // 复制符合条件的层的 circles（圆）
        for (const FootprintCircle &circle : footprintData.circles()) {
            if (shouldDuplicateLayer(circle.layerId)) {
                FootprintCircle tempCircle = circle;
                tempCircle.layerId = 3;
                content += generateCircle(tempCircle, bboxX, bboxY);
                duplicatedCount++;
                qDebug() << "  Duplicated circle from layer" << circle.layerId << "to F.SilkS";
            }
        }
        // 复制符合条件的层的 rectangles（矩形）
        for (const FootprintRectangle &rect : footprintData.rectangles()) {
            if (shouldDuplicateLayer(rect.layerId)) {
                FootprintRectangle tempRect = rect;
                tempRect.layerId = 3;
                content += generateRectangle(tempRect, bboxX, bboxY);
                duplicatedCount++;
                qDebug() << "  Duplicated rectangle from layer" << rect.layerId << "to F.SilkS";
            }
        }
        // 复制符合条件的层的 arcs（圆弧）
        for (const FootprintArc &arc : footprintData.arcs()) {
            if (shouldDuplicateLayer(arc.layerId) && !arc.path.isEmpty()) {
                FootprintArc tempArc = arc;
                tempArc.layerId = 3;
                content += generateArc(tempArc, bboxX, bboxY);
                duplicatedCount++;
                qDebug() << "  Duplicated arc from layer" << arc.layerId << "to F.SilkS";
            }
        }

        qDebug() << "Total duplicated graphics to F.SilkS:" << duplicatedCount;
    }

    // 生成 Fab 层图形元素（非丝印层）
    for (const FootprintTrack &track : footprintData.tracks()) {
        if (track.layerId != 3 && track.layerId != 4) { // 非丝印层
            content += generateTrack(track, bboxX, bboxY);
        }
    }
    for (const FootprintRectangle &rect : footprintData.rectangles()) {
        if (rect.layerId != 3 && rect.layerId != 4) {
            content += generateRectangle(rect, bboxX, bboxY);
        }
    }

    // Generate pads
    for (const FootprintPad &pad : footprintData.pads()) {
        content += generatePad(pad, bboxX, bboxY);
    }

    // Generate holes
    for (const FootprintHole &hole : footprintData.holes()) {
        content += generateHole(hole, bboxX, bboxY);
    }

    // Generate circles (非丝印层)
    for (const FootprintCircle &circle : footprintData.circles()) {
        if (circle.layerId != 3 && circle.layerId != 4) {
            content += generateCircle(circle, bboxX, bboxY);
        }
    }

    // Generate arcs (非丝印层)
    for (const FootprintArc &arc : footprintData.arcs()) {
        if (arc.layerId != 3 && arc.layerId != 4) {
            content += generateArc(arc, bboxX, bboxY);
        }
    }

    // Generate texts
    for (const FootprintText &text : footprintData.texts()) {
        content += generateText(text, bboxX, bboxY);
    }

    // Generate 3D model reference
        if (!footprintData.model3D().name().isEmpty()) {
            content += generateModel3D(footprintData.model3D(), bboxX, bboxY, model3DPath);
        }
    
        content += ")\n";
        
        return content;
    }QString ExporterFootprint::generatePad(const FootprintPad &pad, double bboxX, double bboxY) const
{
    QString content;
    double x = pxToMm(pad.centerX - bboxX);
    double y = pxToMm(pad.centerY - bboxY);
    double width = pxToMm(pad.width);
    double height = pxToMm(pad.height);
    double holeRadius = pxToMm(pad.holeRadius);

    // 智能判断焊盘形状：当 width 和 height 相等时使用 circle
    QString kicadShape;
    if (qAbs(width - height) < 1e-3) {
        kicadShape = "circle";
    } else {
        kicadShape = padShapeToKicad(pad.shape);
    }

    QString kicadType = padTypeToKicad(pad.layerId);
    QString layers = padLayersToKicad(pad.layerId);
    
    QString drillStr;
    if (holeRadius > 0) {
        double drillDiameter = holeRadius * 2;
        // 限制为2位小数，避免精度冗余（如 2.30002）
        drillStr = QString(" (drill %1)").arg(drillDiameter, 0, 'f', 2);
    }

    // Process pad number: extract content between parentheses if present
    QString padNumber = pad.number;
    if (padNumber.contains("(") && padNumber.contains(")")) {
        int start = padNumber.indexOf("(");
        int end = padNumber.indexOf(")");
        padNumber = padNumber.mid(start + 1, end - start - 1);
    }

    // Convert rotation to match Python version: if > 180, convert to negative
    double rotation = pad.rotation;
    if (rotation > 180.0) {
        rotation = rotation - 360.0;
    }

    // Ensure minimum size
    width = qMax(width, 0.01);
    height = qMax(height, 0.01);

    // KiCad 6.x format - match Python version exactly
    // Note: No quotes around pad number, use rect instead of custom
    content += QString("\t(pad %1 %2 %3 (at %4 %5 %6) (size %7 %8) (layers %9)%10)\n")
        .arg(padNumber)
        .arg(kicadType)
        .arg(kicadShape)
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(rotation, 0, 'f', 2)
        .arg(width, 0, 'f', 2)
        .arg(height, 0, 'f', 2)
        .arg(layers)
        .arg(drillStr);

    return content;
}

QString ExporterFootprint::generateTrack(const FootprintTrack &track, double bboxX, double bboxY) const
{
    QString content;
    
    // Parse points from the string
    QStringList pointList = track.points.split(" ", Qt::SkipEmptyParts);
    
    // KiCad 5.x format - generate line segments from points
    for (int i = 0; i < pointList.size() - 3; i += 2) {
        double startX = pxToMm(pointList[i].toDouble() - bboxX);
        double startY = pxToMm(pointList[i + 1].toDouble() - bboxY);
        double endX = pxToMm(pointList[i + 2].toDouble() - bboxX);
        double endY = pxToMm(pointList[i + 3].toDouble() - bboxY);
        
        content += QString("\t(fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
            .arg(startX, 0, 'f', 2)
            .arg(startY, 0, 'f', 2)
            .arg(endX, 0, 'f', 2)
            .arg(endY, 0, 'f', 2)
            .arg(layerIdToKicad(track.layerId))
            .arg(pxToMm(track.strokeWidth), 0, 'f', 2);
    }

    return content;
}

QString ExporterFootprint::generateHole(const FootprintHole &hole, double bboxX, double bboxY) const
{
    QString content;
    double cx = pxToMm(hole.centerX - bboxX);
    double cy = pxToMm(hole.centerY - bboxY);
    double radius = pxToMm(hole.radius);

    // KiCad 5.x format
    content += QString("\t(pad \"\" thru_hole circle (at %1 %2) (size %3 %3) (drill %3) (layers *.Cu *.Mask))\n")
        .arg(cx, 0, 'f', 2)
        .arg(cy, 0, 'f', 2)
        .arg(radius * 2, 0, 'f', 2);

    return content;
}

QString ExporterFootprint::generateCircle(const FootprintCircle &circle, double bboxX, double bboxY) const
{
    QString content;
    double cx = pxToMm(circle.cx - bboxX);
    double cy = pxToMm(circle.cy - bboxY);
    double radius = pxToMm(circle.radius);

    // KiCad 5.x format
    content += QString("\t(fp_circle (center %1 %2) (end %3 %4) (layer %5) (width %6))\n")
        .arg(cx, 0, 'f', 2)
        .arg(cy, 0, 'f', 2)
        .arg(cx + radius, 0, 'f', 2)
        .arg(cy, 0, 'f', 2)
        .arg(layerIdToKicad(circle.layerId))
        .arg(pxToMm(circle.strokeWidth), 0, 'f', 2);

    return content;
}
QString ExporterFootprint::generateRectangle(const FootprintRectangle &rectangle, double bboxX, double bboxY) const
{
    QString content;
    double x = pxToMm(rectangle.x - bboxX);
    double y = pxToMm(rectangle.y - bboxY);
    double width = pxToMm(rectangle.width);
    double height = pxToMm(rectangle.height);
    QString layer = layerIdToKicad(rectangle.layerId);
    double strokeWidth = pxToMm(rectangle.strokeWidth);

    // KiCad 5.x format - 生成完整的矩形外框（4条线）
    // 上边
    content += QString("\t(fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(x + width, 0, 'f', 2)
        .arg(layer)
        .arg(strokeWidth, 0, 'f', 2);
    // 右边
    content += QString("\t(fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
        .arg(x + width, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(y + height, 0, 'f', 2)
        .arg(layer)
        .arg(strokeWidth, 0, 'f', 2);
    // 下边
    content += QString("\t(fp_line (start %1 %2) (end %3 %2) (layer %4) (width %5))\n")
        .arg(x, 0, 'f', 2)
        .arg(y + height, 0, 'f', 2)
        .arg(x + width, 0, 'f', 2)
        .arg(layer)
        .arg(strokeWidth, 0, 'f', 2);
    // 左边
    content += QString("\t(fp_line (start %1 %2) (end %1 %3) (layer %4) (width %5))\n")
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(y + height, 0, 'f', 2)
        .arg(layer)
        .arg(strokeWidth, 0, 'f', 2);

    return content;
}

QString ExporterFootprint::generateArc(const FootprintArc &arc, double bboxX, double bboxY) const
{
    QString content;
    
    // Parse path string to extract arc points (simplified - just use first and last points)
    QStringList pointList = arc.path.split(" ", Qt::SkipEmptyParts);
    
    // KiCad 5.x format (simplified - just a line for now)
    if (pointList.size() >= 4) {
        bool ok1, ok2, ok3, ok4;
        double startX = pxToMm(pointList[0].toDouble(&ok1) - bboxX);
        double startY = pxToMm(pointList[1].toDouble(&ok2) - bboxY);
        double endX = pxToMm(pointList[pointList.size() - 2].toDouble(&ok3) - bboxX);
        double endY = pxToMm(pointList[pointList.size() - 1].toDouble(&ok4) - bboxY);
        
        if (ok1 && ok2 && ok3 && ok4) {
            content += QString("\t(fp_line (start %1 %2) (end %3 %4) (layer %5) (width %6))\n")
                .arg(startX, 0, 'f', 2)
                .arg(startY, 0, 'f', 2)
                .arg(endX, 0, 'f', 2)
                .arg(endY, 0, 'f', 2)
                .arg(layerIdToKicad(arc.layerId))
                .arg(pxToMm(arc.strokeWidth), 0, 'f', 2);
        }
    }

    return content;
}

QString ExporterFootprint::generateText(const FootprintText &text, double bboxX, double bboxY) const
{
    QString content;
    double x = pxToMm(text.centerX - bboxX);
    double y = pxToMm(text.centerY - bboxY);
    
    // KiCad 5.x format
    content += QString("\t(fp_text user \"%1\" (at %2 %3 %4) (layer %5)%6\n")
        .arg(text.text)
        .arg(x, 0, 'f', 2)
        .arg(y, 0, 'f', 2)
        .arg(double(text.rotation), 0, 'f', 2)
        .arg(layerIdToKicad(text.layerId));
    
    if (!text.isDisplayed) {
        content += "\n\t\t(effects (font (size 1 1) (thickness 0.15)) (justify left))";
    } else {
        content += "\n\t\t(effects (font (size 1 1) (thickness 0.15)) (justify left))";
    }
    content += "\n\t)\n";

    return content;
}

QString ExporterFootprint::generateModel3D(const Model3DData &model3D, double bboxX, double bboxY, const QString &model3DPath) const
{
    QString content;

    // Use the provided 3D model path if available, otherwise use the model name
    QString finalPath = model3DPath.isEmpty() ? model3D.name() : model3DPath;

    // KiCad 6.x format - use absolute path (match Python version)
    content += QString("\t(model \"%1\"\n").arg(finalPath);
    content += QString("\t\t(offset (xyz %1 %2 %3))\n")
        .arg(pxToMm(model3D.translation().x - bboxX), 0, 'f', 3)
        .arg(pxToMm(model3D.translation().y - bboxY), 0, 'f', 3)
        .arg(pxToMm(model3D.translation().z), 0, 'f', 3);
    content += "\t\t(scale (xyz 1 1 1))\n";
    content += QString("\t\t(rotate (xyz %1 %2 %3))\n")
        .arg(model3D.rotation().x, 0, 'f', 0)
        .arg(model3D.rotation().y, 0, 'f', 0)
        .arg(model3D.rotation().z, 0, 'f', 0);
    content += "\t)\n";

    return content;
}
double ExporterFootprint::pxToMm(double px) const
{
    return GeometryUtils::convertToMm(px);
}

QString ExporterFootprint::padShapeToKicad(const QString &shape) const
{
    // Convert to lowercase for case-insensitive comparison
    QString shapeLower = shape.toLower();
    
    if (shapeLower == "rect" || shapeLower == "rectangle") {
        return "rect";
    } else if (shapeLower == "circle") {
        return "circle";
    } else if (shapeLower == "oval") {
        return "oval";
    } else if (shapeLower == "roundrect") {
        return "roundrect";
    } else if (shapeLower == "trapezoid") {
        return "trapezoid";
    } else {
        return "custom"; // Default to custom shape
    }
}

QString ExporterFootprint::padTypeToKicad(int layerId) const
{
    // Simplified layer mapping
    if (layerId == 1) {
        return "smd"; // Top layer
    } else if (layerId == 2) {
        return "smd"; // Bottom layer
    } else {
        return "thru_hole"; // Through hole
    }
}

QString ExporterFootprint::padLayersToKicad(int layerId) const
{
    // Layer mapping based on Python version
    switch (layerId) {
        case 1:
            return "F.Cu F.Paste F.Mask"; // Top SMD
        case 2:
            return "B.Cu B.Paste B.Mask"; // Bottom SMD
        case 3:
            return "F.SilkS";
        case 11:
            return "*.Cu *.Mask"; // Through hole - 不包含 Paste 层
        case 13:
            return "F.Fab";
        case 15:
            return "Dwgs.User";
        default:
            return "*.Cu *.Mask";
    }
}

QString ExporterFootprint::layerIdToKicad(int layerId) const
{
    // Layer mapping for graphical elements
    switch (layerId) {
        case 1:
            return "F.Cu";           // Top Layer
        case 2:
            return "B.Cu";           // Bottom Layer
        case 3:
            return "F.SilkS";        // Top Silkscreen
        case 4:
            return "B.SilkS";        // Bottom Silkscreen
        case 5:
            return "F.Paste";        // Top Solder Paste
        case 6:
            return "B.Paste";        // Bottom Solder Paste
        case 7:
            return "F.Mask";         // Top Solder Mask
        case 8:
            return "B.Mask";         // Bottom Solder Mask
        case 9:
            return "F.Cu";           // Top Layer (alternative)
        case 10:
            return "B.Cu";           // Bottom Layer (alternative)
        case 11:
            return "*.Cu";           // All Copper Layers
        case 12:
            return "B.Fab";          // Bottom Fabrication
        case 13:
            return "F.Fab";          // Top Fabrication
        case 14:
            return "Edge.Cuts";      // Board Outline
        case 15:
            return "Dwgs.User";      // Documentation
        case 20:
            return "Cmts.User";      // Comments
        case 21:
            return "Eco1.User";      // Eco1
        case 22:
            return "Eco2.User";      // Eco2
        case 100:
            return "F.Fab";          // Lead Shape Layer (引脚形状层) - 同时也会生成 F.SilkS 版本
        case 101:
            return "F.SilkS";        // Component Polarity Layer (极性标记层)
        default:
            // 默认使用 F.Fab 层，避免丢失数据
            qWarning() << "Unknown layer ID:" << layerId << ", defaulting to F.Fab";
            return "F.Fab";
    }
}

} // namespace EasyKiConverter