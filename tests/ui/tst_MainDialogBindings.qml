import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "MainDialogBindings"
    width: 520
    height: 360
    visible: true
    when: windowShown

    ApplicationWindow {
        id: testWindow
        width: 360
        height: 260
        visible: false
    }

    QtObject {
        id: exportProgressController
        property bool isExporting: false
        property int handleCloseRequestCount: 0

        function handleCloseRequest() {
            handleCloseRequestCount += 1
        }
    }

    QtObject {
        id: configService
        property string exitPreference: ""

        function getExitPreference() {
            return exitPreference
        }

        function getWindowState() {
            return {
                "x": -9999,
                "y": -9999,
                "width": -1,
                "height": -1,
                "maximized": false
            }
        }
    }

    WindowController {
        id: appWindowController
        window: testWindow
        configService: configService
        exportProgressController: exportProgressController
        closeConfirmDialog: closeConfirmDialog
        exitOptionDialog: exitOptionDialog
    }

    ConfirmDialog {
        id: closeConfirmDialog
        title: qsTr("确认退出")
        message: qsTr("转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？")
        confirmText: qsTr("强制退出")
        cancelText: qsTr("继续转换")
        onRejected: appWindowController.resumeExport()
    }

    ExitDialog {
        id: exitOptionDialog
        onCanceled: appWindowController.resumeExport()
    }

    function init() {
        exportProgressController.isExporting = false
        exportProgressController.handleCloseRequestCount = 0
        configService.exitPreference = ""
        appWindowController.forceExitRequested = false
        closeConfirmDialog.close()
        exitOptionDialog.close()
        wait(0)
    }

    function test_exportingCloseRequestOpensRealConfirmDialog() {
        exportProgressController.isExporting = true

        appWindowController.processCloseRequest()

        compare(closeConfirmDialog.visible, true)
        compare(exitOptionDialog.visible, false)
        compare(exportProgressController.handleCloseRequestCount, 0)
    }

    function test_rejectingConfirmDialogResumesExportAndClosesDialog() {
        exportProgressController.isExporting = true
        appWindowController.processCloseRequest()
        compare(closeConfirmDialog.visible, true)

        closeConfirmDialog.rejected()
        wait(0)

        compare(closeConfirmDialog.visible, false)
        compare(appWindowController.forceExitRequested, false)
        compare(exportProgressController.handleCloseRequestCount, 0)
    }

    function test_idleCloseRequestOpensRealExitOptionsDialog() {
        exportProgressController.isExporting = false

        appWindowController.processCloseRequest()

        compare(exitOptionDialog.visible, true)
        compare(closeConfirmDialog.visible, false)
    }

    function test_cancelingExitOptionsDialogReturnsToWindow() {
        appWindowController.processCloseRequest()
        compare(exitOptionDialog.visible, true)

        exitOptionDialog.getButtonSpecByIndex(2).action()
        wait(300)

        compare(exitOptionDialog.visible, false)
        compare(appWindowController.forceExitRequested, false)
        compare(exportProgressController.handleCloseRequestCount, 0)
    }
}
