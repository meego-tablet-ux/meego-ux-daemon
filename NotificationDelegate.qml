/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

import Qt 4.7
import MeeGo.Labs.Components 0.1

Item {
    id: itemContainer
    width: parent.width
    height: contentHeight()
    anchors.left: parent.left

    property alias itemSummary: summaryText.text
    property alias itemBody: contentText.text
    property alias timestamp: timestampText.text
    property alias typeIcon: notificationIcon.source

    function contentHeight() {
        return Math.max(timestampText.y + timestampText.height + timestampText.anchors.bottomMargin,
                        contentText.y + contentText.height);
    }

    BorderImage {
        anchors.fill: parent
        source: "image://meegotheme/widgets/common/notifications/notification_background"
    }

    Item {
        id: panelSize
        property int baseSize: itemContainer.width
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

    Image {
        id: notificationIcon
        source: "image://meegotheme/icons/launchers/meego-app-widgets"
        anchors.top: parent.top
        anchors.topMargin: panelSize.oneThirtieth
        anchors.left: parent.left
        anchors.leftMargin: panelSize.oneThirtieth
        fillMode: Image.PreserveAspectFit
        asynchronous: true
    }

    Text {
        id: summaryText
        anchors.right: parent.right
        anchors.rightMargin: panelSize.oneThirtieth
        anchors.top: notificationIcon.top
        anchors.left: notificationIcon.right
        anchors.leftMargin: panelSize.oneThirtieth
        color: theme_buttonFontColor
        font.bold: true
        font.pixelSize: theme_fontPixelSizeLarge
        wrapMode: Text.NoWrap
        elide: Text.ElideRight
    }

    Text {
        id: contentText
        width: summaryText.width
        anchors.top: summaryText.bottom
        anchors.topMargin: panelSize.oneSixtieth
        anchors.left: summaryText.left
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        font.pixelSize: theme_fontPixelSizeLarge
        color: theme_buttonFontColor
    }

    Text {
        id: timestampText
        width: contentText.width
        anchors.top: notificationIcon.bottom
        anchors.left: parent.left
        anchors.leftMargin: notificationIcon.width/2
        anchors.bottomMargin: 20
        font.pixelSize: theme_fontPixelSizeNormal
        color:theme_fontColorInactive
        wrapMode: Text.NoWrap
        elide: Text.ElideRight
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 1
        width: parent.width
        height: 1
        color: "black"
    }
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: theme_fontColorNormal
    }
}
