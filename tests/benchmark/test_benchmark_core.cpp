#include "models/FootprintData.h"
#include "models/FootprintDataSerializer.h"
#include "models/SymbolData.h"
#include "models/SymbolDataSerializer.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using namespace EasyKiConverter;

class TestBenchmarkCore : public QObject {
    Q_OBJECT

private slots:

    void benchmarkSymbolSerialization() {
        // 构建复杂的 SymbolData
        SymbolData symbolData;
        SymbolInfo info;
        info.name = "BENCHMARK_SYMBOL";
        info.prefix = "U";
        symbolData.setInfo(info);

        SymbolBBox bbox;
        bbox.x = 0;
        bbox.y = 0;
        bbox.width = 100;
        bbox.height = 100;
        symbolData.setBbox(bbox);

        // 添加大量的 Pins
        QList<SymbolPin> pins;
        for (int i = 0; i < 1000; ++i) {
            SymbolPin pin;
            // Use settings for position and ID
            pin.settings.id = QString::number(i + 1);
            pin.settings.posX = 0;
            pin.settings.posY = i * 10;

            // Use name struct for text
            pin.name.text = "PIN_" + QString::number(i + 1);
            pin.name.posX = 20;
            pin.name.posY = i * 10;

            pin.settings.type = (i % 2 == 0) ? PinType::Input : PinType::Output;
            pins.append(pin);
        }
        symbolData.setPins(pins);

        // 添加几何图形
        QList<SymbolRectangle> rects;
        for (int i = 0; i < 500; ++i) {
            SymbolRectangle rect;
            rect.posX = i * 10;
            rect.posY = i * 10;
            rect.width = 50;
            rect.height = 50;
            rect.fillColor = "#FF0000";
            rects.append(rect);
        }
        symbolData.setRectangles(rects);

        QBENCHMARK {
            QJsonObject json = SymbolDataSerializer::toJson(symbolData);
            Q_UNUSED(json);
        }
    }

    void benchmarkSymbolDeserialization() {
        // 准备 JSON 数据
        SymbolData symbolData;
        // ... (构建同上，略微简化以聚焦核心性能)        // 添加大量的 Pins
        QList<SymbolPin> pins;
        for (int i = 0; i < 1000; ++i) {
            SymbolPin pin;
            pin.settings.id = QString::number(i + 1);
            pin.name.text = "PIN_" + QString::number(i + 1);
            pins.append(pin);
        }
        symbolData.setPins(pins);
        QJsonObject json = SymbolDataSerializer::toJson(symbolData);

        QBENCHMARK {
            SymbolData result;
            SymbolDataSerializer::fromJson(result, json);
        }
    }

    void benchmarkFootprintSerialization() {
        FootprintData footprintData;
        FootprintInfo info;
        info.name = "BENCHMARK_FOOTPRINT";
        footprintData.setInfo(info);

        // 添加大量焊盘
        QList<FootprintPad> pads;
        for (int i = 0; i < 1000; ++i) {
            FootprintPad pad;
            pad.number = QString::number(i + 1);
            pad.centerX = i * 2.54;
            pad.centerY = 0;
            pad.width = 1.6;
            pad.height = 1.6;
            pad.shape = "RECT";
            pads.append(pad);
        }
        footprintData.setPads(pads);

        // 添加大量丝印线段
        QList<FootprintTrack> tracks;
        for (int i = 0; i < 500; ++i) {
            FootprintTrack track;
            // EasyEDA track points format: x1 y1 x2 y2 ? or logic?
            // Assuming simplified string for benchmark content
            track.points = QString("%1 0 %2 10").arg(i).arg(i + 1);
            track.strokeWidth = 0.2;
            track.layerId = 1;
            tracks.append(track);
        }
        footprintData.setTracks(tracks);

        QBENCHMARK {
            QJsonObject json = FootprintDataSerializer::toJson(footprintData);
            Q_UNUSED(json);
        }
    }

    void benchmarkFootprintDeserialization() {
        FootprintData footprintData;
        QList<FootprintPad> pads;
        for (int i = 0; i < 1000; ++i) {
            FootprintPad pad;
            pad.number = QString::number(i + 1);
            pads.append(pad);
        }
        footprintData.setPads(pads);
        QJsonObject json = FootprintDataSerializer::toJson(footprintData);

        QBENCHMARK {
            FootprintData result;
            FootprintDataSerializer::fromJson(result, json);
        }
    }
};

QTEST_GUILESS_MAIN(TestBenchmarkCore)
#include "test_benchmark_core.moc"
