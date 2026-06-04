<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>CliConverter</name>
    <message>
        <location filename="../../src/utils/cli/FileReader.cpp" line="22"/>
        <location filename="../../src/utils/cli/FileReader.cpp" line="41"/>
        <source>输入文件不存在: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="29"/>
        <location filename="../../src/utils/cli/FileReader.cpp" line="31"/>
        <source>BOM 表中没有找到有效的元器件编号</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/FileReader.cpp" line="47"/>
        <source>无法打开输入文件: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="17"/>
        <source>开始转换 BOM 表...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="32"/>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="33"/>
        <source>找到 %1 个元器件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="61"/>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="62"/>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="59"/>
        <source>预加载完成，开始导出...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="75"/>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="76"/>
        <source>
转换完成: 成功 %1, 失败 %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="84"/>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="85"/>
        <source>预加载完成: 成功 %1, 失败 %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="16"/>
        <source>开始转换单个元器件...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="20"/>
        <source>未指定元器件编号</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="25"/>
        <source>元器件编号格式无效: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="29"/>
        <source>元器件编号: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="73"/>
        <source>转换完成: 成功 %1, 失败 %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="16"/>
        <source>开始批量转换...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="28"/>
        <source>元器件列表文件为空</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/CliConverter.cpp" line="31"/>
        <source>未知的 CLI 模式</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/CliConverter.cpp" line="45"/>
        <source>未知错误</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CommandLineParser</name>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="306"/>
        <source>无效的日志级别: %1（有效值: %2）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="317"/>
        <source>无效的语言设置: %1（有效值: %2）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="328"/>
        <source>无效的主题设置: %1（有效值: %2）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="336"/>
        <source>缓存目录不能为空</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="344"/>
        <source>磁盘缓存大小必须是大于 0 的整数（单位: MB）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="353"/>
        <source>无效的 3D 模型格式: %1（有效值: %2）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="363"/>
        <source>无效的 3D 模型路径模式: %1（有效值: %2）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="369"/>
        <source>CLI 模式必须指定输出目录 (-o/--output)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="373"/>
        <source>BOM 表转换必须指定输入文件 (-i/--input)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="378"/>
        <source>单个元器件转换必须指定 LCSC 编号 (-c/--component)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="382"/>
        <source>批量转换必须指定输入文件 (-i/--input)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="479"/>
        <source>EasyKiConverter CLI 模式</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="480"/>
        <source>用法:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="481"/>
        <source>&lt;子命令&gt; [选项]</source>
        <translation>&lt;子命令&gt; [选项]</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="483"/>
        <source>子命令:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="484"/>
        <source>转换 BOM 表文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="485"/>
        <source>转换单个元器件（通过 LCSC 编号）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="488"/>
        <source>批量转换元器件（通过元器件列表文件）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="489"/>
        <source>选项:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="491"/>
        <source>输入文件路径（BOM 表或元器件列表文件）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="492"/>
        <source>输出目录路径（必需）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="495"/>
        <source>导出库名称（默认: EasyKiConverter）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="496"/>
        <source>LCSC 元器件编号</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="499"/>
        <source>导出符号库（默认: true）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="501"/>
        <source>导出封装库（默认: true）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="502"/>
        <source>导出 3D 模型</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="504"/>
        <source>3D 模型格式（wrl/step/both，默认: wrl）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="505"/>
        <source>导出数据手册</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="506"/>
        <source>导出预览图</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="507"/>
        <source>设置磁盘缓存目录</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="510"/>
        <source>设置磁盘缓存大小限制 (MB)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="511"/>
        <source>显示进度条</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="512"/>
        <source>安静模式，减少输出</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="515"/>
        <source>启用弱网模式（超时翻倍、增加重试、降低并发）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="517"/>
        <source>更新模式（仅导出缺失或已更改的文件）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="519"/>
        <source>3D 模型路径模式（relative/absolute，默认: relative）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="522"/>
        <source>不覆盖已存在的文件（默认: 覆盖）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="523"/>
        <source>符号库描述文本</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="525"/>
        <source>封装库描述文本</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="527"/>
        <source>示例:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="528"/>
        <source>转换 BOM 表</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="530"/>
        <source>转换单个元器件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="532"/>
        <source>批量转换</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ComponentListCard</name>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="702"/>
        <source>全部 (%1)</source>
        <translation>全部 (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="723"/>
        <source>验证中 (%1)</source>
        <translation>验证中 (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="748"/>
        <source>有效 (%1)</source>
        <translation>有效 (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="773"/>
        <source>无效 (%1)</source>
        <translation>无效 (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="557"/>
        <source>无预览图</source>
        <translation>无预览图</translation>
    </message>
</context>
<context>
    <name>ComponentListItem</name>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="187"/>
        <source>已复制</source>
        <translation>已复制</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="548"/>
        <source>编辑元器件描述</source>
        <translation>编辑元器件描述</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="492"/>
        <source>正在导出...</source>
        <translation>正在导出...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="494"/>
        <source>导出完成</source>
        <translation>导出完成</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="496"/>
        <source>导出失败</source>
        <translation>导出失败</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="501"/>
        <source>正在验证 CAD 数据...</source>
        <translation>正在验证 CAD 数据...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="503"/>
        <source>正在获取预览图...</source>
        <translation>正在获取预览图...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="505"/>
        <source>验证失败</source>
        <translation>验证失败</translation>
    </message>
</context>
<context>
    <name>ComponentListViewModel</name>
    <message>
        <source>元器件不存在（404）</source>
        <translation type="vanished">元器件不存在（404）</translation>
    </message>
    <message>
        <source>预览图获取超时（网络不稳定）</source>
        <translation type="vanished">预览图获取超时（网络不稳定）</translation>
    </message>
    <message>
        <source>预览图不存在</source>
        <translation type="vanished">预览图不存在</translation>
    </message>
    <message>
        <source>预览图获取被拒绝</source>
        <translation type="vanished">预览图获取被拒绝</translation>
    </message>
    <message>
        <source>预览图获取失败</source>
        <translation type="vanished">预览图获取失败</translation>
    </message>
</context>
<context>
    <name>ConfirmDialog</name>
    <message>
        <location filename="../../src/ui/qml/components/ConfirmDialog.qml" line="30"/>
        <source>退出确认</source>
        <translation>退出确认</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ConfirmDialog.qml" line="31"/>
        <source>转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？</source>
        <translation>转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ConfirmDialog.qml" line="33"/>
        <source>确定</source>
        <translation>确定</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ConfirmDialog.qml" line="34"/>
        <source>取消</source>
        <translation>取消</translation>
    </message>
</context>
<context>
    <name>DescriptionEditDialog</name>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="14"/>
        <source>元器件描述</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="15"/>
        <source>编辑当前元器件导出到符号和封装中的描述。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="30"/>
        <source>留空则不导出描述</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="44"/>
        <source>取消</source>
        <translation type="unfinished">取消</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="55"/>
        <source>保存</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>EasyKiConverter::ComponentListViewModel</name>
    <message>
        <location filename="../../src/ui/viewmodels/ComponentListViewModel.cpp" line="890"/>
        <source>元器件不存在（404）</source>
        <translation type="unfinished">元器件不存在（404）</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/ComponentListViewModel.cpp" line="904"/>
        <source>预览图获取超时（网络不稳定）</source>
        <translation type="unfinished">预览图获取超时（网络不稳定）</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/ComponentListViewModel.cpp" line="907"/>
        <source>预览图不存在</source>
        <translation type="unfinished">预览图不存在</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/ComponentListViewModel.cpp" line="909"/>
        <source>预览图获取被拒绝</source>
        <translation type="unfinished">预览图获取被拒绝</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/ComponentListViewModel.cpp" line="911"/>
        <source>预览图获取失败</source>
        <translation type="unfinished">预览图获取失败</translation>
    </message>
</context>
<context>
    <name>EasyKiConverter::SystemTrayManager</name>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="73"/>
        <source>EasyKiConverter - LCSC 转换工具</source>
        <translation type="unfinished">EasyKiConverter - LCSC 转换工具</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="94"/>
        <source>显示窗口</source>
        <translation type="unfinished">显示窗口</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="101"/>
        <source>退出</source>
        <translation type="unfinished">退出</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="141"/>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="163"/>
        <source>导出完成</source>
        <translation type="unfinished">导出完成</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="150"/>
        <source>导出失败：%1 个元器件全部失败</source>
        <translation type="unfinished">导出失败：%1 个元器件全部失败</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="153"/>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="155"/>
        <source>成功 %1 个，失败 %2 个</source>
        <translation type="unfinished">成功 %1 个，失败 %2 个</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="159"/>
        <source>
输出：符号 %1 · 封装 %2 · 3D %3</source>
        <translation type="unfinished">
输出：符号 %1 · 封装 %2 · 3D %3</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="166"/>
        <source>成功导出 1 个元器件</source>
        <translation type="unfinished">成功导出 1 个元器件</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="168"/>
        <source>成功导出 %1 个元器件</source>
        <translation type="unfinished">成功导出 %1 个元器件</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="172"/>
        <source>输出：符号 %1 · 封装 %2 · 3D %3</source>
        <translation type="unfinished">输出：符号 %1 · 封装 %2 · 3D %3</translation>
    </message>
    <message>
        <location filename="../../src/ui/viewmodels/SystemTrayManager.cpp" line="175"/>
        <source>耗时：%1</source>
        <translation type="unfinished">耗时：%1</translation>
    </message>
</context>
<context>
    <name>ExitDialog</name>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="68"/>
        <source>关闭程序</source>
        <translation>关闭程序</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="69"/>
        <source>您可以选择最小化到系统托盘以保持后台运行，或者完全退出程序。</source>
        <translation>您可以选择最小化到系统托盘以保持后台运行，或者完全退出程序。</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="39"/>
        <source>最小化到托盘</source>
        <translation>最小化到托盘</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="49"/>
        <source>退出程序</source>
        <translation>退出程序</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="59"/>
        <source>取消</source>
        <translation>取消</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="118"/>
        <source>记住我的选择</source>
        <translation>记住我的选择</translation>
    </message>
</context>
<context>
    <name>ExportResultsCard</name>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="110"/>
        <source>全部 (%1)</source>
        <translation>全部 (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="138"/>
        <source>导出中 (%1)</source>
        <translation>导出中 (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="166"/>
        <source>成功 (%1)</source>
        <translation>成功 (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="194"/>
        <source>失败 (%1)</source>
        <translation>失败 (%1)</translation>
    </message>
</context>
<context>
    <name>LanguageManager</name>
    <message>
        <source>语言设置</source>
        <translation type="vanished">语言设置</translation>
    </message>
    <message>
        <source>跟随系统</source>
        <translation type="vanished">跟随系统</translation>
    </message>
    <message>
        <source>简体中文</source>
        <translation type="vanished">简体中文</translation>
    </message>
    <message>
        <source>English</source>
        <translation type="vanished">English</translation>
    </message>
    <message>
        <source>语言切换将在重启后完全生效</source>
        <translation type="vanished">语言切换将在重启后完全生效</translation>
    </message>
    <message>
        <source>未选择文件</source>
        <translation type="vanished">未选择文件</translation>
    </message>
    <message>
        <source>3D模型</source>
        <translation type="vanished">3D模型</translation>
    </message>
    <message>
        <source>数据抓取</source>
        <translation type="vanished">数据抓取</translation>
    </message>
    <message>
        <source>数据处理</source>
        <translation type="vanished">数据处理</translation>
    </message>
    <message>
        <source>文件写入</source>
        <translation type="vanished">文件写入</translation>
    </message>
    <message>
        <source>总数</source>
        <translation type="vanished">总数</translation>
    </message>
    <message>
        <source>成功</source>
        <translation type="vanished">成功</translation>
    </message>
    <message>
        <source>失败</source>
        <translation type="vanished">失败</translation>
    </message>
    <message>
        <source>成功率</source>
        <translation type="vanished">成功率</translation>
    </message>
    <message>
        <source>总耗时</source>
        <translation type="vanished">总耗时</translation>
    </message>
    <message>
        <source>平均抓取</source>
        <translation type="vanished">平均抓取</translation>
    </message>
    <message>
        <source>平均处理</source>
        <translation type="vanished">平均处理</translation>
    </message>
    <message>
        <source>平均写入</source>
        <translation type="vanished">平均写入</translation>
    </message>
    <message>
        <source>符号</source>
        <translation type="vanished">符号</translation>
    </message>
    <message>
        <source>封装</source>
        <translation type="vanished">封装</translation>
    </message>
    <message>
        <source>总请求数</source>
        <translation type="vanished">总请求数</translation>
    </message>
    <message>
        <source>重试次数</source>
        <translation type="vanished">重试次数</translation>
    </message>
    <message>
        <source>平均延迟</source>
        <translation type="vanished">平均延迟</translation>
    </message>
    <message>
        <source>速率限制</source>
        <translation type="vanished">速率限制</translation>
    </message>
    <message>
        <source>正在转换...</source>
        <translation type="vanished">正在转换...</translation>
    </message>
    <message>
        <source>开始转换</source>
        <translation type="vanished">开始转换</translation>
    </message>
    <message>
        <source>正在停止...</source>
        <translation type="vanished">正在停止...</translation>
    </message>
    <message>
        <source>停止转换</source>
        <translation type="vanished">停止转换</translation>
    </message>
</context>
<context>
    <name>Main</name>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="21"/>
        <source>EasyKiConverter - 元器件转换工具</source>
        <translation>EasyKiConverter - 元器件转换工具</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="37"/>
        <source>确认退出</source>
        <translation>确认退出</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="38"/>
        <source>转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？</source>
        <translation>转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="39"/>
        <source>强制退出</source>
        <translation>强制退出</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="40"/>
        <source>继续转换</source>
        <translation>继续转换</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="164"/>
        <source>选择 BOM 文件</source>
        <translation>选择 BOM 文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="147"/>
        <location filename="../../src/ui/qml/MainWindow.qml" line="181"/>
        <source>选择缓存目录</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/HeaderSection.qml" line="26"/>
        <source>将嘉立创EDA元器件转换为KiCad格式</source>
        <translation>将嘉立创EDA元器件转换为KiCad格式</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentInputCard.qml" line="10"/>
        <source>添加元器件</source>
        <translation>添加元器件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentInputCard.qml" line="18"/>
        <location filename="../../src/ui/qml/MainWindow.qml" line="444"/>
        <source>输入LCSC元件编号 (例如: C2040)</source>
        <translation>输入LCSC元件编号 (例如: C2040)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentInputCard.qml" line="52"/>
        <location filename="../../src/ui/qml/MainWindow.qml" line="478"/>
        <source>添加</source>
        <translation>添加</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentInputCard.qml" line="63"/>
        <location filename="../../src/ui/qml/MainWindow.qml" line="490"/>
        <source>粘贴</source>
        <translation>粘贴</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="14"/>
        <source>导入BOM文件</source>
        <translation>导入BOM文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="19"/>
        <source>选择BOM文件</source>
        <translation>选择BOM文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="31"/>
        <source>未选择文件</source>
        <translation>未选择文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="44"/>
        <source>客户端弱网络适配</source>
        <translation>客户端弱网络适配</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="53"/>
        <source>仅影响本机网络请求策略，不代表服务器限流；开启后会使用更保守的并发、超时和重试配置</source>
        <translation>仅影响本机网络请求策略，不代表服务器限流；开启后会使用更保守的并发、超时和重试配置</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="95"/>
        <source>导入 BOM 前可先开启，验证和预览图加载也会使用该策略</source>
        <translation>导入 BOM 前可先开启，验证和预览图加载也会使用该策略</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="22"/>
        <source>元器件列表</source>
        <translation>元器件列表</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="628"/>
        <source>共 %1 个元器件</source>
        <translation>共 %1 个元器件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="799"/>
        <source>搜索元器件...</source>
        <translation>搜索元器件...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="871"/>
        <source>重试所有</source>
        <translation>重试所有</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="920"/>
        <source>复制所有编号</source>
        <translation>复制所有编号</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="945"/>
        <source>已复制所有编号</source>
        <translation>已复制所有编号</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="952"/>
        <source>清空列表</source>
        <translation>清空列表</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="11"/>
        <location filename="../../src/ui/qml/components/SidebarPanel.qml" line="143"/>
        <source>导出设置</source>
        <translation>导出设置</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="34"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="20"/>
        <source>输出路径</source>
        <translation>输出路径</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="53"/>
        <location filename="../../src/ui/qml/MainWindow.qml" line="173"/>
        <source>选择输出目录</source>
        <translation>选择输出目录</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="65"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="159"/>
        <source>浏览</source>
        <translation>浏览</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="82"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="32"/>
        <source>库名称</source>
        <translation>库名称</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="98"/>
        <source>输入库名称 (例如: MyLibrary)</source>
        <translation>输入库名称 (例如: MyLibrary)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="20"/>
        <source>基础配置</source>
        <translation>基础配置</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="115"/>
        <source>缓存配置</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="128"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="42"/>
        <source>缓存目录</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="175"/>
        <source>磁盘缓存上限 (MB)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="230"/>
        <source>库描述</source>
        <translation>库描述</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="243"/>
        <source>符号库描述 (sym-lib-table)</source>
        <translation>符号库描述 (sym-lib-table)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="258"/>
        <source>输入符号库描述</source>
        <translation>输入符号库描述</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="317"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="58"/>
        <source>符号库</source>
        <translation>符号库</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="326"/>
        <source>导出符号库文件</source>
        <translation>导出符号库文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="276"/>
        <source>封装库描述 (fp-lib-table)</source>
        <translation>封装库描述 (fp-lib-table)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="291"/>
        <source>输入封装库描述</source>
        <translation>输入封装库描述</translation>
    </message>
    <message>
        <source>关键词</source>
        <translation type="vanished">关键词</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="356"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="67"/>
        <source>封装库</source>
        <translation>封装库</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="308"/>
        <source>导出选项</source>
        <translation>导出选项</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="638"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="397"/>
        <source>导出模式</source>
        <translation>导出模式</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="656"/>
        <source>追加模式</source>
        <translation>追加模式</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="715"/>
        <source>更新模式</source>
        <translation>更新模式</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="365"/>
        <source>导出封装库文件</source>
        <translation>导出封装库文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="397"/>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="97"/>
        <source>3D模型</source>
        <translation>3D模型</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="406"/>
        <source>导出 3D 模型文件</source>
        <translation>导出 3D 模型文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="559"/>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="106"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="371"/>
        <source>预览图</source>
        <translation>预览图</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="568"/>
        <source>导出预览图文件</source>
        <translation>导出预览图文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="598"/>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="111"/>
        <source>手册</source>
        <translation>手册</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="607"/>
        <source>导出数据手册文件</source>
        <translation>导出数据手册文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="524"/>
        <source>相对(推荐)</source>
        <translation>相对(推荐)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="548"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="334"/>
        <source>绝对</source>
        <translation>绝对</translation>
    </message>
    <message>
        <source>调试模式</source>
        <translation type="vanished">调试模式</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="430"/>
        <source>追加</source>
        <translation>追加</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="665"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="694"/>
        <source>保留已经存在的元器件数据，并追加新的元器件</source>
        <translation>保留已经存在的元器件数据，并追加新的元器件</translation>
    </message>
    <message>
        <source>更新</source>
        <translation type="vanished">更新</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="724"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="753"/>
        <source>覆盖已经存在的元器件数据，并追加新的元器件</source>
        <translation>覆盖已经存在的元器件数据，并追加新的元器件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportProgressCard.qml" line="11"/>
        <source>转换进度</source>
        <translation>转换进度</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportProgressCard.qml" line="26"/>
        <source>数据抓取</source>
        <translation>数据抓取</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportProgressCard.qml" line="49"/>
        <source>数据处理</source>
        <translation>数据处理</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportProgressCard.qml" line="72"/>
        <source>文件写入</source>
        <translation>文件写入</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="15"/>
        <source>转换结果</source>
        <translation>转换结果</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="79"/>
        <location filename="../../src/ui/qml/MainWindow.qml" line="748"/>
        <source>正在转换...</source>
        <translation>正在转换...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="81"/>
        <source>重试失败项</source>
        <translation>重试失败项</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="82"/>
        <source>开始转换</source>
        <translation>开始转换</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="144"/>
        <source>正在停止...</source>
        <translation>正在停止...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="144"/>
        <source>停止转换</source>
        <translation>停止转换</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="11"/>
        <source>导出统计</source>
        <translation>导出统计</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="18"/>
        <source>基本统计</source>
        <translation>基本统计</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="27"/>
        <source>总数</source>
        <translation>总数</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="32"/>
        <source>成功</source>
        <translation>成功</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="38"/>
        <source>失败</source>
        <translation>失败</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="44"/>
        <source>成功率</source>
        <translation>成功率</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="60"/>
        <source>抓取进度</source>
        <translation>抓取进度</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="65"/>
        <source>处理进度</source>
        <translation>处理进度</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="70"/>
        <source>写入进度</source>
        <translation>写入进度</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="77"/>
        <source>导出详情</source>
        <translation>导出详情</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="126"/>
        <source>打开输出目录</source>
        <translation>打开输出目录</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="140"/>
        <source>清空缓存</source>
        <translation>清空缓存</translation>
    </message>
    <message>
        <source>时间统计</source>
        <translation type="vanished">时间统计</translation>
    </message>
    <message>
        <source>总耗时</source>
        <translation type="vanished">总耗时</translation>
    </message>
    <message>
        <source>平均抓取</source>
        <translation type="vanished">平均抓取</translation>
    </message>
    <message>
        <source>平均处理</source>
        <translation type="vanished">平均处理</translation>
    </message>
    <message>
        <source>平均写入</source>
        <translation type="vanished">平均写入</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="87"/>
        <source>符号</source>
        <translation>符号</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="92"/>
        <source>封装</source>
        <translation>封装</translation>
    </message>
    <message>
        <source>网络统计</source>
        <translation type="vanished">网络统计</translation>
    </message>
    <message>
        <source>总请求数</source>
        <translation type="vanished">总请求数</translation>
    </message>
    <message>
        <source>重试次数</source>
        <translation type="vanished">重试次数</translation>
    </message>
    <message>
        <source>平均延迟</source>
        <translation type="vanished">平均延迟</translation>
    </message>
    <message>
        <source>速率限制</source>
        <translation type="vanished">速率限制</translation>
    </message>
    <message>
        <source>峰值内存</source>
        <translation type="vanished">峰值内存</translation>
    </message>
    <message>
        <source>打开详细统计报告</source>
        <translation type="vanished">打开详细统计报告</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="153"/>
        <source>打开缓存目录</source>
        <translation>打开缓存目录</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="49"/>
        <location filename="../../src/ui/qml/components/SidebarExportControls.qml" line="87"/>
        <source>打开导出目录</source>
        <translation>打开导出目录</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="21"/>
        <source>错误</source>
        <translation>错误</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="24"/>
        <source>打开导出目录失败，请检查导出路径是否存在。</source>
        <translation>打开导出目录失败，请检查导出路径是否存在。</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="45"/>
        <source>发现新版本 %1</source>
        <translation>发现新版本 %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="52"/>
        <source>当前版本 %1，最新发布：%2</source>
        <translation>当前版本 %1，最新发布：%2</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="52"/>
        <source>当前版本 %1，可前往 GitHub 查看发布说明。</source>
        <translation>当前版本 %1，可前往 GitHub 查看发布说明。</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="63"/>
        <source>查看更新</source>
        <translation>查看更新</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="72"/>
        <source>稍后提醒</source>
        <translation>稍后提醒</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="347"/>
        <source>元器件添加方式</source>
        <translation>元器件添加方式</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="379"/>
        <source>手动添加元器件</source>
        <translation>手动添加元器件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="379"/>
        <source>通过BOM表导入元器件</source>
        <translation>通过BOM表导入元器件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="588"/>
        <source>点击选择 BOM 文件</source>
        <translation>点击选择 BOM 文件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="603"/>
        <source>文件已就绪</source>
        <translation>文件已就绪</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="604"/>
        <source>支持格式: .xlsx, .csv, .txt</source>
        <translation>支持格式: .xlsx, .csv, .txt</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="749"/>
        <source>转换完成</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="18"/>
        <source>输出配置</source>
        <translation>输出配置</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="302"/>
        <source>路径</source>
        <translation>路径</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="22"/>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="44"/>
        <source>选择目录...</source>
        <translation>选择目录...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="483"/>
        <source>符号库描述</source>
        <translation>符号库描述</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="492"/>
        <source>封装库描述</source>
        <translation>封装库描述</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="480"/>
        <source>库信息 (可选)</source>
        <translation>库信息 (可选)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="56"/>
        <source>导出内容</source>
        <translation>导出内容</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="81"/>
        <source>3D 模型</source>
        <translation>3D 模型</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="380"/>
        <source>数据手册</source>
        <translation>数据手册</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="206"/>
        <source>格式</source>
        <translation>格式</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="334"/>
        <source>相对</source>
        <translation>相对</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="430"/>
        <source>覆盖</source>
        <translation>覆盖</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="461"/>
        <source>保留已有元器件，追加新的</source>
        <translation>保留已有元器件，追加新的</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="461"/>
        <source>覆盖已有元器件，追加新的</source>
        <translation>覆盖已有元器件，追加新的</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="391"/>
        <source>运行策略</source>
        <translation>运行策略</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarSettingsView.qml" line="469"/>
        <source>弱网模式</source>
        <translation>弱网模式</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarPanel.qml" line="109"/>
        <source>展开设置</source>
        <translation>展开设置</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarPanel.qml" line="173"/>
        <source>收起设置</source>
        <translation>收起设置</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarExportControls.qml" line="60"/>
        <source>请先添加元器件</source>
        <translation>请先添加元器件</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarExportControls.qml" line="64"/>
        <source>请至少选择一种导出类型</source>
        <translation>请至少选择一种导出类型</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarExportControls.qml" line="32"/>
        <source>开始导出</source>
        <translation>开始导出</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarExportControls.qml" line="58"/>
        <source>正在导出中...</source>
        <translation>正在导出中...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarExportControls.qml" line="29"/>
        <source>导出中</source>
        <translation>导出中</translation>
    </message>
    <message>
        <source>转换中</source>
        <translation type="vanished">转换中</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="797"/>
        <source>停止</source>
        <translation>停止</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="797"/>
        <source>停止中</source>
        <translation>停止中</translation>
    </message>
    <message>
        <source>重试</source>
        <translation type="vanished">重试</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SidebarExportControls.qml" line="31"/>
        <source>重试失败</source>
        <translation>重试失败</translation>
    </message>
    <message>
        <source>就绪</source>
        <translation type="vanished">就绪</translation>
    </message>
    <message>
        <source>有 %1 项失败</source>
        <translation type="vanished">有 %1 项失败</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="780"/>
        <source>打开目录</source>
        <translation>打开目录</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>导出完成</source>
        <translation type="vanished">导出完成</translation>
    </message>
    <message>
        <source>导出失败：%1 个元器件全部失败</source>
        <translation type="vanished">导出失败：%1 个元器件全部失败</translation>
    </message>
    <message>
        <source>成功 %1 个，失败 %2 个</source>
        <translation type="vanished">成功 %1 个，失败 %2 个</translation>
    </message>
    <message>
        <source>
输出：符号 %1 · 封装 %2 · 3D %3</source>
        <translation type="vanished">
输出：符号 %1 · 封装 %2 · 3D %3</translation>
    </message>
    <message>
        <source>成功导出 1 个元器件</source>
        <translation type="vanished">成功导出 1 个元器件</translation>
    </message>
    <message>
        <source>成功导出 %1 个元器件</source>
        <translation type="vanished">成功导出 %1 个元器件</translation>
    </message>
    <message>
        <source>输出：符号 %1 · 封装 %2 · 3D %3</source>
        <translation type="vanished">输出：符号 %1 · 封装 %2 · 3D %3</translation>
    </message>
    <message>
        <source>%1 秒</source>
        <translation type="vanished">%1 秒</translation>
    </message>
    <message>
        <source>%1 分 %2 秒</source>
        <translation type="vanished">%1 分 %2 秒</translation>
    </message>
    <message>
        <source>耗时：%1</source>
        <translation type="vanished">耗时：%1</translation>
    </message>
    <message>
        <source>EasyKiConverter - LCSC 转换工具</source>
        <translation type="vanished">EasyKiConverter - LCSC 转换工具</translation>
    </message>
    <message>
        <source>显示窗口</source>
        <translation type="vanished">显示窗口</translation>
    </message>
    <message>
        <source>退出</source>
        <translation type="vanished">退出</translation>
    </message>
</context>
<context>
    <name>ResultListItem</name>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="205"/>
        <source>符号: %1</source>
        <translation>符号: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="226"/>
        <source>封装: %1</source>
        <translation>封装: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="44"/>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="54"/>
        <source>已导出</source>
        <translation>已导出</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="46"/>
        <source>失败</source>
        <translation>失败</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="48"/>
        <source>处理中</source>
        <translation>处理中</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="50"/>
        <source>已跳过</source>
        <translation>已跳过</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="52"/>
        <source>未启用</source>
        <translation>未启用</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="55"/>
        <source>等待中</source>
        <translation>等待中</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="119"/>
        <source>已复制</source>
        <translation>已复制</translation>
    </message>
    <message>
        <source>未完成</source>
        <translation type="vanished">未完成</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="247"/>
        <source>3D模型: %1</source>
        <translation>3D模型: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="268"/>
        <source>预览图: %1</source>
        <translation>预览图: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="289"/>
        <source>手册: %1</source>
        <translation>手册: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="328"/>
        <source>重试</source>
        <translation>重试</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="363"/>
        <source>删除</source>
        <translation>删除</translation>
    </message>
</context>
<context>
    <name>SliderDialogBase</name>
    <message>
        <location filename="../../src/ui/qml/components/SliderDialogBase.qml" line="50"/>
        <source>对话框</source>
        <translation>对话框</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SliderDialogBase.qml" line="51"/>
        <source>提示信息</source>
        <translation>提示信息</translation>
    </message>
</context>
<context>
    <name>SystemTrayManager</name>
    <message>
        <source>EasyKiConverter - LCSC 转换工具</source>
        <translation type="vanished">EasyKiConverter - LCSC 转换工具</translation>
    </message>
    <message>
        <source>显示窗口</source>
        <translation type="vanished">显示窗口</translation>
    </message>
    <message>
        <source>退出</source>
        <translation type="vanished">退出</translation>
    </message>
    <message>
        <source>导出完成</source>
        <translation type="vanished">导出完成</translation>
    </message>
    <message>
        <source>导出失败：%1 个元器件全部失败</source>
        <translation type="vanished">导出失败：%1 个元器件全部失败</translation>
    </message>
    <message>
        <source>成功 %1 个，失败 %2 个</source>
        <translation type="vanished">成功 %1 个，失败 %2 个</translation>
    </message>
    <message>
        <source>
输出：符号 %1 · 封装 %2 · 3D %3</source>
        <translation type="vanished">
输出：符号 %1 · 封装 %2 · 3D %3</translation>
    </message>
    <message>
        <source>成功导出 1 个元器件</source>
        <translation type="vanished">成功导出 1 个元器件</translation>
    </message>
    <message>
        <source>成功导出 %1 个元器件</source>
        <translation type="vanished">成功导出 %1 个元器件</translation>
    </message>
    <message>
        <source>输出：符号 %1 · 封装 %2 · 3D %3</source>
        <translation type="vanished">输出：符号 %1 · 封装 %2 · 3D %3</translation>
    </message>
    <message>
        <source>耗时：%1</source>
        <translation type="vanished">耗时：%1</translation>
    </message>
</context>
<context>
    <name>main</name>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="46"/>
        <source>EasyKiConverter - LCSC/EasyEDA 元件转 KiCad 库工具</source>
        <translation>EasyKiConverter - LCSC/EasyEDA 元件转 KiCad 库工具</translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="426"/>
        <source>错误: 无效的命令行参数</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="442"/>
        <source>错误: 参数值无效</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="502"/>
        <source>错误: </source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="519"/>
        <source>错误: 无效的 convert 子命令</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="520"/>
        <source>有效的子命令: bom, component, batch</source>
        <translation type="unfinished"></translation>
    </message>
</context>
</TS>
