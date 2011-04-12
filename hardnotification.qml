/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

import Qt 4.7
import QtMobility.sensors 1.1
import MeeGo.Labs.Components 0.1

Window {
    id: scene
    transparent: true
    orientationLocked: true
    orientation: qApp.foregroundOrientation

    Component.onCompleted: {
        scene.fullscreen = true;
        scene.container.clip = false;
    }
    applicationPage: Component {
        ApplicationPage {
            id: page
            fullContent: true
            Rectangle {
                parent: page.content
                anchors.fill: parent
                anchors.margins: -scene.width
                color: theme_dialogFogColor
                opacity:  theme_dialogFogOpacity
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        Qt.quit();
                    }
                }
            }

            Text {
                id: switcherContent
                parent: page.content
                anchors.centerIn: parent
                text: notifyId + ": " + notifyMessage
            }
        }
    }
}
