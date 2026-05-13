import QtQuick
import QtTest
import "../../src/ui/qml/components"

TestCase {
    name: "ExportResultsCard"
    width: 760
    height: 520
    visible: true
    when: windowShown

    QtObject {
        id: progressController
        property bool isExporting: false
        property var resultsList: []
        property string filterMode: "all"
        property int filteredPendingCount: 0
        property int successCount: 0
        property int failureCount: 0
        property string lastFilterMode: ""
        property string lastRetryComponentId: ""
        property string lastRemovedComponentId: ""

        function setFilterMode(mode) {
            filterMode = mode
            lastFilterMode = mode
        }

        function retryComponent(componentId) {
            lastRetryComponentId = componentId
        }

        function removeResult(componentId) {
            lastRemovedComponentId = componentId
        }
    }

    ExportResultsCard {
        id: resultsCard
        width: parent.width
        exportProgressController: progressController
    }

    function init() {
        progressController.isExporting = false
        progressController.resultsList = []
        progressController.filterMode = "all"
        progressController.filteredPendingCount = 0
        progressController.successCount = 0
        progressController.failureCount = 0
        progressController.lastFilterMode = ""
        progressController.lastRetryComponentId = ""
        progressController.lastRemovedComponentId = ""
        wait(0)
    }

    function findByObjectName(item, objectName) {
        if (!item)
            return null
        if (item.objectName === objectName)
            return item

        var i
        if (item.children) {
            for (i = 0; i < item.children.length; ++i) {
                var child = findByObjectName(item.children[i], objectName)
                if (child)
                    return child
            }
        }

        if (item.contentItem && item.contentItem.children) {
            for (i = 0; i < item.contentItem.children.length; ++i) {
                var contentChild = findByObjectName(item.contentItem.children[i], objectName)
                if (contentChild)
                    return contentChild
            }
        }

        return null
    }

    function makeResults() {
        return [
            {
                "componentId": "C100",
                "status": "success",
                "message": "Exported",
                "symbolStatus": "success",
                "footprintStatus": "success",
                "model3DStatus": "disabled",
                "previewStatus": "disabled",
                "datasheetStatus": "disabled",
                "symbolSuccess": true,
                "footprintSuccess": true
            },
            {
                "componentId": "C200",
                "status": "failed",
                "error": "Fixture failure",
                "symbolStatus": "failed",
                "footprintStatus": "pending",
                "model3DStatus": "disabled",
                "previewStatus": "disabled",
                "datasheetStatus": "disabled"
            },
            {
                "componentId": "C300",
                "status": "in_progress",
                "message": "Exporting",
                "symbolStatus": "in_progress",
                "footprintStatus": "pending",
                "model3DStatus": "disabled",
                "previewStatus": "disabled",
                "datasheetStatus": "disabled"
            }
        ]
    }

    function loadResults() {
        progressController.resultsList = makeResults()
        progressController.filteredPendingCount = 1
        progressController.successCount = 1
        progressController.failureCount = 1
        wait(100)
    }

    function cardItem() {
        verify(resultsCard.item !== null, "results loader should have an active item")
        return resultsCard.item
    }

    function resultItem(componentId) {
        var item = findByObjectName(cardItem(), "exportResultItem_" + componentId)
        verify(item !== null, "result item should be discoverable: " + componentId)
        return item
    }

    function filterButton(objectName) {
        var button = findByObjectName(cardItem(), objectName)
        verify(button !== null, "filter button should be discoverable: " + objectName)
        return button
    }

    function test_hiddenWhenIdleWithNoResults() {
        compare(resultsCard.active, false)
        compare(resultsCard.visible, false)
    }

    function test_activeWhileExportingBeforeResultsArrive() {
        progressController.isExporting = true
        wait(0)

        compare(resultsCard.active, true)
        verify(resultsCard.item !== null)
        compare(findByObjectName(cardItem(), "exportResultsList").visible, false)
    }

    function test_resultItemsRenderWhenResultsExist() {
        loadResults()

        compare(resultsCard.active, true)
        verify(findByObjectName(cardItem(), "exportResultsFilterSegmentedControl").visible)
        verify(findByObjectName(cardItem(), "exportResultsList").visible)
        compare(resultItem("C100").status, "success")
        compare(resultItem("C200").status, "failed")
        compare(resultItem("C300").status, "in_progress")
    }

    function test_filterButtonsUpdateControllerMode() {
        loadResults()

        mouseClick(filterButton("exportResultsFilterFailedButton"))
        compare(progressController.lastFilterMode, "failed")
        compare(progressController.filterMode, "failed")

        mouseClick(filterButton("exportResultsFilterSuccessButton"))
        compare(progressController.lastFilterMode, "success")
        compare(progressController.filterMode, "success")

        mouseClick(filterButton("exportResultsFilterAllButton"))
        compare(progressController.lastFilterMode, "all")
        compare(progressController.filterMode, "all")
    }

    function test_disabledFilterDoesNotChangeControllerMode() {
        progressController.resultsList = makeResults()
        progressController.filteredPendingCount = 0
        progressController.successCount = 1
        progressController.failureCount = 1
        wait(100)

        mouseClick(filterButton("exportResultsFilterExportingButton"))

        compare(progressController.lastFilterMode, "")
        compare(progressController.filterMode, "all")
    }

    function test_retryAndDeleteFailedResultCallController() {
        loadResults()

        var failedItem = resultItem("C200")
        failedItem.retryClicked()
        compare(progressController.lastRetryComponentId, "C200")

        failedItem.deleteClicked()
        compare(progressController.lastRemovedComponentId, "C200")
    }
}
