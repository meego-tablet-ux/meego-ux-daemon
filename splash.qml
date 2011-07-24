import Qt 4.7
import QtQuick 1.0
import MeeGo.Components 0.1
import MeeGo.Labs.Components 0.1 as Labs

Item {
    width: screenWidth
    height: screenHeight

    property alias appItem: appLauncherAppLoader.item

    Loader {
        id: appLauncherAppLoader

        // Once we have finished loading the app source then
        // remove the splash content
        onLoaded: {
            if (status == Loader.Ready)
            {
                appLauncherSplashLoader.sourceComponent = undefined;
            }
        }
    }

    Loader {
        id: appLauncherSplashLoader
    }

    Component {
        id:  appLauncherSplashComponent
        Rectangle {
            width: screenWidth
            height: screenHeight
            color: "black"
            Timer {
                id: loadTimer
                interval: 5000
                onTriggered: {
                    appLauncherAppLoader.source = mainWindow.appSource;
                }
            }
            Component.onCompleted: loadTimer.start();
            Connections {
                target: qApp
                onForegroundWindowChanged: {
                    if (qApp.foregroundWindow == mainWindow.winId)
                    {
                        if (appLauncherAppLoader.source == "")
                            appLauncherAppLoader.source = mainWindow.appSource;
                    }
                }
            }
            Image {
                anchors.centerIn: parent
                source: mainWindow.splashImage
            }
        }
    }
    Component.onCompleted: {
        appLauncherSplashLoader.sourceComponent = appLauncherSplashComponent;
    }
}
