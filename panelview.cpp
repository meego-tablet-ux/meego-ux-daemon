#include"panelview.h"

PMonitor::PMonitor(void) : Dialog(false)
{
	const int width = qApp->desktop()->rect().width();
	const int height = qApp->desktop()->rect().height();

	setSceneRect(0, 0, width, height);
	setViewportUpdateMode(QGraphicsView::NoViewportUpdate);
	setAttribute(Qt::WA_PaintUnclipped);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_NoBackground);
	setCacheMode(QGraphicsView::CacheNone);
	setOptimizationFlags(
		QGraphicsView::DontSavePainterState | 
		QGraphicsView::DontAdjustForAntialiasing);

	viewport()->setAttribute(Qt::WA_TranslucentBackground, false);

	scene()->setItemIndexMethod(QGraphicsScene::NoIndex);	

	rootContext()->setContextProperty("screenWidth", width);
	rootContext()->setContextProperty("screenHeight", height);

	setSource(QUrl::fromLocalFile("/usr/share/meego-ux-panels/main.qml"));
}

PanelView::PanelView(void) : Dialog(false), 
		QDeclarativeImageProvider(QDeclarativeImageProvider::Pixmap)
{
	const int width = qApp->desktop()->rect().width();
	const int height = qApp->desktop()->rect().height();

	r = new PMonitor();
	
	/* FIXME hardcoded width */
	r->rootObject()->setProperty("width", 4200);
	r->rootObject()->setProperty("height", height);

	QObject::connect(r->scene(), SIGNAL(changed(const QList<QRectF>&)), 
			this, SLOT(invalidate(void)));

	setSceneRect(0, 0, width, height);

	/* FIXME hardcoded width */
	cache = new QPixmap(4200, height);

	p = new QPainter(cache);
	p->setRenderHint(QPainter::Antialiasing, false);
	p->setRenderHint(QPainter::TextAntialiasing, false);
	p->setRenderHint(QPainter::SmoothPixmapTransform, false);
	p->setRenderHint(QPainter::HighQualityAntialiasing, false);
	
	regenerate();
	engine()->addImageProvider(QLatin1String("gen"), this);

	/* FIXME hardcoded path to dummy QML */
	setSource(QUrl::fromLocalFile("/home/meego/panelview/real.qml"));
/*
	QDeclarativeItem *i = qobject_cast<QDeclarativeItem *>(rootObject());
	i->setProperty("width", r->width());
	i->setProperty("height", r->height());
*/
	
	setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	setOptimizationFlags(
		QGraphicsView::DontSavePainterState | 
		QGraphicsView::DontAdjustForAntialiasing);
	
	scene()->setItemIndexMethod(QGraphicsScene::NoIndex);
	
	viewport()->setAutoFillBackground(false);
	viewport()->setAttribute(Qt::WA_PaintUnclipped);
	viewport()->setAttribute(Qt::WA_TranslucentBackground, false);

	dirty = false;
}

PanelView::~PanelView(void)
{
	delete r;
	delete p;
	delete cache;
}

void PanelView::paintEvent(QPaintEvent *e)
{
	if(dirty) {
		regenerate();

		/*TODO Revamp This */
		QList<QObject*> tmp;
		QDeclarativeItem *i =  qobject_cast<QDeclarativeItem*>(rootObject());
		tmp = i->children();
		QString meh = tmp.at(1)->property("source").toString();
		meh += '0';
		qDebug() << meh;
		tmp.at(1)->setProperty("source", meh);

		dirty = false;
	
	}
	QDeclarativeView::paintEvent(e);
	return;
}

void PanelView::keyPressEvent(QKeyEvent *e)
{
	r->keyPressEvent(e);
	return;
}

void PanelView::keyReleaseEvent(QKeyEvent *e)
{
	r->keyReleaseEvent(e);
	return;
}

void PanelView::mouseDoubleClickEvent(QMouseEvent *e)
{
	r->mouseDoubleClickEvent(e);
	return;
}

void PanelView::mousePressEvent(QMouseEvent *e)
{
	QDeclarativeItem *i;

	i = qobject_cast<QDeclarativeItem *>(rootObject());
	QPoint j(e->x() + i->property("contentX").toDouble(), e->y());
	QMouseEvent k(e->type(), j, e->button(), e->buttons(), e->modifiers()); 
	
	r->mousePressEvent(&k);
	QDeclarativeView::mousePressEvent(e);
	return;
}

void PanelView::mouseReleaseEvent(QMouseEvent *e)
{
	r->mouseReleaseEvent(e);
	return;
}

void PanelView::tabletEvent(QTabletEvent *e)
{
	r->tabletEvent(e);
	return;
}

void PanelView::regenerate(void)
{
	r->viewport()->render(p);
	return;
}

QPixmap PanelView::requestPixmap(const QString &id, QSize *size, 
		const QSize &resize) 
{
	return *cache;
}

void PanelView::invalidate(void)
{
	dirty = true;
	viewport()->update();
	return;
}



