import QtQuick 1.0

Flickable { 
	width: 1024;
	height: 600;
	contentWidth:4200;
	contentHeight: 600;
	Image { 
		width: 4200;
		height: 600;
		anchors.top: parent.top;
		anchors.bottom: parent.bottom;
		source: "image://gen/panel";
	 }
}
