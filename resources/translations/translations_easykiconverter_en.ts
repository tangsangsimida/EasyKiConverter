<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US">
<context>
    <name>CliConverter</name>
    <message>
        <location filename="../../src/utils/cli/FileReader.cpp" line="16"/>
        <location filename="../../src/utils/cli/FileReader.cpp" line="35"/>
        <source>输入文件不存在: %1</source>
        <translation>Input file does not exist: %1</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/FileReader.cpp" line="25"/>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="29"/>
        <source>BOM 表中没有找到有效的元器件编号</source>
        <translation>No valid component IDs found in BOM file</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/FileReader.cpp" line="41"/>
        <source>无法打开输入文件: %1</source>
        <translation>Cannot open input file: %1</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="17"/>
        <source>开始转换 BOM 表...</source>
        <translation>Starting BOM conversion...</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="33"/>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="32"/>
        <source>找到 %1 个元器件</source>
        <translation>Found %1 component(s)</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="62"/>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="53"/>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="61"/>
        <source>预加载完成，开始导出...</source>
        <translation>Preload completed, starting export...</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="76"/>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="75"/>
        <source>
转换完成: 成功 %1, 失败 %2</source>
        <translation>
Conversion completed: %1 succeeded, %2 failed</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BomConverter.cpp" line="85"/>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="84"/>
        <source>预加载完成: 成功 %1, 失败 %2</source>
        <translation>Preload completed: %1 succeeded, %2 failed</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="15"/>
        <source>开始转换单个元器件...</source>
        <translation>Starting single component conversion...</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="19"/>
        <source>未指定元器件编号</source>
        <translation>No component ID specified</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="23"/>
        <source>元器件编号: %1</source>
        <translation>Component ID: %1</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/ComponentConverter.cpp" line="67"/>
        <source>转换完成: 成功 %1, 失败 %2</source>
        <translation>Conversion completed: %1 succeeded, %2 failed</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="16"/>
        <source>开始批量转换...</source>
        <translation>Starting batch conversion...</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/BatchConverter.cpp" line="28"/>
        <source>元器件列表文件为空</source>
        <translation>Component list file is empty</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/CliConverter.cpp" line="31"/>
        <source>未知的 CLI 模式</source>
        <translation>Unknown CLI mode</translation>
    </message>
    <message>
        <location filename="../../src/utils/cli/CliConverter.cpp" line="45"/>
        <source>未知错误</source>
        <translation>Unknown error</translation>
    </message>
</context>
<context>
    <name>CommandLineParser</name>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="285"/>
        <source>无效的日志级别: %1（有效值: %2）</source>
        <translation>Invalid log level: %1 (valid values: %2)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="296"/>
        <source>无效的语言设置: %1（有效值: %2）</source>
        <translation>Invalid language setting: %1 (valid values: %2)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="307"/>
        <source>无效的主题设置: %1（有效值: %2）</source>
        <translation>Invalid theme setting: %1 (valid values: %2)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="315"/>
        <source>缓存目录不能为空</source>
        <translation>Cache directory cannot be empty</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="323"/>
        <source>磁盘缓存大小必须是大于 0 的整数（单位: MB）</source>
        <translation>Disk cache size must be an integer greater than 0 (unit: MB)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="332"/>
        <source>无效的 3D 模型格式: %1（有效值: %2）</source>
        <translation>Invalid 3D model format: %1 (valid values: %2)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="340"/>
        <source>CLI 模式必须指定输出目录 (-o/--output)</source>
        <translation>CLI mode requires output directory (-o/--output)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="345"/>
        <source>BOM 表转换必须指定输入文件 (-i/--input)</source>
        <translation>BOM conversion requires input file (-i/--input)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="349"/>
        <source>单个元器件转换必须指定 LCSC 编号 (-c/--component)</source>
        <translation>Single component conversion requires LCSC ID (-c/--component)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="355"/>
        <source>批量转换必须指定输入文件 (-i/--input)</source>
        <translation>Batch conversion requires input file (-i/--input)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="423"/>
        <source>EasyKiConverter CLI 模式</source>
        <translation>EasyKiConverter CLI Mode</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="424"/>
        <source>用法:</source>
        <translation>Usage:</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="426"/>
        <source>&lt;子命令&gt; [选项]</source>
        <translation>&lt;subcommand&gt; [options]</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="427"/>
        <source>子命令:</source>
        <translation>Subcommands:</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="428"/>
        <source>转换 BOM 表文件</source>
        <translation>Convert BOM file</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="430"/>
        <source>转换单个元器件（通过 LCSC 编号）</source>
        <translation>Convert single component (by LCSC ID)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="432"/>
        <source>批量转换元器件（通过元器件列表文件）</source>
        <translation>Batch convert components (from component list file)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="433"/>
        <source>选项:</source>
        <translation>Options:</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="435"/>
        <source>输入文件路径（BOM 表或元器件列表文件）</source>
        <translation>Input file path (BOM or component list file)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="436"/>
        <source>输出目录路径（必需）</source>
        <translation>Output directory path (required)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="439"/>
        <source>导出库名称（默认: EasyKiConverter）</source>
        <translation>Export library name (default: EasyKiConverter)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="440"/>
        <source>LCSC 元器件编号</source>
        <translation>LCSC component ID</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="443"/>
        <source>导出符号库（默认: true）</source>
        <translation>Export symbol library (default: true)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="445"/>
        <source>导出封装库（默认: true）</source>
        <translation>Export footprint library (default: true)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="446"/>
        <source>导出 3D 模型</source>
        <translation>Export 3D models</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="448"/>
        <source>3D 模型格式（wrl/step/both，默认: wrl）</source>
        <translation>3D model format (wrl/step/both, default: wrl)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="449"/>
        <source>导出预览图</source>
        <translation>Export preview images</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="450"/>
        <source>设置磁盘缓存目录</source>
        <translation>Set disk cache directory</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="453"/>
        <source>设置磁盘缓存大小限制 (MB)</source>
        <translation>Set disk cache size limit (MB)</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="454"/>
        <source>显示进度条</source>
        <translation>Show progress bar</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="455"/>
        <source>安静模式，减少输出</source>
        <translation>Quiet mode, reduce output</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="457"/>
        <source>示例:</source>
        <translation>Examples:</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="458"/>
        <source>转换 BOM 表</source>
        <translation>Convert BOM file</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="460"/>
        <source>转换单个元器件</source>
        <translation>Convert single component</translation>
    </message>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="462"/>
        <source>批量转换</source>
        <translation>Batch convert</translation>
    </message>
</context>
<context>
    <name>ComponentListCard</name>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="386"/>
        <source>全部 (%1)</source>
        <translation>All (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="407"/>
        <source>验证中 (%1)</source>
        <translation>Validating (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="432"/>
        <source>有效 (%1)</source>
        <translation>Valid (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="457"/>
        <source>无效 (%1)</source>
        <translation>Invalid (%1)</translation>
    </message>
</context>
<context>
    <name>ComponentListItem</name>
    <message>
        <source>已复制 ID</source>
        <translation type="vanished">ID Copied</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="87"/>
        <source>已复制</source>
        <translation>Copied</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListItem.qml" line="590"/>
        <source>编辑元器件描述</source>
        <translation>Edit Component Description</translation>
    </message>
</context>
<context>
    <name>ConfirmDialog</name>
    <message>
        <location filename="../../src/ui/qml/components/ConfirmDialog.qml" line="30"/>
        <source>退出确认</source>
        <translation>Exit Confirmation</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ConfirmDialog.qml" line="31"/>
        <source>转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？</source>
        <translation>Conversion is in progress. Exiting will cancel the current conversion, but exported files will be preserved. Are you sure you want to exit?</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ConfirmDialog.qml" line="33"/>
        <source>确定</source>
        <translation>OK</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ConfirmDialog.qml" line="34"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
</context>
<context>
    <name>DescriptionEditDialog</name>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="14"/>
        <source>元器件描述</source>
        <translation>Component Description</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="15"/>
        <source>编辑当前元器件导出到符号和封装中的描述。</source>
        <translation>Edit the description exported to symbol and footprint for this component.</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="30"/>
        <source>留空则不导出描述</source>
        <translation>Leave empty to skip description export</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="44"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/DescriptionEditDialog.qml" line="55"/>
        <source>保存</source>
        <translation>Save</translation>
    </message>
</context>
<context>
    <name>ExitDialog</name>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="63"/>
        <source>关闭程序</source>
        <translation>Close Program</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="64"/>
        <source>您可以选择最小化到系统托盘以保持后台运行，或者完全退出程序。</source>
        <translation>You can choose to minimize to the system tray to keep running in the background, or completely exit the program.</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="34"/>
        <source>最小化到托盘</source>
        <translation>Minimize to Tray</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="44"/>
        <source>退出程序</source>
        <translation>Exit Program</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="54"/>
        <source>取消</source>
        <translation>Cancel</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExitDialog.qml" line="113"/>
        <source>记住我的选择</source>
        <translation>Remember my choice</translation>
    </message>
</context>
<context>
    <name>ExportResultsCard</name>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="106"/>
        <source>全部 (%1)</source>
        <translation>All (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="133"/>
        <source>导出中 (%1)</source>
        <translation>Exporting (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="160"/>
        <source>成功 (%1)</source>
        <translation>Success (%1)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="187"/>
        <source>失败 (%1)</source>
        <translation>Failed (%1)</translation>
    </message>
</context>
<context>
    <name>LanguageManager</name>
    <message>
        <source>语言设置</source>
        <translation type="vanished">Language Settings</translation>
    </message>
    <message>
        <source>跟随系统</source>
        <translation type="vanished">System Default</translation>
    </message>
    <message>
        <source>简体中文</source>
        <translation type="vanished">Simplified Chinese</translation>
    </message>
    <message>
        <source>English</source>
        <translation type="vanished">English</translation>
    </message>
    <message>
        <source>语言切换将在重启后完全生效</source>
        <translation type="vanished">Language changes will take full effect after restart</translation>
    </message>
</context>
<context>
    <name>Main</name>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="35"/>
        <source>确认退出</source>
        <translation>Confirm Exit</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="36"/>
        <source>转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？</source>
        <translation>Conversion is in progress. Exiting will cancel the current conversion, but exported files will be preserved. Are you sure you want to exit?</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="37"/>
        <source>强制退出</source>
        <translation>Force Exit</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/Main.qml" line="38"/>
        <source>继续转换</source>
        <translation>Continue</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="207"/>
        <source>选择 BOM 文件</source>
        <translation>Select BOM File</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/HeaderSection.qml" line="26"/>
        <source>将嘉立创EDA元器件转换为KiCad格式</source>
        <translation>Convert LCSC EDA Components to KiCad Format</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentInputCard.qml" line="10"/>
        <source>添加元器件</source>
        <translation>Add Component</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentInputCard.qml" line="18"/>
        <source>输入LCSC元件编号 (例如: C2040)</source>
        <translation>Enter LCSC Component ID (e.g., C2040)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentInputCard.qml" line="52"/>
        <source>添加</source>
        <translation>Add</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentInputCard.qml" line="63"/>
        <source>粘贴</source>
        <translation>Paste</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="14"/>
        <source>导入BOM文件</source>
        <translation>Import BOM File</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="19"/>
        <source>选择BOM文件</source>
        <translation>Select BOM File</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="44"/>
        <source>客户端弱网络适配</source>
        <translation>Client Weak Network Adaptation</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="53"/>
        <source>仅影响本机网络请求策略，不代表服务器限流；开启后会使用更保守的并发、超时和重试配置</source>
        <translation>Only affects local network request policy, does not represent server rate limiting; when enabled, more conservative concurrency, timeout, and retry settings will be used</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="95"/>
        <source>导入 BOM 前可先开启，验证和预览图加载也会使用该策略</source>
        <translation>Can be enabled before importing BOM, verification and preview image loading will also use this policy</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="17"/>
        <source>元器件列表</source>
        <translation>Component List</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="312"/>
        <source>共 %1 个元器件</source>
        <translation>Total %1 Components</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="483"/>
        <source>搜索元器件...</source>
        <translation>Search Components...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="555"/>
        <source>重试所有</source>
        <translation>Retry All</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="604"/>
        <source>复制所有编号</source>
        <translation>Copy All IDs</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="629"/>
        <source>已复制所有编号</source>
        <translation>All IDs Copied</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ComponentListCard.qml" line="636"/>
        <source>清空列表</source>
        <translation>Clear List</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="11"/>
        <source>导出设置</source>
        <translation>Settings</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="34"/>
        <source>输出路径</source>
        <translation>Path</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="216"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="53"/>
        <source>选择输出目录</source>
        <translation>Select Output Directory</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="65"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="159"/>
        <source>浏览</source>
        <translation>Browse</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="82"/>
        <source>库名称</source>
        <translation>Library Name</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="98"/>
        <source>输入库名称 (例如: MyLibrary)</source>
        <translation>Enter Library Name (e.g., MyLibrary)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="20"/>
        <source>基础配置</source>
        <translation>Basic Configuration</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="115"/>
        <source>缓存配置</source>
        <translation>Cache Settings</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="128"/>
        <source>缓存目录</source>
        <translation>Cache Directory</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="175"/>
        <source>磁盘缓存上限 (MB)</source>
        <translation>Disk Cache Limit (MB)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="230"/>
        <source>库描述</source>
        <translation>Library Description</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="243"/>
        <source>符号库描述 (sym-lib-table)</source>
        <translation>Symbol Library Description (sym-lib-table)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="258"/>
        <source>输入符号库描述</source>
        <translation>Enter Symbol Library Description</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="132"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="317"/>
        <source>符号库</source>
        <translation>Symbols</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="276"/>
        <source>封装库描述 (fp-lib-table)</source>
        <translation>Footprint Library Description (fp-lib-table)</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="291"/>
        <source>输入封装库描述</source>
        <translation>Enter Footprint Library Description</translation>
    </message>
    <message>
        <source>关键词</source>
        <translation type="vanished">Keywords</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="132"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="356"/>
        <source>封装库</source>
        <translation>Footprints</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="308"/>
        <source>导出选项</source>
        <translation>Export Options</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="584"/>
        <source>导出模式</source>
        <translation>Export Mode</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="593"/>
        <source>追加模式</source>
        <translation>Append Mode</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="640"/>
        <source>更新模式</source>
        <translation>Update Mode</translation>
    </message>
    <message>
        <source>调试模式</source>
        <translation type="vanished">Debug Mode</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="132"/>
        <source>追加</source>
        <translation>Append</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="602"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="631"/>
        <source>保留已经存在的元器件数据，并追加新的元器件</source>
        <translation>Preserve existing component data and append new components</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="132"/>
        <source>更新</source>
        <translation>Update</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="649"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="678"/>
        <source>覆盖已经存在的元器件数据，并追加新的元器件</source>
        <translation>Overwrite existing component data and append new components</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportProgressCard.qml" line="10"/>
        <source>转换进度</source>
        <translation>Export Progress</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportResultsCard.qml" line="14"/>
        <source>转换结果</source>
        <translation>Export Results</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="77"/>
        <source>重试失败项</source>
        <translation>Retry Failed Items</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="11"/>
        <source>导出统计</source>
        <translation>Export Statistics</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="18"/>
        <source>基本统计</source>
        <translation>Basic Statistics</translation>
    </message>
    <message>
        <source>时间统计</source>
        <translation type="vanished">Time Statistics</translation>
    </message>
    <message>
        <source>网络统计</source>
        <translation type="vanished">Network Statistics</translation>
    </message>
    <message>
        <source>峰值内存</source>
        <translation type="vanished">Peak Memory</translation>
    </message>
    <message>
        <source>打开详细统计报告</source>
        <translation type="vanished">Open Detailed Report</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="153"/>
        <source>打开缓存目录</source>
        <translation>Open Cache Directory</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="46"/>
        <source>打开导出目录</source>
        <translation>Open Export Directory</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="19"/>
        <source>错误</source>
        <translation>Error</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="22"/>
        <source>打开导出目录失败，请检查导出路径是否存在。</source>
        <translation>Failed to open export directory. Please check if the export path exists.</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/BomImportCard.qml" line="31"/>
        <source>未选择文件</source>
        <translation>No File Selected</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="132"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="397"/>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="97"/>
        <source>3D模型</source>
        <translation>3D</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="132"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="505"/>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="106"/>
        <source>预览图</source>
        <translation>Preview</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="132"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="544"/>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="111"/>
        <source>手册</source>
        <translation>Datasheet</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="134"/>
        <source>保留已存在的元器件</source>
        <translation>Keep Existing Components</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="134"/>
        <source>覆盖已存在的元器件</source>
        <translation>Overwrite Existing Components</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/MainWindow.qml" line="224"/>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="147"/>
        <source>选择缓存目录</source>
        <translation>Select Cache Directory</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportProgressCard.qml" line="25"/>
        <source>数据抓取</source>
        <translation>Data Fetch</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportProgressCard.qml" line="48"/>
        <source>数据处理</source>
        <translation>Data Processing</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportProgressCard.qml" line="71"/>
        <source>文件写入</source>
        <translation>File Writing</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="27"/>
        <source>总数</source>
        <translation>Total</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="32"/>
        <source>成功</source>
        <translation>Success</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="38"/>
        <source>失败</source>
        <translation>Failed</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="44"/>
        <source>成功率</source>
        <translation>Success Rate</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="60"/>
        <source>抓取进度</source>
        <translation>Fetch Progress</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="65"/>
        <source>处理进度</source>
        <translation>Processing Progress</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="70"/>
        <source>写入进度</source>
        <translation>Write Progress</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="77"/>
        <source>导出详情</source>
        <translation>Export Details</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="126"/>
        <source>打开输出目录</source>
        <translation>Open Output Directory</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="140"/>
        <source>清空缓存</source>
        <translation>Clear Cache</translation>
    </message>
    <message>
        <source>总耗时</source>
        <translation type="vanished">Total Time</translation>
    </message>
    <message>
        <source>平均抓取</source>
        <translation type="vanished">Avg Fetch</translation>
    </message>
    <message>
        <source>平均处理</source>
        <translation type="vanished">Avg Process</translation>
    </message>
    <message>
        <source>平均写入</source>
        <translation type="vanished">Avg Write</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="87"/>
        <source>符号</source>
        <translation>Symbols</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportStatisticsCard.qml" line="92"/>
        <source>封装</source>
        <translation>Footprints</translation>
    </message>
    <message>
        <source>总请求数</source>
        <translation type="vanished">Total Requests</translation>
    </message>
    <message>
        <source>重试次数</source>
        <translation type="vanished">Retries</translation>
    </message>
    <message>
        <source>平均延迟</source>
        <translation type="vanished">Avg Latency</translation>
    </message>
    <message>
        <source>速率限制</source>
        <translation type="vanished">Rate Limit</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="75"/>
        <source>正在转换...</source>
        <translation>Exporting...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="78"/>
        <source>开始转换</source>
        <translation>Start Export</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="139"/>
        <source>正在停止...</source>
        <translation>Stopping...</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportButtonsSection.qml" line="139"/>
        <source>停止转换</source>
        <translation>Stop Export</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="326"/>
        <source>导出符号库文件</source>
        <translation>Export symbol library files</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="365"/>
        <source>导出封装库文件</source>
        <translation>Export footprint library files</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="406"/>
        <source>导出 3D 模型文件</source>
        <translation>Export 3D model files</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="514"/>
        <source>导出预览图文件</source>
        <translation>Export preview image files</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ExportSettingsCard.qml" line="553"/>
        <source>导出数据手册文件</source>
        <translation>Export datasheet files</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="45"/>
        <source>发现新版本 %1</source>
        <translation>New version %1 found</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="52"/>
        <source>当前版本 %1，最新发布：%2</source>
        <translation>Current version %1, latest release: %2</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="52"/>
        <source>当前版本 %1，可前往 GitHub 查看发布说明。</source>
        <translation>Current version %1, please visit GitHub to view release notes.</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="63"/>
        <source>查看更新</source>
        <translation>View Update</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/UpdateBanner.qml" line="72"/>
        <source>稍后提醒</source>
        <translation>Remind Later</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>导出完成</source>
        <translation type="vanished">Export Complete</translation>
    </message>
    <message>
        <source>导出失败：%1 个元器件全部失败</source>
        <translation type="vanished">Export failed: All %1 components failed</translation>
    </message>
    <message>
        <source>成功 %1 个，失败 %2 个</source>
        <translation type="vanished">Success: %1, Failed: %2</translation>
    </message>
    <message>
        <source>
输出：符号 %1 · 封装 %2 · 3D %3</source>
        <translation type="vanished">
Output: Symbols %1 · Footprints %2 · 3D %3</translation>
    </message>
    <message>
        <source>成功导出 1 个元器件</source>
        <translation type="vanished">Successfully exported 1 component</translation>
    </message>
    <message>
        <source>成功导出 %1 个元器件</source>
        <translation type="vanished">Successfully exported %1 components</translation>
    </message>
    <message>
        <source>输出：符号 %1 · 封装 %2 · 3D %3</source>
        <translation type="vanished">Output: Symbols %1 · Footprints %2 · 3D %3</translation>
    </message>
    <message>
        <source>%1 秒</source>
        <translation type="vanished">%1 seconds</translation>
    </message>
    <message>
        <source>%1 分 %2 秒</source>
        <translation type="vanished">%1 min %2 sec</translation>
    </message>
    <message>
        <source>耗时：%1</source>
        <translation type="vanished">Time: %1</translation>
    </message>
    <message>
        <source>EasyKiConverter - LCSC 转换工具</source>
        <translation type="vanished">EasyKiConverter - LCSC Conversion Tool</translation>
    </message>
    <message>
        <source>显示窗口</source>
        <translation type="vanished">Show Window</translation>
    </message>
    <message>
        <source>退出</source>
        <translation type="vanished">Exit</translation>
    </message>
</context>
<context>
    <name>ResultListItem</name>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="178"/>
        <source>符号: %1</source>
        <translation>Symbol: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="200"/>
        <source>封装: %1</source>
        <translation>Footprint: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="44"/>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="54"/>
        <source>已导出</source>
        <translation>Exported</translation>
    </message>
    <message>
        <source>已复制 ID</source>
        <translation type="obsolete">ID Copied</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="119"/>
        <source>已复制</source>
        <translation>Copied</translation>
    </message>
    <message>
        <source>未完成</source>
        <translation type="vanished">Pending</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="46"/>
        <source>失败</source>
        <translation>Failed</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="48"/>
        <source>处理中</source>
        <translation>Processing</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="50"/>
        <source>已跳过</source>
        <translation>Skipped</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="52"/>
        <source>未启用</source>
        <translation>Not Enabled</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="55"/>
        <source>等待中</source>
        <translation>Waiting</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="222"/>
        <source>3D模型: %1</source>
        <translation>3D Model: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="244"/>
        <source>预览图: %1</source>
        <translation>Preview: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="266"/>
        <source>手册: %1</source>
        <translation>Datasheet: %1</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="305"/>
        <source>重试</source>
        <translation>Retry</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/ResultListItem.qml" line="328"/>
        <source>删除</source>
        <translation>Delete</translation>
    </message>
</context>
<context>
    <name>SliderDialogBase</name>
    <message>
        <location filename="../../src/ui/qml/components/SliderDialogBase.qml" line="41"/>
        <source>对话框</source>
        <translation>Dialog</translation>
    </message>
    <message>
        <location filename="../../src/ui/qml/components/SliderDialogBase.qml" line="42"/>
        <source>提示信息</source>
        <translation>Prompt</translation>
    </message>
</context>
<context>
    <name>main</name>
    <message>
        <location filename="../../src/utils/CommandLineParser.cpp" line="38"/>
        <source>EasyKiConverter - LCSC/EasyEDA 元件转 KiCad 库工具</source>
        <translation>EasyKiConverter - LCSC/EasyEDA to KiCad Library Converter</translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="325"/>
        <source>错误: 无效的命令行参数</source>
        <translation>Error: Invalid command line arguments</translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="341"/>
        <source>错误: 参数值无效</source>
        <translation>Error: Invalid parameter values</translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="400"/>
        <source>错误: </source>
        <translation>Error: </translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="417"/>
        <source>错误: 无效的 convert 子命令</source>
        <translation>Error: Invalid convert subcommand</translation>
    </message>
    <message>
        <location filename="../../src/main.cpp" line="418"/>
        <source>有效的子命令: bom, component, batch</source>
        <translation>Valid subcommands: bom, component, batch</translation>
    </message>
</context>
</TS>
