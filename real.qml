import QtQuick 1.0

Flickable { 
	anchors.top: parent.top;
	anchors.bottom: parent.bottom;
	anchors.fill: parent;
	clip:true;
	Image { 
		clip: true
		width: parent.width;
		height: parent.height;
		anchors.top: parent.top;
		anchors.bottom: parent.bottom;
		anchors.fill: parent;
		source: "image://gen/panel";
	 }
}
