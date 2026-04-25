import QtQuick
import QtTest
import "../../src/ui/qml/components"
import "../../src/ui/qml/styles"

TestCase {
    name: "CardCollapse"
    width: 400
    height: 400
    visible: true
    when: windowShown

    Card {
        id: testCard
        title: "Test Card"
        collapsible: true
        isCollapsed: false
        anchors.centerIn: parent
        width: 300
    }

    function test_initialState() {
        compare(testCard.isCollapsed, false, "Card should be expanded initially")
        compare(testCard.collapsible, true, "Card should be collapsible")
    }

    function test_collapseToggle() {
        // Simulate clicking the header area to collapse
        testCard.isCollapsed = true
        compare(testCard.isCollapsed, true, "Card should collapse after toggle")

        testCard.isCollapsed = false
        compare(testCard.isCollapsed, false, "Card should expand after second toggle")
    }

    function test_nonCollapsibleCard() {
        var nonCollapsibleCard = createTemporaryQmlObject(
            'import QtQuick; import EasyKiconverter_Cpp_Version.src.ui.qml.components 1.0; Card { collapsible: false; title: "Fixed" }',
            testCard
        )
        compare(nonCollapsibleCard.collapsible, false, "Card should not be collapsible")
    }
}
