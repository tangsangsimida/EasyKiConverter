
import QtQuick 2.15
import QtTest 1.15
import "../../src/ui/qml/components"

TestCase {
    name: "BasicComponents"
    width: 400
    height: 400
    visible: true
    when: windowShown

    ModernButton {
        id: testButton
        text: "Click Me"
        anchors.centerIn: parent
    }

    function test_buttonProperties() {
        compare(testButton.text, "Click Me", "Button text should be correct")
        verify(testButton.implicitWidth > 0, "Button implicitWidth should be positive")
    }

    function test_buttonClick() {
        var clickedSignalReceived = false
        testButton.clicked.connect(function() { clickedSignalReceived = true })

        mouseClick(testButton)
        verify(clickedSignalReceived, "Button should emit clicked signal")
    }
}
