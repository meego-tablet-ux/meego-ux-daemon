#ifndef _PANELVIEW_H
#define _PANELVIEW_H 

#include<QString>
#include<QSize>
#include<QUrl>

#include<QPixmap>
#include<QPainter>
#include<QGraphicsView>
#include<QGraphicsScene>
#include<QWidget>

#include<QPaintEvent>
#include<QKeyEvent>
#include<QMouseEvent>
#include<QTabletEvent>

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
		  public QDeclarativeImageProvidier {

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

	void regenerate(void);
	
	QPixmap requestPixmap(const QString&, QSize *, const QSize&);
	
public slots:
	void invalidate(void);
private:
	bool dirty;
	QPixmap *cache;
	QPainter *p;
	PMonitor *r;
};

#endif 

