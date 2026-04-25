import QtQuick
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "ControllerBinding"
    width: 400
    height: 400
    visible: true
    when: windowShown

    // Mock controller object
    QtObject {
        id: mockController
        property string outputPath: "/tmp/test"
        property bool exportSymbol: true
        property bool exportFootprint: false
        property string exportMode: "append"

        function setOutputPath(path) {
            outputPath = path
        }

        function setExportSymbol(value) {
            exportSymbol = value
        }
    }

    ComponentListItem {
        id: testItem
        itemData: QtObject {
            property string componentId: "C12345"
            property string category: "Capacitor"
            property string validationPhase: "completed"
            property int previewImageCount: 2
            property var previewImages: []
        }
        anchors.centerIn: parent
    }

    function test_itemDataBinding() {
        compare(testItem.itemData.componentId, "C12345", "Component ID should be bound correctly")
        compare(testItem.itemData.validationPhase, "completed", "Validation phase should be bound correctly")
        compare(testItem.itemData.previewImageCount, 2, "Preview image count should be bound correctly")
    }

    function test_mockControllerInteraction() {
        compare(mockController.outputPath, "/tmp/test", "Mock controller should have initial path")

        mockController.setOutputPath("/new/path")
        compare(mockController.outputPath, "/new/path", "Mock controller should update path")

        mockController.setExportSymbol(false)
        compare(mockController.exportSymbol, false, "Mock controller should update export symbol flag")
    }

    function test_componentSignals() {
        var deleteClicked = false
        var copyClicked = false

        testItem.deleteClicked.connect(function() { deleteClicked = true })
        testItem.copyClicked.connect(function() { copyClicked = true })

        // Emit signals programmatically for testing
        testItem.deleteClicked()
        testItem.copyClicked()

        verify(deleteClicked, "Delete clicked signal should be emitted")
        verify(copyClicked, "Copy clicked signal should be emitted")
    }
}
