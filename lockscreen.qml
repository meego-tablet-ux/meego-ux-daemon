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

Window {
    id: window
    fullScreen: true
    lockOrientationIn: "landscape"
    Labs.BackgroundModel {
        id: backgroundModel
    }

    function pickIcon (event, imgUri){

        switch (event)
        {
        case "x-nokia.call" :
                return "image://meegotheme/icons/oobe/phone-unavailable";
            break;
        case "im" :
                return "image://meegotheme/icons/oobe/chat-unavailable";
            break;
        case "email.arrived" :
                return "image://meegotheme/icons/oobe/mail-unavailable";
            break;
        case "transfer.complete":
                return "image://meegotheme/icons/internal/notifications-download";
            break;
        case "device.added":
                return "image://meegotheme/icons/internal/notifications-bluetooth-selected"
            break;

        default:
                return "image://meegotheme/icons/toolbar/list-add"
        }
    }

    Component {
        id: lockscreenNotificationDelegate
        Item {
            Rectangle {
                id: notificationBox                
                height: childrenRect.height + 30
                width: childrenRect.width + 30
                color: "white"
                radius: 20
                smooth: true

                Image {
                    id: iconImage
                    anchors.centerIn: parent
                    height: 70
                    width: 70
                    source: pickIcon(eventType,imageURI)
                }
            }

            Rectangle{
                id: multipleCircle
                width: 40
                height: 40
                radius: 50
                color: "white"
                anchors.verticalCenter: notificationBox.top
                anchors.horizontalCenter: notificationBox.right
                smooth: true

                Text{
                    id: multipleNum
                    anchors.centerIn: parent
                    style: Text.Raised
                    styleColor: "gray"
                    text: count > 1 ? count : "1"
                }
            }
        }
    }

    overlayItem: Item {
        id: container
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            color: "black"
        }
        Image {
            anchors.fill: parent
            asynchronous: true
            sourceSize.height: height
            source: backgroundModel.activeWallpaper
            fillMode: Image.PreserveAspectCrop
        }
 
        Item {
            id: mainContent
            width: parent.width
            height: parent.height
            x: 0
            y: 0
            opacity: qApp.screenOn ? 1.0 : 0.0
            Behavior on opacity {
                PropertyAnimation { duration: 250; }
            }

            property bool animateAway: false

            StatusBar {
                anchors.top: parent.top
                width: parent.width
                height: theme_statusBarHeight
                active: window.isActiveWindow
                showClock: false
                backgroundOpacity: theme_panelStatusBarOpacity
                MouseArea {
                    anchors.fill: parent
                    // disable activating the statusindicatormenu
                }
            }

            Item {
                id: dateTimeItem
                anchors.top: parent.top
                anchors.topMargin: height
                width: parent.width
                height: 150
                Rectangle {
                    anchors.fill: parent
                    color: theme_lockscreenShapeTimeColor
                    opacity: theme_lockscreenShapeTimeOpacity
                }
                LocalTime {
                    id: localTime
                    interval: qApp.screenOn ? 60000 : 0
                }
                Text {
                    id: timeText
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -dateText.height
                    height: font.pixelSize
                    text: localTime.shortTime
                    horizontalAlignment: Text.AlignHCenter
                    smooth: true
                    font.pixelSize: theme_lockscreenTimeFontPixelSize
                    font.bold: true
                    color: theme_lockscreenTimeFontColor
                    style: Text.Raised
                    styleColor: theme_lockscreenTimeFontDropshadowColor
                }
                Text {
                    id: dateText
                    anchors.top: timeText.bottom
                    anchors.topMargin: height
                    anchors.horizontalCenter: parent.horizontalCenter
                    height: font.pixelSize
                    text: localTime.longDate
                    horizontalAlignment: Text.AlignHCenter
                    smooth: true
                    font.pixelSize: theme_lockscreenDateFontPixelSize
                    font.bold: true
                    color: theme_lockscreenDateFontColor
                    style: Text.Raised
                    styleColor: theme_lockscreenDateFontDropshadowColor
                }
            }
            GridView {
                id: notificationsList
                interactive: false
		anchors.bottom: lockbutton.top
                anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottomMargin: 20
                width: parent.width - 40
                cellWidth: 140
                cellHeight: 140
                height: childrenRect.height
                model: notificationModel
                delegate: lockscreenNotificationDelegate
            }

            BorderImage {
                id: lockbutton
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                height: 86

                property bool pressed: false
                property int startMouseY
                source: pressed ? "image://meegotheme/widgets/apps/lockscreen/lockscreen-unlockbar-active" : "image://meegotheme/widgets/apps/lockscreen/lockscreen-unlockbar"

                Image {
                    id: pointerIcon
                    anchors.bottom: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: "image://meegotheme/widgets/apps/lockscreen/lockscreen-direction"
                }

                Image {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: lockbutton.pressed ? "image://meegotheme/widgets/apps/lockscreen/lockscreen-lock-active" : "image://meegotheme/widgets/apps/lockscreen/lockscreen-lock"
                }
                MouseArea {
                    anchors.fill: parent
                    onPressed: {
                        lockbutton.startMouseY = mouseY;
                        lockbutton.pressed = true;
                    }
                    onPositionChanged: {
                        if (lockbutton.pressed)
                        {
                            var pos = parent.mapToItem(container, mouseX, mouseY);
                            mainContent.y = pos.y - mainContent.height;
                            if ((littleBlueDot.y - mainContent.height) > mainContent.y - (lockbutton.height + 30))
                                mainContent.animateAway = true;
                        }
                    }

                    onReleased: {
                        lockbutton.pressed = false;
                        if (!mainContent.animateAway)
                            mainContent.y = 0
                    }
                }
            }

            states: State {
                name: "closing"
                when:  mainContent.animateAway
                PropertyChanges {
                    target: mainContent
                    y: -mainContent.height
                }
            }
            transitions: Transition {
                from: "*"
                to: "closing"
                animations: SequentialAnimation {
                    PropertyAnimation {
                        properties: "y"
                        easing.type: Easing.OutSine
                        duration: 200
                    }
                    ScriptAction {
                        script:  Qt.quit()
                    }
                }
            }
        }

        Image {
            id: littleBlueDot
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: parent.height/4
            visible: lockbutton.pressed
            source: "image://meegotheme/widgets/apps/lockscreen/lockscreen-mark-unlock"
        }
    }
}
