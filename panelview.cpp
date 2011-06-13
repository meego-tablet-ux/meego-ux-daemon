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
	int fwidth;
	QObject *i;

	r = new PMonitor();
	/* TODO maybe some error handling in case i == NULL at any step here */
	i = r->rootObject()->findChild<QDeclarativeItem *>("deviceScreen");
	i = i->findChild<QDeclarativeItem *>("PC");
	i = i->findChild<QDeclarativeItem *>("PFLICK");
	fwidth = i->property("contentWidth").toInt();
	r->rootObject()->setProperty("width", fwidth);
	r->rootObject()->setProperty("height", height);

	QObject::connect(r->scene(), SIGNAL(changed(const QList<QRectF>&)), 
			this, SLOT(invalidate(void)));

	setSceneRect(0, 0, width, height);

	cache = new QPixmap(fwidth, height);

	p = new QPainter(cache);
	p->setRenderHint(QPainter::Antialiasing, false);
	p->setRenderHint(QPainter::TextAntialiasing, false);
	p->setRenderHint(QPainter::SmoothPixmapTransform, false);
	p->setRenderHint(QPainter::HighQualityAntialiasing, false);
	
	r->viewport()->render(p);
	engine()->addImageProvider(QLatin1String("gen"), this);

	setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/real.qml"));
	
	rootObject()->setProperty("contentWidth", fwidth);
	rootObject()->setProperty("contentHeight", height);
	rootObject()->setProperty("width", width);
	rootObject()->setProperty("height", height);
	
	setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

	setOptimizationFlags(
		QGraphicsView::DontSavePainterState | 
		QGraphicsView::DontAdjustForAntialiasing);
	
	setCacheMode(QGraphicsView::CacheBackground);
	scene()->setItemIndexMethod(QGraphicsScene::NoIndex);
	
//	viewport()->setAutoFillBackground(false);
/*
	viewport()->setAttribute(Qt::WA_TranslucentBackground, false);
*/

	viewport()->setAttribute(Qt::WA_PaintUnclipped);
	dirty = false;
	create_bg();

}

PanelView::~PanelView(void)
{
	delete r;
	delete p;
	delete cache;
	delete background;
	delete bg_window;
}

void PanelView::paintEvent(QPaintEvent *e)
{
	QDeclarativeItem *i;
	const int width = qApp->desktop()->rect().width();
	const int height = qApp->desktop()->rect().height();

	i =  qobject_cast<QDeclarativeItem*>(rootObject());

	if(dirty) {
		/* TODO Revamp This */
		QList<QObject*> tmp;
		tmp = i->children();
		QString meh = tmp.at(1)->property("source").toString();
		meh += '0';
		qDebug() << meh;
		tmp.at(1)->setProperty("source", meh);

		dirty = false;
	
	}


/*
	QPainter wp(viewport());
	drawBackground(&wp, QRect(0,0,width, height));
	scene()->render(&wp,
			QRectF(0, 0, width, height),
			QRectF(0, 0, width, height), 
			Qt::KeepAspectRatioByExpanding);
*/

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
	QDeclarativeView::mouseReleaseEvent(e);
	return;
}

void PanelView::tabletEvent(QTabletEvent *e)
{
	r->tabletEvent(e);
	return;
}

QPixmap PanelView::requestPixmap(const QString &id, QSize *size, 
		const QSize &resize) 
{
	return *cache;
}

void PanelView::invalidate(void)
{
	int i, j;
	const QRgb tran  = QColor(Qt::transparent).rgb();
	QPixmap *old = cache;


	dirty = true;

	QImage img(cache->width(), cache->height(), 
		QImage::Format_ARGB32); 
	QPainter ip(&img);
	r->viewport()->render(&ip);
	ip.end();

	const QRgb white = img.pixel(0,0);
	for(i = 0; i < img.height(); i++) {
		for(j = 0; j < img.width(); j++) {
			if(img.pixel(j, i) == white) {
				img.setPixel(j,i, tran);
			}
		}
	}


	QPixmap *tmp = new QPixmap(cache->width(), cache->height());
	tmp->convertFromImage(img, Qt::ColorOnly | Qt::NoOpaqueDetection);
	QBitmap mask = QBitmap::fromImage(img.createHeuristicMask());
	tmp->setMask(mask);

	p->end();
	cache = tmp;
	delete old;
	p->begin(cache);

	
#if 0
	if(!p->isActive()) { p->begin(cache); } 
	/*TODO investigate whether it's faster to render on paint device
 	 * or directly to QPixmap...if it makes a difference at all..
 	 */
	r->viewport()->render(p);
	p->end(); 
#endif 
	//scene()->invalidate(QRectF(0,0,0,0), QGraphicsScene::BackgroundLayer)
	viewport()->update();
	return;
}

/*
void PanelView::drawForeground(QPainter *p, const QRectF &rect)
{
	
}
*/

void PanelView::create_bg(void)
{
	const int width = qApp->desktop()->rect().width();
	const int height = qApp->desktop()->rect().height();

	bg_window = new QDeclarativeView();
	bg_window->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/background.qml"));
	background = new QPixmap(width, height); 
	
	QObject::connect(bg_window->scene(), SIGNAL(changed(const QList<QRectF>&)), 
			this, SLOT(bg_changed(void)));
}

void PanelView::bg_changed(void)
{
	QPainter bg_painter(background);
	bg_window->viewport()->render(&bg_painter); 
	QBrush br(*background);	
	br.setStyle(Qt::TexturePattern);
	setBackgroundBrush(br); 

	scene()->invalidate(QRectF(0,0,0,0), QGraphicsScene::BackgroundLayer); 
}


