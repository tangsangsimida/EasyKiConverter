import QtQuick
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "ExportProgressCard"
    width: 720
    height: 320
    visible: true
    when: windowShown

    QtObject {
        id: progressController
        property bool isExporting: false
        property real progress: 0
        property int fetchProgress: 0
        property int processProgress: 0
        property int writeProgress: 0
        property string status: ""
    }

    ExportProgressCard {
        id: progressCard
        width: parent.width
        exportProgressController: progressController
    }

    function init() {
        progressController.isExporting = false
        progressController.progress = 0
        progressController.fetchProgress = 0
        progressController.processProgress = 0
        progressController.writeProgress = 0
        progressController.status = ""
        wait(0)
    }

    function findByObjectName(item, objectName) {
        if (!item)
            return null
        if (item.objectName === objectName)
            return item

        for (var i = 0; i < item.children.length; ++i) {
            var child = findByObjectName(item.children[i], objectName)
            if (child)
                return child
        }

        return null
    }

    function statusLabel() {
        var label = findByObjectName(progressCard, "exportStatusLabel")
        verify(label !== null, "export status label should be discoverable")
        return label
    }

    function percentLabel() {
        var label = findByObjectName(progressCard, "exportProgressPercentLabel")
        verify(label !== null, "export progress percent label should be discoverable")
        return label
    }

    function progressBar() {
        var bar = findByObjectName(progressCard, "exportProgressBar")
        verify(bar !== null, "export progress bar should be discoverable")
        return bar
    }

    function test_hiddenWhenIdleWithNoProgress() {
        compare(progressCard.visible, false)
    }

    function test_visibleWhileExportingEvenBeforeProgress() {
        progressController.isExporting = true
        wait(0)

        compare(progressCard.visible, true)
        compare(percentLabel().text, "0%")
        compare(progressBar().height, 12)
    }

    function test_visibleAfterProgressAndShowsRoundedPercent() {
        progressController.progress = 42.6
        wait(0)

        compare(progressCard.visible, true)
        compare(percentLabel().text, "43%")
    }

    function test_statusLabelTracksControllerStatus() {
        var label = statusLabel()
        compare(label.visible, false)

        progressController.isExporting = true
        progressController.status = "Exporting components..."
        wait(0)

        compare(label.visible, true)
        compare(label.text, "Exporting components...")
    }
}
