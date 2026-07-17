#pragma once

/**
 * @file FootprintDataConverter.h
 * @brief FootprintData -> FootprintComponentIR 转换桥接
 *
 * 提供从旧模型 FootprintData 到 IR FootprintComponentIR 的转换函数。
 * 负责：
 * - 焊盘形状字符串 -> PadShape 枚举（EasyedaPadShapeMap）
 * - 层 ID 整数 -> LayerType 枚举（EasyedaLayerMap）
 * - 坐标字符串解析
 * - SVG 路径解析
 * - holeRadius > 0 -> PadType 枚举
 */

#include "EasyedaLayerMap.h"
#include "EasyedaPadShapeMap.h"
#include "FootprintIR.h"
#include "Model3DDataConverter.h"
#include "SymbolDataConverter.h"  // 复用 parseFlatPointString 等工具函数
#include "models/FootprintData.h"

namespace EasyKiConverter {
namespace IR {

/**
 * @brief FootprintData -> FootprintComponentIR 转换
 * @param data 旧模型的封装数据
 * @return IR 的封装组件数据
 */
inline FootprintComponentIR toFootprintIR(const FootprintData& data) {
    FootprintComponentIR ir;

    // 通用元数据
    ir.name = data.info().name;
    ir.description = data.info().description;

    // 转换焊盘
    for (const auto& pad : data.pads()) {
        FootprintPadIR pir;
        pir.number = pad.number;
        pir.position = QPointF(pad.centerX * PX_TO_MM, -pad.centerY * PX_TO_MM);
        pir.shape = EasyedaPadShapeMap::toPadShape(pad.shape);
        pir.size = QSizeF(pad.width * PX_TO_MM, pad.height * PX_TO_MM);
        pir.layer = EasyedaLayerMap::toLayerType(pad.layerId);
        pir.rotation = pad.rotation;
        pir.holeSize = pad.holeRadius * 2.0 * PX_TO_MM;  // radius -> diameter
        pir.holeLength = pad.holeLength * PX_TO_MM;
        pir.netName = pad.net;
        pir.isPlated = pad.isPlated;
        pir.isLocked = pad.isLocked;
        pir.padType = (pad.holeRadius > 0) ? PadType::ThroughHole : PadType::Smd;

        // 异形焊盘自定义形状
        if (pir.shape == PadShape::Polygon && !pad.points.isEmpty()) {
            pir.customShapePoints = parseFlatPointString(pad.points);
            for (auto& pt : pir.customShapePoints) {
                pt.setX(pt.x() - pad.centerX * PX_TO_MM);
                pt.setY(-(pt.y() - pad.centerY * PX_TO_MM));
            }
        }

        ir.pads.append(pir);
    }

    // 转换走线
    for (const auto& track : data.tracks()) {
        FootprintTrackIR tir;
        tir.points = parseFlatPointString(track.points);
        for (auto& pt : tir.points) {
            pt.setY(-pt.y());  // Y 翻转
        }
        tir.width = track.strokeWidth * PX_TO_MM;
        tir.layer = EasyedaLayerMap::toLayerType(track.layerId);
        tir.netName = track.net;
        tir.isLocked = track.isLocked;
        ir.tracks.append(tir);
    }

    // 转换安装孔
    for (const auto& hole : data.holes()) {
        FootprintHoleIR hir;
        hir.center = QPointF(hole.centerX * PX_TO_MM, -hole.centerY * PX_TO_MM);
        hir.radius = hole.radius * PX_TO_MM;
        hir.isLocked = hole.isLocked;
        ir.holes.append(hir);
    }

    // 转换圆
    for (const auto& circle : data.circles()) {
        FootprintCircleIR cir;
        cir.center = QPointF(circle.cx * PX_TO_MM, -circle.cy * PX_TO_MM);
        cir.radius = circle.radius * PX_TO_MM;
        cir.strokeWidth = circle.strokeWidth * PX_TO_MM;
        cir.layer = EasyedaLayerMap::toLayerType(circle.layerId);
        cir.isLocked = circle.isLocked;
        ir.circles.append(cir);
    }

    // 转换矩形
    for (const auto& rect : data.rectangles()) {
        FootprintRectangleIR rir;
        rir.bounds = QRectF(rect.x * PX_TO_MM,
                            -(rect.y + rect.height) * PX_TO_MM,  // Y 翻转
                            rect.width * PX_TO_MM,
                            rect.height * PX_TO_MM);
        rir.strokeWidth = rect.strokeWidth * PX_TO_MM;
        rir.layer = EasyedaLayerMap::toLayerType(rect.layerId);
        rir.isLocked = rect.isLocked;
        ir.rectangles.append(rir);
    }

    // 转换圆弧
    for (const auto& arc : data.arcs()) {
        FootprintArcIR air;
        // FootprintArc.path 是 SVG 弧线，尝试解析圆心/半径/角度
        // 简化处理：使用 parseSimpleSvgPath 提取坐标，取前 3 个点
        const QList<QPointF> arcPoints = parseSimpleSvgPath(arc.path);
        if (arcPoints.size() >= 3) {
            air.center = arcPoints[2];  // 第三个点通常是圆心
            const double dx = arcPoints[0].x() - air.center.x();
            const double dy = arcPoints[0].y() - air.center.y();
            air.radius = qSqrt(dx * dx + dy * dy);
        }
        air.width = arc.strokeWidth * PX_TO_MM;
        air.layer = EasyedaLayerMap::toLayerType(arc.layerId);
        air.netName = arc.net;
        air.isLocked = arc.isLocked;
        ir.arcs.append(air);
    }

    // 转换文本
    for (const auto& text : data.texts()) {
        FootprintTextIR tir;
        tir.text = text.text;
        tir.position = QPointF(text.centerX * PX_TO_MM, -text.centerY * PX_TO_MM);
        tir.rotation = text.rotation;
        tir.mirror = (text.mirror == "1" || text.mirror.toLower() == "true");
        tir.strokeWidth = text.strokeWidth * PX_TO_MM;
        tir.fontSize = text.fontSize * PX_TO_MM;
        tir.layer = EasyedaLayerMap::toLayerType(text.layerId);
        tir.isDisplayed = text.isDisplayed;
        tir.isFabrication = (text.type == "N");
        tir.isLocked = text.isLocked;

        // 非 ASCII 文本路径
        if (!text.textPath.isEmpty()) {
            tir.textPathPoints = parseSimpleSvgPath(text.textPath);
            for (auto& pt : tir.textPathPoints) {
                pt.setY(-pt.y());
            }
        }

        ir.texts.append(tir);
    }

    // 转换实心区域
    for (const auto& region : data.solidRegions()) {
        FootprintRegionIR rir;
        rir.vertices = parseSimpleSvgPath(region.path);
        for (auto& pt : rir.vertices) {
            pt.setY(-pt.y());
        }
        rir.layer = EasyedaLayerMap::toLayerType(region.layerId);
        rir.isKeepOut = region.isKeepOut || (region.layerId == 99);
        rir.isLocked = region.isLocked;
        ir.regions.append(rir);
    }

    // 转换板框轮廓
    for (const auto& outline : data.outlines()) {
        FootprintOutlineIR oir;
        oir.points = parseSimpleSvgPath(outline.path);
        for (auto& pt : oir.points) {
            pt.setY(-pt.y());
        }
        oir.strokeWidth = outline.strokeWidth * PX_TO_MM;
        oir.layer = EasyedaLayerMap::toLayerType(outline.layerId);
        oir.isLocked = outline.isLocked;
        ir.outlines.append(oir);
    }

    // 转换 3D 模型
    ir.models3d.append(toModel3DIR(data.model3D()));

    return ir;
}

}  // namespace IR
}  // namespace EasyKiConverter
