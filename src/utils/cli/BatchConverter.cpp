#include "BatchConverter.h"

#include "CliContext.h"
#include "FileReader.h"

#include <QCoreApplication>

namespace EasyKiConverter {

/** @brief 创建批量转换器并绑定 CLI 上下文。 */
BatchConverter::BatchConverter(CliContext* context, QObject* parent) : BaseConverter(context, parent) {}

/** @brief 读取元器件列表、预加载组件并执行导出流程。 */
bool BatchConverter::execute() {
    printMessage(QCoreApplication::translate("CliConverter", "开始批量转换..."));

    // 读取元器件列表文件
    QString readError;
    QStringList componentIds = FileReader::readComponentListFile(context()->parser().inputFile(), readError);

    if (!readError.isEmpty()) {
        setError(readError);
        return false;
    }

    if (componentIds.isEmpty()) {
        setError(QCoreApplication::translate("CliConverter", "元器件列表文件为空"));
        return false;
    }

    return runExport(componentIds);
}

}  // namespace EasyKiConverter
