import QtQuick 2.15
import QtTest 1.15
import "../../src/ui/qml/components"

TestCase {
    name: "ExportSettingsCard"
    width: 900
    height: 700
    visible: true
    when: windowShown

    QtObject {
        id: controller
        property string outputPath: "/tmp/out"
        property string libName: "TestLib"
        property string cacheDir: "/tmp/cache"
        property int diskCacheLimitMB: 5120
        property int maxDiskCacheLimitMB: 1048576
        property int lastDiskCacheLimit: -1
        property bool exportSymbol: true
        property bool exportFootprint: true
        property bool exportModel3D: true
        property int exportModel3DFormat: 3
        property int exportModel3DPathMode: 0
        property bool exportPreviewImages: false
        property bool exportDatasheet: false
        property int exportMode: 0
        property string symbolLibraryDescription: ""
        property string footprintLibraryDescription: ""

        function setOutputPath(path) {
            outputPath = path
        }
        function setLibName(name) {
            libName = name
        }
        function setCacheDir(path) {
            cacheDir = path
        }
        function setDiskCacheLimitMB(value) {
            lastDiskCacheLimit = value
            diskCacheLimitMB = value
        }
        function setExportSymbol(enabled) {
            exportSymbol = enabled
        }
        function setExportFootprint(enabled) {
            exportFootprint = enabled
        }
        function setExportModel3D(enabled) {
            exportModel3D = enabled
        }
        function setExportModel3DFormat(format) {
            exportModel3DFormat = format
        }
        function setExportModel3DPathMode(mode) {
            exportModel3DPathMode = mode
        }
        function setExportPreviewImages(enabled) {
            exportPreviewImages = enabled
        }
        function setExportDatasheet(enabled) {
            exportDatasheet = enabled
        }
        function setExportMode(mode) {
            exportMode = mode
        }
        function setSymbolLibraryDescription(desc) {
            symbolLibraryDescription = desc
        }
        function setFootprintLibraryDescription(desc) {
            footprintLibraryDescription = desc
        }
    }

    ExportSettingsCard {
        id: card
        width: parent.width
        exportSettingsController: controller
    }

    function findByObjectName(item, objectName) {
        if (item.objectName === objectName)
            return item

        for (var i = 0; i < item.children.length; ++i) {
            var child = findByObjectName(item.children[i], objectName)
            if (child)
                return child
        }

        return null
    }

    function diskCacheLimitInput() {
        var input = findByObjectName(card, "diskCacheLimitInput")
        verify(input !== null, "disk cache limit input should be discoverable")
        return input
    }

    function test_emptyDiskCacheLimitRestoresCurrentValue() {
        controller.diskCacheLimitMB = 5120
        controller.lastDiskCacheLimit = -1

        var input = diskCacheLimitInput()
        input.text = ""
        input.editingFinished()

        compare(input.text, "5120")
        compare(controller.lastDiskCacheLimit, -1)
    }

    function test_diskCacheLimitClampsRange() {
        var input = diskCacheLimitInput()
        input.text = "1048577"
        input.editingFinished()

        compare(input.text, "1048576")
        compare(controller.lastDiskCacheLimit, 1048576)
    }
}
