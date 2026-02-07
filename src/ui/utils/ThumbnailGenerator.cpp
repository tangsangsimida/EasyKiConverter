#include "ThumbnailGenerator.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace EasyKiConverter {

QImage ThumbnailGenerator::generateThumbnail(const QSharedPointer<ComponentData>& data, int size) {
    if (!data)
        return QImage();

    // 优先展示符号，如果没有符号则展示封装
    if (data->symbolData() && !data->symbolData()->pins().isEmpty()) {
        return generateSymbolThumbnail(data->symbolData(), size);
    } else if (data->footprintData()) {
        return generateFootprintThumbnail(data->footprintData(), size);
    }

    return QImage();
}

QImage ThumbnailGenerator::generateSymbolThumbnail(const QSharedPointer<SymbolData>& symbolData, int size) {
    if (!symbolData)
        return QImage();

    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);

    // 计算边框
    QRectF bbox = calculateSymbolBoundingBox(symbolData);

    // 如果没有有效内容，返回空图片
    if (bbox.isNull() || bbox.width() <= 0 || bbox.height() <= 0) {
        return image;
    }

    // 计算缩放比例，保留 10% 边距
    double margin = size * 0.1;
    double scaleX = (size - 2 * margin) / bbox.width();
    double scaleY = (size - 2 * margin) / bbox.height();
    double scale = qMin(scaleX, scaleY);

    // 居中偏移
    double offsetX = (size - bbox.width() * scale) / 2 - bbox.x() * scale;
    double offsetY = (size - bbox.height() * scale) / 2 - bbox.y() * scale;

    painter.translate(offsetX, offsetY);
    painter.scale(scale, scale);

    drawSymbol(painter, symbolData, QRect(0, 0, size, size));  // rect 参数其实在这里没用到，因为已经缩放了

    return image;
}

QImage ThumbnailGenerator::generateFootprintThumbnail(const QSharedPointer<FootprintData>& footprintData, int size) {
    // 暂时简单实现，如果有需求可以进一步完善
    if (!footprintData)
        return QImage();

    QImage image(size, size, QImage::Format_ARGB32);
    image.fill(Qt::black);  // PCB 背景色通常是深色

    // ... 类似符号的绘制逻辑，但针对封装
    // 简化起见，先只绘制符号
    return image;
}

void ThumbnailGenerator::drawSymbol(QPainter& painter,
                                    const QSharedPointer<SymbolData>& symbolData,
                                    const QRect& targetRect) {
    Q_UNUSED(targetRect);

    // 绘制样式
    QPen pinPen(Qt::darkBlue, 1);  // 考虑到缩放，线宽可能需要调整，但在 transform 下 1 代表逻辑单位
    QPen bodyPen(Qt::darkRed, 2);
    QBrush bodyBrush(Qt::NoBrush);

    // 绘制矩形
    painter.setPen(bodyPen);
    painter.setBrush(bodyBrush);
    for (const auto& rect : symbolData->rectangles()) {
        painter.drawRect(QRectF(rect.posX, rect.posY, rect.width, rect.height));
    }

    // 绘制多边形
    for (const auto& poly : symbolData->polygons()) {
        QStringList pointsStr = poly.points.split(' ');
        QPolygonF polygon;
        for (int i = 0; i < pointsStr.size(); i += 2) {
            if (i + 1 < pointsStr.size()) {
                polygon.append(QPointF(pointsStr[i].toDouble(), pointsStr[i + 1].toDouble()));
            }
        }
        painter.drawPolygon(polygon);
    }

    // 绘制多段线
    for (const auto& poly : symbolData->polylines()) {
        QStringList pointsStr = poly.points.split(' ');
        QPolygonF polygon;
        for (int i = 0; i < pointsStr.size(); i += 2) {
            if (i + 1 < pointsStr.size()) {
                polygon.append(QPointF(pointsStr[i].toDouble(), pointsStr[i + 1].toDouble()));
            }
        }
        painter.drawPolyline(polygon);
    }

    // 绘制引脚
    painter.setPen(pinPen);
    for (const auto& pin : symbolData->pins()) {
        if (!pin.settings.isDisplayed)
            continue;

        QPointF pos(pin.settings.posX, pin.settings.posY);

        // 简单绘制一个小圆圈表示引脚位置
        painter.drawEllipse(pos, 2, 2);

        // 如果有引脚路径，解析并绘制
        // 这里简化处理，通常引脚是一条线
        // 根据旋转角度绘制一条短线
        double length = 10.0;  // 假设长度
        double angle = pin.settings.rotation * M_PI / 180.0;
        QPointF endPos = pos + QPointF(length * cos(angle), length * sin(angle));
        painter.drawLine(pos, endPos);
    }
}

void ThumbnailGenerator::drawFootprint(QPainter& painter,
                                       const QSharedPointer<FootprintData>& footprintData,
                                       const QRect& targetRect) {
    // TODO: 实现封装绘制
}

QRectF ThumbnailGenerator::calculateSymbolBoundingBox(const QSharedPointer<SymbolData>& symbolData) {
    QRectF bbox;
    bool first = true;

    auto updateBBox = [&](const QRectF& rect) {
        if (first) {
            bbox = rect;
            first = false;
        } else {
            bbox = bbox.united(rect);
        }
    };

    for (const auto& rect : symbolData->rectangles()) {
        updateBBox(QRectF(rect.posX, rect.posY, rect.width, rect.height));
    }

    for (const auto& pin : symbolData->pins()) {
        updateBBox(QRectF(pin.settings.posX - 5, pin.settings.posY - 5, 10, 10));  // 估算引脚范围
    }

    for (const auto& poly : symbolData->polygons()) {
        QStringList pointsStr = poly.points.split(' ');
        for (int i = 0; i < pointsStr.size(); i += 2) {
            if (i + 1 < pointsStr.size()) {
                updateBBox(QRectF(pointsStr[i].toDouble(), pointsStr[i + 1].toDouble(), 1, 1));
            }
        }
    }

    // 如果 bbox 仍然为空（例如只有引脚），给一个默认值
    if (bbox.isNull()) {
        return QRectF(0, 0, 100, 100);
    }

    return bbox;
}

QRectF ThumbnailGenerator::calculateFootprintBoundingBox(const QSharedPointer<FootprintData>& footprintData) {
    return QRectF();
}

}  // namespace EasyKiConverter
