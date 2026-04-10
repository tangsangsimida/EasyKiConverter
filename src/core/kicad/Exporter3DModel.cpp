#include "Exporter3DModel.h"

#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTextStream>

namespace EasyKiConverter {

// API 端点
static const QString ENDPOINT_3D_MODEL = "https://modules.easyeda.com/3dmodel/%1";
static const QString ENDPOINT_3D_MODEL_STEP = "https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1";

Exporter3DModel::Exporter3DModel(QObject* parent) : QObject(parent) {}

Exporter3DModel::~Exporter3DModel() {
    cancel();
}

void Exporter3DModel::downloadObjModel(const QString& uuid, const QString& savePath) {
    if (uuid.isEmpty()) {
        QString errorMsg = "UUID is empty";
        qWarning() << errorMsg;
        emit downloadError(errorMsg);
        return;
    }

    m_currentUuid = uuid;
    m_savePath = savePath;

    QString url = getModelUrl(uuid, ModelFormat::OBJ);

    // 使用 NetworkClient 同步下载，避免跨线程问题
    RetryPolicy policy;
    policy.maxRetries = 3;
    policy.baseTimeoutMs = 60000;  // 60 秒超时

    NetworkResult result = NetworkClient::instance().get(QUrl(url), policy);

    if (result.success) {
        // 保存下载的数据到文件
        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly)) {
            QString errorMsg = QString("Failed to open file for writing: %1").arg(savePath);
            qWarning() << errorMsg;
            emit downloadError(errorMsg);
            return;
        }

        file.write(result.data);
        file.close();

        qDebug() << "Exporter3DModel: Successfully downloaded 3D model to" << savePath;
        emit downloadSuccess(savePath);
    } else {
        QString errorMsg = QString("Download failed: %1").arg(result.error);
        qWarning() << errorMsg;
        emit downloadError(errorMsg);
    }
}

void Exporter3DModel::downloadStepModel(const QString& uuid, const QString& savePath) {
    if (uuid.isEmpty()) {
        QString errorMsg = "UUID is empty";
        qWarning() << errorMsg;
        emit downloadError(errorMsg);
        return;
    }

    m_currentUuid = uuid;
    m_savePath = savePath;

    QString url = getModelUrl(uuid, ModelFormat::STEP);

    // 使用 NetworkClient 同步下载，避免跨线程问题
    RetryPolicy policy;
    policy.maxRetries = 3;
    policy.baseTimeoutMs = 60000;  // 60 秒超时

    NetworkResult result = NetworkClient::instance().get(QUrl(url), policy);

    if (result.success) {
        // 保存下载的数据到文件
        QFile file(savePath);
        if (!file.open(QIODevice::WriteOnly)) {
            QString errorMsg = QString("Failed to open file for writing: %1").arg(savePath);
            qWarning() << errorMsg;
            emit downloadError(errorMsg);
            return;
        }

        file.write(result.data);
        file.close();

        qDebug() << "Exporter3DModel: Successfully downloaded 3D model to" << savePath;
        emit downloadSuccess(savePath);
    } else {
        QString errorMsg = QString("Download failed: %1").arg(result.error);
        qWarning() << errorMsg;
        emit downloadError(errorMsg);
    }
}

bool Exporter3DModel::exportToWrl(const Model3DData& modelData, const QString& savePath) {
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << savePath;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    // 获取 OBJ 数据
    QByteArray objData = modelData.rawObj().toUtf8();

    // 生成 WRL 文件内容
    QString content = generateWrlContent(modelData, objData);

    out << content;
    file.flush();  // 确保数据被写
    file.close();

    // 验证文件大小
    QFileInfo fileInfo(savePath);

    return true;
}

bool Exporter3DModel::exportToStep(const Model3DData& modelData, const QString& savePath) {
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open file for writing:" << savePath;
        return false;
    }

    // 直接写入 STEP 二进制数
    file.write(modelData.step());
    file.close();

    return true;
}

void Exporter3DModel::convertToKiCadCoordinates(Model3DData& modelData) {
    // EasyEDA 使用右手坐标系，KiCad 使用左手坐标
    // 需要进行坐标转

    // 获取当前的平移和旋转
    Model3DBase translation = modelData.translation();
    Model3DBase rotation = modelData.rotation();

    // 旋转 X 轴 180 度
    rotation.x += 180.0;

    // Y 轴翻转
    rotation.y = -rotation.y;

    // Z 轴翻
    rotation.z = -rotation.z;

    // 更新模型数据
    modelData.setTranslation(translation);
    modelData.setRotation(rotation);
}

void Exporter3DModel::cancel() {
    // 使用同步的 NetworkClient，不需要取消请求
    // 如果需要支持取消，可以使用 wasCancelled 标志
}

QString Exporter3DModel::generateWrlContent(const Model3DData& modelData, const QByteArray& objData) {
    QString content;

    // 解析 OBJ 数据
    QJsonObject parsedData = parseObjData(objData);
    QJsonArray vertices = parsedData["vertices"].toArray();
    QJsonArray faces = parsedData["faces"].toArray();
    QJsonObject materials = parsedData["materials"].toObject();
    QJsonArray shapes = parsedData["shapes"].toArray();

    // qDebug() << "Generating WRL content - vertices:" << vertices.size()
    //          << "faces:" << faces.size()
    //          << "materials:" << materials.size()
    //          << "shapes:" << shapes.size();

    // WRL 文件头部
    content += "#VRML V2.0 utf8\n";
    content += "# 3D model generated by EasyKiConverter (https://github.com/tangsangsimida/EasyKiConverter)\n";
    content += "\n";

    // 如果有形状数据，使用形状数据生成 WRL
    if (!shapes.isEmpty()) {
        for (const QJsonValue& shapeValue : shapes) {
            QJsonObject shape = shapeValue.toObject();
            QString materialId = shape["materialId"].toString();
            QJsonArray shapePoints = shape["points"].toArray();
            QJsonArray coordIndex = shape["coordIndex"].toArray();

            // qDebug() << "Processing shape - materialId:" << materialId
            //          << "points:" << shapePoints.size()
            //          << "coordIndex:" << coordIndex.size();

            // 在倒数第二个位置插入最后一个点的副本
            if (shapePoints.size() > 0) {
                shapePoints.insert(shapePoints.size() - 1, shapePoints.last());
                // qDebug() << "Inserted duplicate of last point, new size:" << shapePoints.size();
            }

            // 获取材质信息
            QJsonObject material = materials[materialId].toObject();
            QJsonArray diffuseColor = material["diffuseColor"].toArray();
            QJsonArray specularColor = material["specularColor"].toArray();
            QString transparency = material["transparency"].toString("0");

            // 生成 Shape
            content += "Shape {\n";
            content += "  appearance Appearance {\n";
            content += "    material Material {\n";
            content += QString("      diffuseColor %1 %2 %3\n")
                           .arg(diffuseColor.isEmpty() ? "0.8" : diffuseColor[0].toString())
                           .arg(diffuseColor.isEmpty() ? "0.8" : diffuseColor[1].toString())
                           .arg(diffuseColor.isEmpty() ? "0.8" : diffuseColor[2].toString());
            content += QString("      specularColor %1 %2 %3\n")
                           .arg(specularColor.isEmpty() ? "0.2" : specularColor[0].toString())
                           .arg(specularColor.isEmpty() ? "0.2" : specularColor[1].toString())
                           .arg(specularColor.isEmpty() ? "0.2" : specularColor[2].toString());
            content += "      ambientIntensity 0.2\n";
            content += QString("      transparency %1\n").arg(transparency);
            content += "      shininess 0.5\n";
            content += "    }\n";
            content += "  }\n";
            content += "  geometry IndexedFaceSet {\n";
            content += "    ccw TRUE\n";
            content += "    solid FALSE\n";
            content += "    coord DEF co Coordinate {\n";
            content += "      point [\n";

            for (const QJsonValue& pointValue : shapePoints) {
                // pointValue 现在是字符串格式，直接输出
                content += "        " + pointValue.toString() + ",\n";
            }

            content += "      ]\n";
            content += "    }\n";
            content += "    coordIndex [\n";

            for (const QJsonValue& indexValue : coordIndex) {
                // indexValue 是一QJsonArray，包含面索引
                QJsonArray faceIndices = indexValue.toArray();
                QString indexStr;
                for (const QJsonValue& idx : faceIndices) {
                    indexStr += QString::number(idx.toInt()) + " ";
                }
                content += "        " + indexStr + "\n";
            }

            content += "    ]\n";
            content += "  }\n";
            content += "}\n";
        }
    } else {
        // qDebug() << "No shapes found, using simple Transform mode";
        // 如果没有形状数据，使用简单的 Transform 包装
        // 获取平移和旋
        Model3DBase translation = modelData.translation();
        Model3DBase rotation = modelData.rotation();

        content += "Transform {\n";
        content += "  translation ";
        content += QString("%1 %2 %3\n").arg(translation.x).arg(translation.y).arg(translation.z);
        content += "  rotation ";
        content += QString("%1 %2 %3 %4\n").arg(rotation.x).arg(rotation.y).arg(rotation.z).arg(1.0);
        content += "  scale 1 1 1\n";
        content += "  children [\n";
        content += "    Shape {\n";
        content += "      appearance Appearance {\n";
        content += "        material Material {\n";
        content += "          diffuseColor 0.8 0.8 0.8\n";
        content += "          specularColor 0.2 0.2 0.2\n";
        content += "          ambientIntensity 0.2\n";
        content += "          shininess 0.2\n";
        content += "        }\n";
        content += "      }\n";
        content += "      geometry IndexedFaceSet {\n";
        content += "        coord Coordinate {\n";
        content += "          point [\n";

        for (const QJsonValue& vertexValue : vertices) {
            QJsonArray vertex = vertexValue.toArray();
            if (vertex.size() >= 3) {
                content += QString("          %1 %2 %3\n")
                               .arg(vertex[0].toDouble())
                               .arg(vertex[1].toDouble())
                               .arg(vertex[2].toDouble());
            }
        }

        content += "          ]\n";
        content += "        }\n";
        content += "        coordIndex [\n";

        for (const QJsonValue& faceValue : faces) {
            QJsonArray face = faceValue.toArray();
            QString faceStr;
            for (const QJsonValue& indexValue : face) {
                faceStr += QString::number(indexValue.toInt() - 1) + " ";
            }
            faceStr += "-1";
            content += "          " + faceStr + "\n";
        }

        content += "        ]\n";
        content += "      }\n";
        content += "    }\n";
        content += "  ]\n";
        content += "}\n";
    }

    return content;
}

QJsonObject Exporter3DModel::parseObjData(const QByteArray& objData) {
    QJsonObject result;
    QJsonArray vertices;
    QJsonArray faces;
    QJsonObject materials;
    QJsonArray shapes;

    QString objString = QString::fromUtf8(objData);
    QStringList lines = objString.split('\n');

    // qDebug() << "=== OBJ Data Debug ===";
    // qDebug() << "OBJ data size:" << objData.size();
    // qDebug() << "OBJ string size:" << objString.size();
    // qDebug() << "Total lines:" << lines.size();
    // qDebug() << "First 500 chars:" << objString.left(500);
    // qDebug() << "First 20 lines:";
    // for (int i = 0; i < qMin(20, lines.size()); ++i) {
    //     qDebug() << "  Line" << i << ":" << lines[i].left(100);
    // }
    // qDebug() << "===================";

    // 第一遍：提取材质信息
    QString currentMaterialId;
    QJsonObject currentMaterial;
    bool inMaterial = false;

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
            continue;
        }

        QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0] == "newmtl") {
            // 保存上一个材
            if (!currentMaterialId.isEmpty() && !currentMaterial.isEmpty()) {
                materials[currentMaterialId] = currentMaterial;
            }
            // 开始新材质
            currentMaterialId = parts[1];
            currentMaterial = QJsonObject();
            inMaterial = true;
        } else if (inMaterial) {
            if (parts[0] == "endmtl") {
                // 保存材质
                if (!currentMaterialId.isEmpty() && !currentMaterial.isEmpty()) {
                    materials[currentMaterialId] = currentMaterial;
                }
                currentMaterialId.clear();
                currentMaterial = QJsonObject();
                inMaterial = false;
            } else if (parts[0] == "Ka") {
                QJsonArray ambientColor;
                for (int i = 1; i < parts.size(); ++i) {
                    ambientColor.append(parts[i]);
                }
                currentMaterial["ambientColor"] = ambientColor;
            } else if (parts[0] == "Kd") {
                QJsonArray diffuseColor;
                for (int i = 1; i < parts.size(); ++i) {
                    diffuseColor.append(parts[i]);
                }
                currentMaterial["diffuseColor"] = diffuseColor;
            } else if (parts[0] == "Ks") {
                QJsonArray specularColor;
                for (int i = 1; i < parts.size(); ++i) {
                    specularColor.append(parts[i]);
                }
                currentMaterial["specularColor"] = specularColor;
            } else if (parts[0] == "d") {
                currentMaterial["transparency"] = parts[1];
            }
        }
    }

    // 保存最后一个材
    if (!currentMaterialId.isEmpty() && !currentMaterial.isEmpty()) {
        materials[currentMaterialId] = currentMaterial;
    }

    // qDebug() << "Found" << materials.size() << "materials";

    // 第二遍：提取顶点数据（存储为字符串，用于 WRL 输出
    QStringList vertexStrings;  // 存储顶点坐标字符
    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
            continue;
        }

        QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0] == 'v') {
            // 顶点数据，转换为毫米（除以 2.54）
            if (parts.size() >= 4) {
                double x = parts[1].toDouble() / 2.54;
                double y = parts[2].toDouble() / 2.54;
                double z = parts[3].toDouble() / 2.54;
                // 保留 4 位小数，与 Python 版本保持一致
                QString vertexStr = QString("%1 %2 %3").arg(x, 0, 'f', 4).arg(y, 0, 'f', 4).arg(z, 0, 'f', 4);
                vertexStrings.append(vertexStr);

                // 同时保存JSON 数组，用于后续处
                QJsonArray vertex;
                vertex.append(x);
                vertex.append(y);
                vertex.append(z);
                vertices.append(vertex);
            }
        }
    }

    // qDebug() << "Found" << vertices.size() << "vertices";

    // 第三遍：按材质分割形状并提取面数
    QString currentShapeMaterial = "default";
    QStringList currentShapePoints;  // 使用字符串列
    QJsonArray currentShapeCoordIndex;
    QMap<int, int> vertexIndexMap;  // 映射原始顶点索引到形状中的索
    int shapeVertexCounter = 0;

    for (const QString& line : lines) {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) {
            continue;
        }

        QStringList parts = trimmedLine.split(' ', Qt::SkipEmptyParts);
        if (parts.isEmpty()) {
            continue;
        }

        if (parts[0] == "usemtl") {
            // 保存上一个形
            if (!currentShapePoints.isEmpty()) {
                QJsonObject shape;
                shape["materialId"] = currentShapeMaterial;
                shape["points"] = QJsonArray::fromStringList(currentShapePoints);
                shape["coordIndex"] = currentShapeCoordIndex;
                shapes.append(shape);
            }
            // 开始新形状
            currentShapeMaterial = parts[1];
            currentShapePoints.clear();
            currentShapeCoordIndex = QJsonArray();
            vertexIndexMap.clear();
            shapeVertexCounter = 0;
        } else if (parts[0] == 'f') {
            // 面数
            QJsonArray faceIndices;
            for (int i = 1; i < parts.size(); ++i) {
                QString vertexIndexStr = parts[i].split('/')[0];
                int vertexIndex = vertexIndexStr.toInt() - 1;  // OBJ 索引1 开

                if (!vertexIndexMap.contains(vertexIndex)) {
                    // WRL 格式使用字符串格式的顶点
                    if (vertexIndex >= 0 && vertexIndex < vertexStrings.size()) {
                        currentShapePoints.append(vertexStrings[vertexIndex]);
                    } else {
                        // 默认顶点
                        currentShapePoints.append("0 0 0");
                    }
                    vertexIndexMap[vertexIndex] = shapeVertexCounter;
                    faceIndices.append(shapeVertexCounter);
                    shapeVertexCounter++;
                } else {
                    faceIndices.append(vertexIndexMap[vertexIndex]);
                }
            }
            faceIndices.append(-1);  // WRL 格式要求每个面以 -1 结束
            currentShapeCoordIndex.append(QJsonArray(faceIndices));
        }
    }

    // 保存最后一个形
    if (!currentShapePoints.isEmpty()) {
        QJsonObject shape;
        shape["materialId"] = currentShapeMaterial;
        shape["points"] = QJsonArray::fromStringList(currentShapePoints);
        shape["coordIndex"] = currentShapeCoordIndex;
        shapes.append(shape);
    }

    // qDebug() << "Found" << shapes.size() << "shapes";

    result["vertices"] = vertices;
    result["faces"] = faces;
    result["materials"] = materials;
    result["shapes"] = shapes;

    return result;
}

QString Exporter3DModel::getModelUrl(const QString& uuid, ModelFormat format) const {
    if (format == ModelFormat::OBJ) {
        return ENDPOINT_3D_MODEL.arg(uuid);
    } else {
        return ENDPOINT_3D_MODEL_STEP.arg(uuid);
    }
}

}  // namespace EasyKiConverter
