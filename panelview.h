#ifndef _PANELVIEW_H
#define _PANELVIEW_H 

#include<QString>
#include<QSize>
#include<QUrl>

#include<QPixmap>
#include<QBitmap>
#include<QPainter>
#include<QGraphicsView>
#include<QGraphicsScene>
#include<QWidget>
#include<QDesktopWidget>


#include<QGLFramebufferObject>

#include<QPaintEvent>
#include<QKeyEvent>
#include<QMouseEvent>
#include<QTabletEvent>

#include<QDeclarativeEngine>
#include<QDeclarativeContext>
#include<QDeclarativeImageProvider>
#include<QDeclarativeView>
#include<QDeclarativeItem>

#include<QApplication>

#include"dialog.h"

#define NUM_P 8
#define NUM_C 4 
#define NUM_R 2

#define ISRC "image://gen/"
#define ISRC_LEN strlen(ISRC)


class PMonitor;
class PanelView;

class PMonitor : public Dialog {
	Q_OBJECT
public:
	PMonitor(void);
	friend class PanelView;
};


class PanelView : public Dialog,
		  public QDeclarativeImageProvider {
	Q_OBJECT
public:
	PanelView(void);
	~PanelView(void);

	void keyPressEvent(QKeyEvent *);
	void keyReleaseEvent(QKeyEvent *);
	void mouseDoubleClickEvent(QMouseEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void tabletEvent(QTabletEvent *);
	
	QImage  requestImage(const QString &, QSize *, const QSize &);

public slots:
	void invalidate(const QList<QRectF> &);
	void bg_changed(void);
private:
	void create_bg(void);
	inline void draw_single(int); 

	int fwidth;

	QImage *cache[NUM_P];
	PMonitor *r;
	QImage *background;
	QGLFramebufferObject *fbo;

	PMonitor *bg_window;

};

#endif 

