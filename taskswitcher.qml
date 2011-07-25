/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

import Qt 4.7
import QtMobility.sensors 1.1
import MeeGo.Components 0.1
import MeeGo.Labs.Components 0.1 as Labs

Window {
    id: window
    fullScreen: true
    orientationLock: qApp.foregroundOrientation
    blockOrientationWhenInactive: false

    property string closeText: qsTr("Close")
    property string openText: qsTr("Open")

    Labs.FavoriteApplicationsModel {
        id: favorites
    }

    overlayItem: Item {
        id: page
        anchors.fill: parent
        Rectangle {
            anchors.fill: parent
            anchors.margins: -window.width
            color: theme_dialogFogColor
            opacity:  theme_dialogFogOpacity
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    Qt.quit();
                }
            }
        }

        Item {
            id: switcherContent
            anchors.centerIn: parent
            width: switcherContent.iconContainerWidth * switcherContent.gridColumns + dialogMargin * 2
            height: (switcherContent.iconContainerHeight + theme_iconFontPixelSize + theme_iconTextPadding * 2) * switcherContent.gridRows + switcherContent.iconContainerHeight + dialogMargin * 2
            Behavior on height {
                PropertyAnimation { duration: 100; }
            }

            property int iconTextPadding: theme_iconTextPadding
            property int iconWidth: theme_taskSwitcherIconWidth
            property int iconHeight: theme_taskSwitcherIconHeight
            property int iconContainerWidth: theme_taskSwitcherIconContainerWidth
            property int iconContainerHeight: theme_taskSwitcherIconContainerHeight
            property int gridRows: calculateNumRows()
            property int gridColumns: theme_taskSwitcherNumCols

            property int dialogMargin: theme_taskSwitcherDialogMargin

            property int horizontalSeperatorThickness: theme_taskSwitcherHorizontalSeperatorThickness
            property int horizontalSeperatorMargin: theme_taskSwitcherHorizontalSeperatorMargin
            property color horizontalSeperatorColor: theme_taskSwitcherHorizontalSeperatorColor

            property int verticalSeperatorThickness: theme_taskSwitcherVerticalSeperatorThickness
            property int verticalSeperatorMargin: theme_taskSwitcherVerticalSeperatorMargin
            property color verticalSeperatorColor: theme_taskSwitcherVerticalSeperatorColor

            function calculateNumRows() {
                var appCount = qApp.runningApps.length;

                if (appCount == 0)
                    return 0;

                if (appCount <= theme_taskSwitcherNumCols)
                    return 1;

                if (appCount >= theme_taskSwitcherNumCols * theme_taskSwitcherNumRows)
                    return theme_taskSwitcherNumRows;

                return Math.ceil(appCount / theme_taskSwitcherNumCols);
            }


            BorderImage {
                source: "image://themedimage/images/notificationBox_bg"
                anchors.fill: parent
                border.left: 5
                border.top: 5
                border.right: 5
                border.bottom: 5
            }

            Row {
                id: fixedButtonsContainer
                anchors.top:  parent.top
                anchors.topMargin: switcherContent.dialogMargin
                anchors.left: parent.left
                anchors.leftMargin: (parent.width - switcherContent.iconContainerWidth * 2 - switcherContent.verticalSeperatorThickness)/4
                spacing: anchors.leftMargin
                Item {
                    id: panelsButton
                    width: switcherContent.iconContainerWidth
                    height: switcherContent.iconContainerHeight
                    Image {
                        anchors.centerIn: parent
                        source: "image://systemicon/meego-app-panels"
                        onSourceSizeChanged: {
                            if (sourceSize.width > switcherContent.iconWidth || sourceSize.height > switcherContent.iconHeight)
                            {
                                width = switcherContent.iconWidth;
                                height = switcherContent.iconHeight;
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            qApp.showPanels();
                            Qt.quit();
                        }
                    }
                }
                Item {
                    width: switcherContent.verticalSeperatorThickness
                    height: switcherContent.iconContainerHeight
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width
                        height: switcherContent.iconContainerHeight - switcherContent.verticalSeperatorMargin * 2
                        color: switcherContent.verticalSeperatorColor
                    }
                }
                Item {
                    id: homeButton
                    width: switcherContent.iconContainerWidth
                    height: switcherContent.iconContainerHeight
                    Image {
                        anchors.centerIn: parent
                        source: "image://systemicon/meego-app-grid"
                        onSourceSizeChanged: {
                            if (sourceSize.width > switcherContent.iconWidth || sourceSize.height > switcherContent.iconHeight)
                            {
                                width = switcherContent.iconWidth;
                                height = switcherContent.iconHeight;
                            }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            qApp.showGrid();
                            Qt.quit();
                        }
                    }
                }
            }
            Rectangle {
                id: seperator
                anchors.top: fixedButtonsContainer.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - switcherContent.horizontalSeperatorMargin * 2 - switcherContent.dialogMargin * 2
                height: qApp.runningApps.length > 0 ? switcherContent.horizontalSeperatorThickness : 0
                color: switcherContent.horizontalSeperatorColor
            }
            Grid {
                anchors.top: seperator.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: switcherContent.iconContainerWidth * switcherContent.gridColumns
                height: parent.height - (seperator.y + seperator.height)
                rows: switcherContent.gridRows
                columns: switcherContent.gridColumns
                Component.onCompleted: {
                    qApp.runningAppsLimit = theme_taskSwitcherNumRows * theme_taskSwitcherNumCols;
                }
                Repeater {
                    model: qApp.runningApps
                    Item {
                        id: dinstance
                        width: switcherContent.iconContainerWidth
                        height: switcherContent.iconContainerHeight + theme_iconFontPixelSize
                        opacity: 1.0
                        Behavior on opacity {
                            PropertyAnimation { duration: 100; }
                        }
                        Image {
                            id: iconItem
                            anchors.centerIn: parent
                            source: modelData.icon
                            onSourceSizeChanged: {
                                if (sourceSize.width > switcherContent.iconWidth || sourceSize.height > switcherContent.iconHeight)
                                {
                                    width = switcherContent.iconWidth;
                                    height = switcherContent.iconHeight;
                                }
                            }
                        }
                        Rectangle {
                            anchors.verticalCenter: labelItem.verticalCenter
                            anchors.horizontalCenter: parent.horizontalCenter
                            height: labelItem.height + switcherContent.iconTextPadding * 2
                            width: labelItem.paintedWidth + switcherContent.iconTextPadding * 2
                            color: theme_iconFontBackgroundColor
                            radius: 5
                            opacity: 0.5
                        }
                        Text {
                            id: labelItem
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 5
                            anchors.horizontalCenter: parent.horizontalCenter
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            color: theme_iconFontColor
                            font.pixelSize: theme_iconFontPixelSize
                            font.bold: true
                            smooth: true
                            text: modelData.title
                            Component.onCompleted: {
                                if (labelItem.paintedWidth > parent.width - 30)
                                {
                                    labelItem.width = parent.width - 30;
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                favorites.append(modelData.filename);
                                qApp.launchDesktopByName(modelData.filename);
                                Qt.quit();
                            }
                            onPressAndHold:{
                                var map = mapToItem(page, mouseX, mouseY);
                                contextMenu.setPosition(map.x, map.y + 10);
                                contextMenu.show();
                            }
                        }
                        ContextMenu {
                            id: contextMenu

                            // Attempting to do any operation the deletes our
                            // parent while the contextmenu is animating away
                            // will result in stutter and in somecases deadlock
                            Timer {
                                id: actionTimer
                                interval: 1
                                onTriggered: {
                                    if (contextMenu.quitOnContextMenuClose)
                                    {
                                        favorites.append(modelData.filename);
                                        qApp.launchDesktopByName(modelData.filename);
                                        Qt.quit();
                                    }
                                    else
                                    {
                                        qApp.closeDesktopByName(modelData.filename);
                                    }
                                }
                            }
                            property bool quitOnContextMenuClose: false
                            // Adding another very short delay will yield
                            // control and ensure we have completely finished
                            // cleanup
                            onClosed: actionTimer.start()

                            content: ActionMenu {
                                model: [window.openText, window.closeText]
                                payload: [0, 1]
                                onTriggered: {
                                    dinstance.opacity = 0.0;
                                    if (index == 0)
                                    {
                                        contextMenu.quitOnContextMenuClose = true;
                                    }
                                    contextMenu.hide();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
