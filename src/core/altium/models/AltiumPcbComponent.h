#pragma once

#include "AltiumCommon.h"

#include <QList>
#include <QPointF>
#include <QString>

namespace EasyKiConverter {

/**
 * @brief Altium PCB 焊盘
 */
struct AltiumPcbPad {
    QString designator;                     ///< 焊盘编号
    int32_t locationX = 0;                  ///< X 坐标（PCB 原始单位）
    int32_t locationY = 0;                  ///< Y 坐标
    int32_t sizeTopX = 0, sizeTopY = 0;    ///< 顶层尺寸
    int32_t sizeMidX = 0, sizeMidY = 0;    ///< 中间层尺寸
    int32_t sizeBotX = 0, sizeBotY = 0;    ///< 底层尺寸
    int32_t holeSize = 0;                   ///< 孔径
    uint8_t shapeTop = 1;                   ///< 顶层形状 (1=Round, 2=Rect, 3=Oct, 9=RndRect)
    uint8_t shapeMid = 1;
    uint8_t shapeBot = 1;
    double rotation = 0.0;                  ///< 旋转角度
    bool isPlated = true;                   ///< 是否电镀
    uint8_t layer = 1;                      ///< 所在层 (1=Top, 32=Bottom)
    uint16_t netIndex = 0xFFFF;             ///< 网络索引
    bool isSMD = false;                     ///< 是否表贴（无孔）
};

/**
 * @brief Altium PCB 走线
 */
struct AltiumPcbTrack {
    int32_t startX = 0, startY = 0;
    int32_t endX = 0, endY = 0;
    int32_t width = 0;
    uint8_t layer = 1;
    uint16_t netIndex = 0xFFFF;
};

/**
 * @brief Altium PCB 弧线
 */
struct AltiumPcbArc {
    int32_t centerX = 0, centerY = 0;
    int32_t radius = 0;
    double startAngle = 0.0, endAngle = 360.0;
    int32_t width = 0;
    uint8_t layer = 1;
    uint16_t netIndex = 0xFFFF;
};

/**
 * @brief Altium PCB 文本
 */
struct AltiumPcbText {
    int32_t locationX = 0, locationY = 0;
    int32_t height = 60000;                 ///< 文字高度（默认 6mil）
    int32_t strokeWidth = 10000;            ///< 笔画宽度（默认 1mil）
    double rotation = 0.0;
    bool isMirrored = false;
    uint8_t layer = 33;                     ///< 默认 Top Overlay
    QString text;
};

/**
 * @brief Altium PCB 填充
 */
struct AltiumPcbFill {
    int32_t corner1X = 0, corner1Y = 0;
    int32_t corner2X = 0, corner2Y = 0;
    double rotation = 0.0;
    uint8_t layer = 1;
    uint16_t netIndex = 0xFFFF;
};

/**
 * @brief Altium PCB 区域
 */
struct AltiumPcbRegion {
    QList<QPointF> vertices;                ///< 顶点（double 精度，PCB 原始单位）
    QList<QList<QPointF>> holes;            ///< 孔洞轮廓
    uint8_t layer = 1;
    bool isBoardCutout = false;
    int kind = 0;
};

/**
 * @brief Altium PCB 封装元件
 * @details 对应 AltiumSharp PcbComponent，包含所有 PCB 图元
 */
struct AltiumPcbComponent {
    QString name;                           ///< 封装名称
    QString description;                    ///< 封装描述
    double height = 0.0;                    ///< 元件高度（mil）

    QList<AltiumPcbPad> pads;
    QList<AltiumPcbTrack> tracks;
    QList<AltiumPcbArc> arcs;
    QList<AltiumPcbText> texts;
    QList<AltiumPcbFill> fills;
    QList<AltiumPcbRegion> regions;

    /** @brief 3D 模型信息 */
    struct Model3D {
        QString name;                       ///< 模型文件名
        QByteArray stepData;                ///< STEP 文件数据
        double rotX = 0, rotY = 0, rotZ = 0;  ///< 旋转角度
        double dz = 0;                      ///< Z 偏移（mil）
    };
    QList<Model3D> models;
};

}  // namespace EasyKiConverter
