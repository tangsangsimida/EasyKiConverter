#include <QCoreApplication>
#include <QDebug>
#include "src/core/utils/LayerMapper.h"

using namespace EasyKiConverter;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "=== 嘉立创 EDA -> KiCad 图层映射测试 ===\n";

    // 创建图层映射器
    LayerMapper mapper;

    // 测试 1: 基本图层映射
    qDebug() << "【测试 1: 基本图层映射】";
    QMap<int, QString> testLayers = {
        {1, "TopLayer"},
        {2, "BottomLayer"},
        {3, "TopSilkLayer"},
        {4, "BottomSilkLayer"},
        {5, "TopPasteMaskLayer"},
        {6, "BottomPasteMaskLayer"},
        {7, "TopSolderMaskLayer"},
        {8, "BottomSolderMaskLayer"},
        {10, "BoardOutLine"},
        {11, "Multi-Layer"},
        {15, "Mechanical"},
        {19, "3DModel"},
        {21, "Inner1"},
        {52, "Inner32"},
        {99, "ComponentShapeLayer"},
        {101, "ComponentPolarityLayer"},
        {102, "Hole"},
        {103, "DRCError"}
    };

    for (auto it = testLayers.begin(); it != testLayers.end(); ++it) {
        int easyedaId = it.key();
        QString easyedaName = it.value();
        int kicadId = mapper.mapToKiCadLayer(easyedaId);
        QString kicadName = mapper.getKiCadLayerName(kicadId);

        qDebug() << QString("  EasyEDA %1 (%2) -> KiCad %3 (%4)")
                    .arg(easyedaId, 3)
                    .arg(easyedaName, -25)
                    .arg(kicadId, 2)
                    .arg(kicadName);
    }

    qDebug() << "\n【测试 2: 单位转换】";
    // 测试单位转换
    QList<double> testValues = {10, 20, 39.37, 78.74, 100, 200, 9.8425, 3.1496};

    for (double milValue : testValues) {
        double mmValue = LayerMapper::milToMm(milValue);
        qDebug() << QString("  %1 mil = %2 mm").arg(milValue, 8, 'f', 4).arg(mmValue, 8, 'f', 4);
    }

    qDebug() << "\n【测试 3: 图层类型判断】";
    // 测试图层类型判断
    QList<int> testLayerIds = {0, 31, 32, 34, 36, 44, 49};

    for (int layerId : testLayerIds) {
        QString layerName = mapper.getKiCadLayerName(layerId);
        QStringList types;

        if (mapper.isSignalLayer(layerId)) types << "信号层";
        if (mapper.isSilkLayer(layerId)) types << "丝印层";
        if (mapper.isMaskLayer(layerId)) types << "阻焊层";
        if (mapper.isPasteLayer(layerId)) types << "助焊层";
        if (mapper.isMechanicalLayer(layerId)) types << "机械层";

        qDebug() << QString("  %1 (%2): %3").arg(layerId, 2).arg(layerName, -15).arg(types.join(", "));
    }

    qDebug() << "\n【测试 4: 内层映射】";
    // 测试内层映射
    qDebug() << "  Inner1 (ID=21) ->" << mapper.mapToKiCadLayer(21) << mapper.getKiCadLayerName(mapper.mapToKiCadLayer(21));
    qDebug() << "  Inner16 (ID=36) ->" << mapper.mapToKiCadLayer(36) << mapper.getKiCadLayerName(mapper.mapToKiCadLayer(36));
    qDebug() << "  Inner32 (ID=52) ->" << mapper.mapToKiCadLayer(52) << mapper.getKiCadLayerName(mapper.mapToKiCadLayer(52));

    qDebug() << "\n【测试 5: 未知图层】";
    // 测试未知图层
    int unknownLayerId = 999;
    int kicadId = mapper.mapToKiCadLayer(unknownLayerId);
    qDebug() << "  Unknown layer" << unknownLayerId << "->" << kicadId << mapper.getKiCadLayerName(kicadId);

    qDebug() << "\n【测试 6: 实际封装数据示例】";
    // 模拟实际封装数据
    qDebug() << "  焊盘 1: 位置 (4000, 2980.315) mil -> ("
             << LayerMapper::milToMm(4000) << ", "
             << LayerMapper::milToMm(2980.315) << ") mm";
    qDebug() << "  焊盘直径: 9.8425 mil ->" << LayerMapper::milToMm(9.8425) << "mm";
    qDebug() << "  焊盘孔径: 3.1496 mil ->" << LayerMapper::milToMm(3.1496) << "mm";

    qDebug() << "\n【测试 7: 阻焊扩展值转换】";
    // 测试阻焊扩展值转换
    double maskExpansionMil = 0.3;
    double maskExpansionMm = LayerMapper::milToMm(maskExpansionMil);
    qDebug() << "  阻焊扩展: 0.3 mil ->" << maskExpansionMm << "mm";

    qDebug() << "\n【测试 8: 完整映射描述】";
    qDebug() << mapper.getMappingDescription();

    qDebug() << "\n=== 测试完成 ===";

    return 0;
}