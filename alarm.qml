/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

import Qt 4.7
import QtMultimediaKit 1.1
import MeeGo.Components 0.1

Window {
    id: window
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

    overlayItem: Item {
        id: page
        anchors.fill: parent

        Rectangle {
            anchors.fill: parent
            anchors.margins: -page.width
            color: theme_dialogFogColor
            opacity:  theme_dialogFogOpacity
        }

        ModalDialog {
            id: dialog
            title: qsTr("Alarm")
            cancelButtonText: qsTr("Snooze")

            property int alarmId: 0
            property string message: ""
            property bool showSnoozeButton: false
            property string uri: ""

            Connections {
                target: qApp
                onAlarm: {
                    dialog.alarmId = alarmId;
                    dialog.title = title;
                    dialog.message = message;
                    dialog.showCancelButton = snooze;
                    dialog.uri = soundUri;
                    dialog.show();
                    audio.play();
                }
            }

            Audio {
                id: audio
                autoLoad: true
                source: dialog.uri
            }

            content: Text {
                text: dialog.message
            }

            onAccepted: {
                if (dialog.uri != "")
                {
                    // unload the alarm sound
                }
                if (dialog.showCancelButton)
                {
                    qApp.stopSnooze(dialog.alarmId);
                }
                audio.stop();
                dialog.hide();
                Qt.quit();
            }
            onRejected: {
                if (dialog.uri != "")
                {
                    // unload the alarm sound
                }
                audio.stop();
                dialog.hide();
                Qt.quit();
            }
        }
    }
}
