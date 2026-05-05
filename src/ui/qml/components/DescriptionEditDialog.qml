import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../styles"

/**
 * @brief 元器件描述编辑对话框
 *
 * 使用与退出确认对话框一致的滑块按钮和遮罩样式。
 */
SliderDialogBase {
    id: root
    hasOverlay: true
    title: qsTr("元器件描述")
    message: qsTr("编辑当前元器件导出到符号和封装中的描述。")

    property string descriptionText: ""

    signal accepted(string description)
    signal rejected

    mainContentSource: ColumnLayout {
        spacing: AppStyle.spacing.sm

        TextArea {
            id: descriptionInput
            Layout.fillWidth: true
            Layout.preferredHeight: 128
            text: root.descriptionText
            wrapMode: TextEdit.Wrap
            selectByMouse: true
            font.pixelSize: AppStyle.fontSizes.sm
            color: AppStyle.colors.textPrimary
            placeholderText: qsTr("留空则不导出描述")
            onTextChanged: root.descriptionText = text
            Component.onCompleted: Qt.callLater(forceActiveFocus)

            background: Rectangle {
                color: AppStyle.colors.surface
                border.color: descriptionInput.activeFocus ? AppStyle.colors.borderFocus : AppStyle.colors.border
                border.width: descriptionInput.activeFocus ? 2 : 1
                radius: AppStyle.radius.md
            }
        }
    }

    buttonSpecs: [
        {
            text: qsTr("取消"),
            color: AppStyle.colors.textSecondary,
            action: function () {
                root.rejected();
                root.closeWithAnimation();
            }
        },
        {
            isSeparator: true
        },
        {
            text: qsTr("保存"),
            color: AppStyle.colors.primary,
            action: function () {
                root.accepted(root.descriptionText);
                root.close();
            }
        }
    ]

    Keys.onEscapePressed: {
        root.rejected();
        root.closeWithAnimation();
    }

    function open() {
        visible = true;
        root.dialogBox.y = 0;
        root.dialogBoxTranslate.y = 0;
        root.showAnimation.start();
        root.forceActiveFocus();

        var childIndex = 0;
        for (var i = 0; i < buttonSpecs.length; i++) {
            if (buttonSpecs[i].isSeparator) {
                continue;
            }
            var loader = buttonColLayout.children[childIndex];
            if (loader && loader.actualButton) {
                root.updateSlider(loader.actualButton, AppStyle.colors.textSecondary);
                break;
            }
            childIndex++;
        }
    }
}
