/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

import Qt 4.7
import MeeGo.Labs.Components 0.1

Window {
    id: scene
    transparent: true
    fullscreen: true
    fullContent: true
    showtoolbar: false
    orientationLocked: true
    orientation: qApp.preferredPortraitOrientation

    BackgroundModel {
        id: backgroundModel
    }

    function initializeModelFilters()
    {
        notificationModel.addFilter("im.received");
        notificationModel.addFilter("phone.missed");
        notificationModel.refreshViewableList();
        notificationsList.currentIndex = 0;
    }

    Component {
        id: lockscreenNotificationDelegate

        Image{
            id: notificationBox
            source: "image://meegotheme/images/notificationBox_bg"
            height: childrenRect.height + 30
            width: childrenRect.width + 30

            Image {
                id: iconImage
                anchors.centerIn: parent
                height: 70
                width: 70
                source: imageURI != "" ? imageURI : "image://meegotheme/icons/launchers/meego-app-widgets" 
            }

            Text{
                id: multipleNum
                width: 70
                text: count > 1 ? "x " + count : ""   	
            }
        }
    }
    Item {
        id: mainContent
        parent: scene.content
        width: parent.width
        height: parent.height
        x: 0
        y: 0

        property bool animateAway: false
        Component.onCompleted: initializeModelFilters()
 	Component.onDestruction: notificationModel.clearFilters()	
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
        StatusBar {
            anchors.top: parent.top
            width: parent.width
            height: theme_statusBarHeight
            active: scene.foreground
            showClock: false
            backgroundOpacity: theme_panelStatusBarOpacity
            MouseArea {
                anchors.fill: parent
                // disable activating the statusindicatormenu
            }
        }

        LocalTime {
            id: localTime
            interval: 60000
        }
        Item {
	    id: dateTimeItem
            anchors.top: parent.top
            anchors.topMargin: height
            width: parent.width
            height: parent.height/4
            Text {
                id: timeText
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -dateText.height * 2
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
            anchors.top: dateTimeItem.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 40
            model: notificationModel
            cellWidth: 140
            cellHeight: 140

            height: 400

            delegate: lockscreenNotificationDelegate
        }

        Image {
            id: lockbutton
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            property bool pressed: false
            property int startMouseY
            source: pressed ? "image://meegotheme/widgets/apps/lockscreen/lock-button-active" : "image://meegotheme/widgets/apps/lockscreen/lock-button"
            Image {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                source: lockbutton.pressed ? "image://meegotheme/widgets/apps/lockscreen/lock-active" : "image://meegotheme/widgets/apps/lockscreen/lock"
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
                        var pos = parent.mapToItem(scene.content, mouseX, mouseY);
                        mainContent.y = pos.y - mainContent.height;
                    }
                }

                onReleased: {
                    lockbutton.pressed = false;
                    if (mainContent.y < -10)
                        mainContent.animateAway = true;
                    else
                        mainContent.y = 0;
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
}
