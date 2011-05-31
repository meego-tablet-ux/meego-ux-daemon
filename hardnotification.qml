/*
* Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

import Qt 4.7
import MeeGo.Components 0.1

Window {
    id: window
    fullScreen: true
    orientationLock: qApp.foregroundOrientation
    blockOrientationWhenInactive: false

    overlayItem:  ModalDialog {
        id: dialog
        title: qsTr("")
        cancelButtonText: qsTr("Decline")
        acceptButtonText: qsTr("Accept")
        showCancelButton: true
        showAcceptButton: true

        property string body: ""
        property string remoteAction: ""
        property string uId: ""
        property string notificationId: ""

        Connections {
            target: qApp
            onHardNotification: {

                dialog.title = subject;
                dialog.body = body;
                dialog.remoteAction = remoteAction;
                dialog.uId = userId;
                dialog.notificationId = notificationId;
                dialog.show();

            }
        }

        content: Text {
            anchors.centerIn: parent
            width: parent.width - 20
            wrapMode: Text.Wrap
            text: dialog.body
        }

        onAccepted: {

            notificationModel.trigger(dialog.uId, dialog.notificationId);
            Qt.quit();
        }
        onRejected: {

            notificationModel.deleteNotification(dialog.uId, dialog.notificationId);
            Qt.quit();
        }
    }
}
