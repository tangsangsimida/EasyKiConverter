import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "WindowControllerClosing"
    width: 480
    height: 320
    visible: true
    when: windowShown

    ApplicationWindow {
        id: testWindow
        width: 320
        height: 240
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
        property string preference: ""
        property int getExitPreferenceCount: 0

        function getExitPreference() {
            getExitPreferenceCount += 1
            return preference
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

    QtObject {
        id: closeConfirmDialog
        property bool visible: false
        property int openCount: 0
        property int closeCount: 0

        function open() {
            visible = true
            openCount += 1
        }

        function close() {
            visible = false
            closeCount += 1
        }
    }

    QtObject {
        id: exitOptionDialog
        property bool visible: false
        property int openCount: 0
        property int closeCount: 0

        function open() {
            visible = true
            openCount += 1
        }

        function close() {
            visible = false
            closeCount += 1
        }
    }

    WindowController {
        id: controller
        window: testWindow
        configService: configService
        exportProgressController: exportProgressController
        closeConfirmDialog: closeConfirmDialog
        exitOptionDialog: exitOptionDialog
    }

    function init() {
        exportProgressController.isExporting = false
        exportProgressController.handleCloseRequestCount = 0
        configService.preference = ""
        configService.getExitPreferenceCount = 0
        closeConfirmDialog.visible = false
        closeConfirmDialog.openCount = 0
        closeConfirmDialog.closeCount = 0
        exitOptionDialog.visible = false
        exitOptionDialog.openCount = 0
        exitOptionDialog.closeCount = 0
        controller.forceExitRequested = false
        wait(0)
    }

    function test_handleClosingWhileExportingRejectsCloseAndShowsConfirmDialog() {
        exportProgressController.isExporting = true
        var closeEvent = {
            "accepted": true
        }

        controller.handleClosing(closeEvent)

        compare(closeEvent.accepted, false)
        compare(closeConfirmDialog.visible, true)
        compare(closeConfirmDialog.openCount, 1)
        compare(exitOptionDialog.openCount, 0)
        compare(exportProgressController.handleCloseRequestCount, 0)
    }

    function test_handleClosingWithoutExportShowsExitOptions() {
        var closeEvent = {
            "accepted": true
        }

        controller.handleClosing(closeEvent)

        compare(closeEvent.accepted, false)
        compare(configService.getExitPreferenceCount, 1)
        compare(exitOptionDialog.visible, true)
        compare(exitOptionDialog.openCount, 1)
        compare(closeConfirmDialog.openCount, 0)
    }

    function test_existingDialogPreventsDuplicateClosePrompt() {
        exportProgressController.isExporting = true
        closeConfirmDialog.visible = true

        controller.processCloseRequest()

        compare(closeConfirmDialog.openCount, 0)
        compare(exitOptionDialog.openCount, 0)
    }

    function test_handleEscTogglesVisibleDialogsBeforeOpeningNewPrompt() {
        closeConfirmDialog.visible = true

        controller.handleEsc()

        compare(closeConfirmDialog.visible, false)
        compare(closeConfirmDialog.closeCount, 1)
        compare(exitOptionDialog.openCount, 0)

        exportProgressController.isExporting = false
        controller.handleEsc()

        compare(exitOptionDialog.visible, true)
        compare(exitOptionDialog.openCount, 1)
    }

    function test_handleClosingAllowsCloseAfterForcedExitFlag() {
        controller.forceExitRequested = true
        var closeEvent = {
            "accepted": false
        }

        controller.handleClosing(closeEvent)

        compare(closeEvent.accepted, true)
        compare(closeConfirmDialog.openCount, 0)
        compare(exitOptionDialog.openCount, 0)
    }
}
