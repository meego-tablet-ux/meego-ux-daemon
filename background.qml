import  QtQuick 1.0
import  Qt 4.7
import  MeeGo.Labs.Components 0.1
import  MeeGo.Components 0.1 as Ux
import  MeeGo.Panels 0.1
import  MeeGo.Sharing 0.1
Window {
	id: scene
	fullscreen: true
	fullContent: true
	anchors.centerIn: parent
	Rectangle {
		id: background
		anchors.fill: parent
		color: "black"
		property variant backgroundImage: null
		BackgroundModel {
			id: backgroundModel
			Component.onCompleted: {
				background.backgroundImage = backgroundImageComponent.createObject(background);
			}
			onActiveWallpaperChanged: {
				background.backgroundImage.destroy();
				background.backgroundImage = backgroundImageComponent.createObject(background);
			}
		}
		Component {
			id: backgroundImageComponent
			Image {
				anchors.fill: parent
				asynchronous: true
				source: backgroundModel.activeWallpaper
				sourceSize.height:  background.height
				fillMode: Image.PreserveAspectCrop
			}
		}
	}
	StatusBar {
		id: sb
		anchors.top: parent.top
		width: parent.width
		height: theme_statusBarHeight
		active: scene.foreground
		backgroundOpacity: theme_panelStatusBarOpacity
	}
}

