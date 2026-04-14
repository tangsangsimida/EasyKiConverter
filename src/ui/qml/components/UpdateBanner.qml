import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: updateBanner
    property var updateChecker
    radius: AppStyle.radius.lg
    color: AppStyle.isDarkMode ? "#12243a" : "#e8f2ff"
    border.width: 1
    border.color: AppStyle.isDarkMode ? "#29507d" : "#9bc0ff"
    visible: updateChecker && updateChecker.hasUpdate && !updateChecker.dismissed
    implicitHeight: visible ? bannerLayout.implicitHeight + AppStyle.spacing.lg * 2 : 0
    Behavior on implicitHeight {
        NumberAnimation {
            duration: 220
            easing.type: Easing.OutCubic
        }
    }

    RowLayout {
        id: bannerLayout
        anchors.fill: parent
        anchors.margins: AppStyle.spacing.lg
        spacing: AppStyle.spacing.lg
        Rectangle {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: 18
            color: AppStyle.isDarkMode ? "#1e3b60" : "#d4e6ff"
            Text {
                anchors.centerIn: parent
                text: "i"
                color: AppStyle.colors.primary
                font.pixelSize: 18
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Text {
                text: qsTranslate("MainWindow", "发现新版本 %1").arg(updateChecker ? updateChecker.latestVersion : "")
                color: AppStyle.colors.textPrimary
                font.pixelSize: AppStyle.fontSizes.md
                font.bold: true
            }

            Text {
                text: updateChecker && updateChecker.releaseName && updateChecker.releaseName.length > 0 ? qsTranslate("MainWindow", "当前版本 %1，最新发布：%2").arg(updateChecker.currentVersion).arg(updateChecker.releaseName) : qsTranslate("MainWindow", "当前版本 %1，可前往 GitHub 查看发布说明。").arg(updateChecker ? updateChecker.currentVersion : "")
                color: AppStyle.colors.textSecondary
                font.pixelSize: AppStyle.fontSizes.sm
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        RowLayout {
            spacing: AppStyle.spacing.sm
            Button {
                text: qsTranslate("MainWindow", "查看更新")
                onClicked: {
                    if (updateChecker && updateChecker.releaseUrl) {
                        Qt.openUrlExternally(updateChecker.releaseUrl);
                    }
                }
            }

            Button {
                text: qsTranslate("MainWindow", "稍后提醒")
                flat: true
                onClicked: {
                    if (updateChecker) {
                        updateChecker.dismissUpdate();
                    }
                }
            }
        }
    }
}
