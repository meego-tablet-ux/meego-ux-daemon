/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

import Qt 4.7
import MeeGo.Components 0.1
import MeeGo.Labs.Components 0.1 as Labs
import MeeGo.Settings 0.1
import MeeGo.Panels 0.1

Window {
    id: scene
    fullContent: true
    fullScreen: true

    lockOrientationIn: {
        if (qApp.foregroundOrientation == 1)
            "landscape";
        else if (qApp.foregroundOrientation == 2)
            "portrait";
        else if (qApp.foregroundOrientation == 3)
            "invertedLandscape";
        else
            "invertedPortrait";
    }

    Connections {
        target: mainWindow
        onActivateContent: {
            indicatorContainer.open = true;
        }
    }

    FuzzyDateTime {
        id: fuzzy
    }

    Component {
        id: notificationDelegateComponent
        NotificationDelegate {
            width: scene.content.width
            itemSummary: summary
            itemBody: body
            timestamp: fuzzy.getFuzzy(time)
            typeIcon: {
                if (eventType == "email.arrived" || eventType == "email")
                {
                    return "image://meegotheme/icons/launchers/meego-app-email";
                }
                else if (eventType == "im.recieved" || eventType == "im")
                {
                    return "image://meegotheme/icons/launchers/meego-app-im";
                }
                else
                {
                    return "image://meegotheme/icons/launchers/meego-app-widgets";
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    notificationModel.trigger(userId, notificationId);
                }
            }
        }
    }

    overlayItem: Item {
        id: notificationContainer
        width: parent.width
        height: parent.height

        // Expected by the WifiDialog we use right out of the
        // panels components
        Item {
            id: panelSize
            property int baseSize: parent.width
            property int oneHalf: Math.round(baseSize/2)
            property int oneThird: Math.round(baseSize/3)
            property int oneFourth: Math.round(baseSize/4)
            property int oneSixth: Math.round(baseSize/6)
            property int oneEigth: Math.round(baseSize/8)
            property int oneTenth: Math.round(baseSize/10)
            property int oneTwentieth: Math.round(baseSize/20)
            property int oneTwentyFifth: Math.round(baseSize/25)
            property int oneThirtieth: Math.round(baseSize/30)
            property int oneSixtieth: Math.round(baseSize/60)
            property int oneEightieth: Math.round(baseSize/80)
            property int oneHundredth: Math.round(baseSize/100)
        }

        VolumeDialog {
            id: volumeDialog
            visible: false
            z: 100
        }

        WifiDialog {
            id: wifiDialog
            visible: false
            z: 100
        }

        Rectangle {
            id: fog
            anchors.fill: parent
            color: theme_dialogFogColor
            opacity: theme_dialogFogOpacity
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    indicatorContainer.open = false;
                }
            }
        }

        // WiFiDialog expects this, so take the opportunity
        // to close the indicator menu
        Item {
            id: spinnerContainer
            function startSpinner() {
                indicatorContainer.open = false;
            }
            Labs.ApplicationsModel {
                    id: appsModel
                    directories: [ "/usr/share/meego-ux-appgrid/applications", "/usr/share/applications", "~/.local/share/applications" ]
            }
        }

        Item {
            id: indicatorContainer
            width: parent.width
            height: notificationsStatusBar.height + controls.height + banner.height + notificationsList.height + grabby.height

            property bool open: false
            Component.onCompleted: indicatorContainer.open = true

            BorderImage {
                id: background
                anchors.fill: parent
                source: "image://meegotheme/images/bg_application_p"
            }

            Item {
                id: controls
                anchors.top: parent.top
                anchors.topMargin: theme_statusBarHeight
                width: parent.width
                height: networksIcon.height * 2

                Image {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: -controls.width/4
                    id: networksIcon
                    source: "image://meegotheme/icons/settings/networks"
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var pos = parent.mapToItem(notificationContainer, mouse.x, mouse.y);
                            wifiDialog.dlgX = pos.x;
                            wifiDialog.dlgY = pos.y;
                            wifiDialog.visible = true;
                        }
                    }
                }
                Image {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: controls.width/4
                    id: volumeIcon
                    source: "image://meegotheme/icons/settings/sound"
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var pos = parent.mapToItem(notificationContainer, mouse.x, mouse.y);
                            volumeDialog.dlgX = pos.x;
                            volumeDialog.dlgY = pos.y;
                            volumeDialog.visible = true;
                        }
                    }
                }
            }

            Item {
                id: banner
                anchors.top: controls.bottom
                width: parent.width
                height: notificationsList.model.count == 0 ? 0 : clearButton.height + 20
                opacity: height > 0 ? 1.0 : 0.0

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: theme_fontColorNormal
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    height: parent.height
                    width: parent.width - clearButton.width - 20
                    font.pixelSize: theme_fontPixelSizeLarge
                    font.bold: true
                    color: theme_fontColorNormal
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: qsTr("Notifications")
                }
                Button {
                    id: clearButton
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Clear")
                    onClicked: {
                        qApp.clearAllNotifications();
                    }
                }
            }
            ListView {
                id: notificationsList
                anchors.top: banner.bottom
                width: parent.width

                // Something strange has started hapening with Qt 4.7.2,
                // where attempting to bind the height to
                // Math.min(contentHeight, maxHeight) fails because
                // contentHeight returns -1.  By catching the case
                // where the contentHeight is < 0 then I work around this.
                height: model.count == 0 ? 0 : Math.min(contentHeight < 0 ? currentItem.height : contentHeight, maxHeight)

                clip: true
                interactive: height < contentHeight
                property int maxHeight: scene.content.height - notificationsStatusBar.height - controls.height - banner.height - grabby.height

                Behavior on height {
                    NumberAnimation {
                        duration: 200;
                        easing.type: Easing.OutSine
                    }
                }

                model: notificationModel
                delegate: notificationDelegateComponent
            }
            Item {
                id: grabby
                anchors.top: notificationsList.bottom
                width: parent.width
                height: 20

                Image {
                    anchors.centerIn: parent
                    source: "image://meegotheme/widgets/common/notifications/grabby"
                }

                MouseArea {
                    anchors.fill: parent
                    property bool selected: false
                    onPressed: {
                        selected = true;
                    }
                    onPositionChanged: {
                        if (selected)
                        {
                            var pos = parent.mapToItem(notificationContainer, mouseX, mouseY);
                            var newY = pos.y - indicatorContainer.height;
                            if (newY < 0)
                            {
                                indicatorContainer.y = newY;
                            }
                            else
                            {
                                indicatorContainer.y = 0;
                            }
                        }
                    }
                    onReleased: {
                        selected = false;
                        if (indicatorContainer.y < 0)
                        {
                            indicatorContainer.open = false;
                        }
                    }
                }
            }

            StatusBar {
                id: notificationsStatusBar
                anchors.top: parent.top
                width: parent.width
                height: theme_statusBarHeight
                active: scene.foreground
                backgroundOpacity: theme_panelStatusBarOpacity
            }

            states: [
                State {
                    name: "opened"
                    when: indicatorContainer.open
                    PropertyChanges {
                        target: indicatorContainer
                        y: 0
                    }
                    PropertyChanges {
                        target: fog
                        opacity: theme_dialogFogOpacity
                    }

                },

                State {
                    name: "closed"
                    when: !indicatorContainer.open
                    PropertyChanges {
                        target: indicatorContainer
                        y: -indicatorContainer.height
                    }
                    PropertyChanges {
                        target: fog
                        opacity: 0.0
                    }
                }
            ]
            transitions: [
                Transition {
                    from: "*"
                    to: "opened"
                    PropertyAnimation {
                        properties: "y,opacity"
                        duration: 200
                        easing.type: Easing.OutSine
                    }
                },
                Transition {
                    from: "opened"
                    to: "closed"
                    SequentialAnimation {
                        PropertyAnimation {
                            properties: "y,opacity"
                            duration: 200
                            easing.type: Easing.OutSine
                        }
                        ScriptAction {
                            script: Qt.quit()
                        }
                    }
                }
            ]
        }
    }
}
