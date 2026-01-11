#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// 模拟 EasyedaImporter::importSvgNodeData 函数
void testImportSvgNodeData(const QString &svgNodeData)
{
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

    // 检查是否为外形轮廓（c_etype == "outline3D"）
    if (attrs.contains("c_etype") && attrs["c_etype"].toString() == "outline3D") {
        qDebug() << "=== Testing outline3D case ===";
        
        // 这是外形轮廓，但同时也包含 3D 模型的 UUID
        // 先提取 3D 模型的 UUID
        QString modelUuid = attrs.contains("uuid") ? attrs["uuid"].toString() : "";
        QString modelName = attrs.contains("title") ? attrs["title"].toString() : "";
        
        qDebug() << "3D model info (from SVGNODE):";
        qDebug() << "  Name:" << modelName;
        qDebug() << "  UUID:" << modelUuid;
        
        // 然后解析外形轮廓
        qDebug() << "  Type: outline3D";
        qDebug() << "  Layer ID:" << (attrs.contains("layerid") ? attrs["layerid"].toString().toInt() : 19);
    } else {
        qDebug() << "=== Testing non-outline3D case ===";
        QString modelUuid = attrs.contains("uuid") ? attrs["uuid"].toString() : "";
        QString modelName = attrs.contains("title") ? attrs["title"].toString() : "";
        
        qDebug() << "3D model info:";
        qDebug() << "  Name:" << modelName;
        qDebug() << "  UUID:" << modelUuid;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "=== Testing 3D Model UUID Extraction ===";
    
    // 测试 1: outline3D 类型的 SVGNODE
    QString test1 = "SVGNODE~{\"attrs\":{\"uuid\":\"adad5b22370b40e298ad6709efc97dab\",\"title\":\"RELAY-TH_8358-012-1HP\",\"c_etype\":\"outline3D\",\"layerid\":19}}";
    qDebug() << "\nTest 1: outline3D type";
    testImportSvgNodeData(test1);
    
    // 测试 2: 非 outline3D 类型的 SVGNODE
    QString test2 = "SVGNODE~{\"attrs\":{\"uuid\":\"6564f947c51b46459fed6fd8c26d021c\",\"title\":\"TestModel\",\"layerid\":20}}";
    qDebug() << "\nTest 2: non-outline3D type";
    testImportSvgNodeData(test2);
    
    return 0;
}