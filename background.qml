import Qt 4.7
import MeeGo.Labs.Components 0.1 as Labs
import MeeGo.Components 0.1
import MeeGo.Panels 0.1

import MeeGo.Sharing 0.1
import MeeGo.Sharing.UI 0.1

Window {
    id: window
    fullContent: true
    anchors.centerIn: parent

    Theme {
        id: theme
    }

    Labs.BackgroundModel {
        id: backgroundModel
    }

    onOrientationChanged: {
        if (bgRect.bgImage1) {
                if (window.orientation & 1) {
                bgRect.bgImage2 = bgImageLandscape.createObject(bgRect);
                } else {
                bgRect.bgImage2 = bgImagePortrait.createObject(bgRect);
            }
            animBG2In.start();
        } else {
            if (window.orientation & 1) {
                bgRect.bgImage1 = bgImageLandscape.createObject(bgRect);
            } else {
                bgRect.bgImage1 = bgImagePortrait.createObject(bgRect);
            }
            animBG1In.start();
        }
    }

    Rectangle {
        id: bgRect
        anchors.fill: parent
        z: -1
        color: "black"
        property QtObject bgImage1: null
        property QtObject bgImage2: null

        //Get around the animations complaining about null objects
        Component.onCompleted: {
            bgImage2 = dummyImage.createObject(bgRect);
        }

        Component {
            id: dummyImage
            Image {
            }
        }


        Connections {
            target: bgRect.bgImage1
            onStatusChanged: {
            console.log("Image1 status changed:", bgRect.bgImage1.status);
            if (bgRect.bgImage1.status == Image.Ready) {
                console.log("Starting animBG1In");
                animBG1In.start();
            }
            }
        }

        Connections {
            target: bgRect.bgImage2
            onStatusChanged: {
            console.log("Image2 status changed:", bgRect.bgImage2.status);
            if (bgRect.bgImage2.status == Image.Ready) {
                console.log("Starting animBG2In");
                animBG2In.start();
            }
            }
        }


        SequentialAnimation {
            id: animBG1In
            ScriptAction {
            script: { console.log("Inside animBG1In, 1/2:", bgRect.bgImage1, bgRect.bgImage2); }
            }
            ParallelAnimation {
            NumberAnimation {
                target: bgRect.bgImage1
                property: "opacity"
                to: 1.0
                duration: 500
            }
            NumberAnimation {
                target: bgRect.bgImage2
                property: "opacity"
                to: 0
                duration: 500
            }
            }
            ScriptAction {
            script: { bgRect.bgImage2.destroy(); bgRect.bgImage2 = null; }
            }
        }

        SequentialAnimation {
            id: animBG2In
            ScriptAction {
            script: { console.log("Inside animBG2In, 1/2:", bgRect.bgImage1, bgRect.bgImage2); }
            }
            ParallelAnimation {
            NumberAnimation {
                target: bgRect.bgImage2
                property: "opacity"
                to: 1.0
                duration: 500
            }
            NumberAnimation {
                target: bgRect.bgImage1
                property: "opacity"
                to: 0
                duration: 500
            }
            }
            ScriptAction {
            script: { bgRect.bgImage1.destroy(); bgRect.bgImage1 = null; }
            }
        }


        Component {
            id: bgImageLandscape
            Image {
            id: bgImageLPrivate
            anchors.fill: parent
            asynchronous: true
            source: backgroundModel.activeWallpaper
            fillMode: Image.PreserveAspectCrop
            opacity: 0.0

            Component.onCompleted: {
                sourceSize.height = bgRect.height;
                rotation = (window.orientation == 1) ? 0 : 180;
            }
            }
        }

        Component {
            id: bgImagePortrait
            Image {
            id: bgImagePPrivate
            asynchronous: true
            source: backgroundModel.activeWallpaper
            opacity: 0.0
            anchors.fill: parent
            height: bgRect.width
            width: bgRect.height
            fillMode: Image.PreserveAspectCrop

            Component.onCompleted: {
                sourceSize.height = bgRect.width;
                rotation = (window.orientation == 2) ? 270 : 90;
            }
            }
        }
    }



    Item {
        id: background
        anchors.fill: parent
//        color: "black"
        property variant backgroundImage: null
        Image {
        id: backgroundImage
        anchors.fill: parent
        asynchronous: true
        source: backgroundModel.activeWallpaper
        fillMode: Image.PreserveAspectCrop
        Component.onCompleted: {
        sourceSize.height = background.height;
        }
        }
        Component {
            id: backgroundImageComponent
            Image {
            //anchors.centerIn: parent
            anchors.fill: parent
            asynchronous: true
            source: backgroundModel.activeWallpaper
            sourceSize.height: background.height
            fillMode: Image.PreserveAspectCrop
            }
        }
    }

    StatusBar {
        anchors.top: parent.top
        width: parent.width
        height: theme.statusBarHeight
        active: window.isActiveWindow
        backgroundOpacity: theme.panelStatusBarOpacity
    }
}

