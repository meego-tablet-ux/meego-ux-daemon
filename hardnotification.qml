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
        title: ""
        cancelButtonText: qsTr("Decline")
        acceptButtonText: qsTr("Accept")
   
        showCancelButton: true
        showAcceptButton: true

        property string body: ""
        property string uId: ""
        property string notificationId: ""
        property string imageUri: ""
	property string snoozeButtonTxt: qsTr("Snooze")

        Connections {
            target: qApp
            onHardNotification: {

                dialog.title = subject;
                dialog.body = body;
                dialog.uId = userId;
                dialog.notificationId = notificationId;
                dialog.imageUri = imageURI
                dialog.show();

            }
        }

        content:Row{
            id: contentRow
            width: dialog.width - 40
            height: iconImage.height + bodyText.paintedHeight
            spacing: 20
            anchors.centerIn: parent

            Image {
                id: iconImage
                source: dialog.imageUri
                height: 70
                width: 70
            }

            Text {
                id: bodyText
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - iconImage.width - 20
                wrapMode: Text.Wrap
                text: dialog.body
            }
        }

        onAccepted: {

            notificationModel.trigger(dialog.uId, dialog.notificationId);
            Qt.quit();
        }

        onRejected: {

            notificationModel.triggerDeclineAction(dialog.uId, dialog.notificationId);
            Qt.quit();
        }
    }
}
