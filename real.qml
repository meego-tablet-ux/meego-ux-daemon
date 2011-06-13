import QtQuick 1.0

Flickable { 
	anchors.top: parent.top;
	anchors.bottom: parent.bottom;
	anchors.fill: parent;
//	clip:true;
//	Image { 
//		clip: true
//		width: parent.width;
//		height: parent.height;
//		anchors.top: parent.top;
//		anchors.bottom: parent.bottom;
//		anchors.fill: parent;
	Image { 
		x:	0;
		y:	0;
		source: "image://gen/0";
	 }

	Image { 
		width: 1024;
		height: 400;
		x:	1024;
		y:	0;
		source: "image://gen/1";
	 }

	Image { 
		width: 1024;
		height: 400;
		x:	2048;
		y:	0;
		source: "image://gen/2";
	 }

	Image { 
		width: 1024;
		height: 400;
		x:	3072;
		y:	0;
		source: "image://gen/3";
	 }

	Image { 
		width: 1024;
		height: 400;
		x:	0;
		y:	400;
		source: "image://gen/4";
	 }

	Image { 
		width: 1024;
		height: 400;
		x:	1024;
		y:	400;
		source: "image://gen/5";
	 }

	Image { 
		width: 1024;
		height: 400;
		x:	2048;
		y:	400;
		source: "image://gen/6";
	 }

	Image { 
		width: 1024;
		height: 400;
		x:	3072;
		y:	400;
		source: "image://gen/7";
	 }

}
