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

	void paintEvent(QPaintEvent *);
	void keyPressEvent(QKeyEvent *);
	void keyReleaseEvent(QKeyEvent *);
	void mouseDoubleClickEvent(QMouseEvent *);
	void mousePressEvent(QMouseEvent *);
	void mouseReleaseEvent(QMouseEvent *);
	void tabletEvent(QTabletEvent *);
	
	QPixmap requestPixmap(const QString&, QSize *, const QSize&);

	//void drawForeground(QPainter *, const QRectF &);
	
public slots:
	void invalidate(void);
	void bg_changed(void);
private:
	void create_bg(void);

	bool dirty;
	QPixmap *cache;
	QPixmap *background;
	QPainter *p;
	PMonitor *r;

	QDeclarativeView *bg_window;
};

extern const QByteArray background_qml;

#endif 

