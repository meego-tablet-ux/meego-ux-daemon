#include"panelview.h"


PMonitor::PMonitor(void) : Dialog(false)
{
	const int width = qApp->desktop()->rect().width();
	const int height = qApp->desktop()->rect().height();

	setSceneRect(0, 0, width, height);
	setViewportUpdateMode(QGraphicsView::NoViewportUpdate);

	setAttribute(Qt::WA_NoSystemBackground);
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

	int j, k, total=1, cur_width, cur_height=0;
	QList<QObject*> tmp;
	QDeclarativeItem *dec;
	QObject *i;

	r = new PMonitor();

	if((i = r->rootObject()->findChild<QDeclarativeItem *>("appPage")) == NULL | 
	   (i = i->findChild<QDeclarativeItem *>("PC")) == NULL | 
	   (i = i->findChild<QDeclarativeItem *>("PLV")) == NULL) {
		qFatal("Upgrade your version of MeeGo-UX-Panels");
	}
	fwidth = i->property("contentWidth").toInt();
	r->rootObject()->setProperty("width", fwidth);
	r->rootObject()->setProperty("height", height);

	QObject::connect(r->scene(), SIGNAL(changed(const QList<QRectF>&)), 
			this, SLOT(invalidate(const QList<QRectF>&)));

	setSceneRect(0, 0, width, height);

	const int p_width = fwidth /  NUM_C;
	const int p_height = height / NUM_R; 
	for(j = 0; j < NUM_P; j++) {
		cache[j] = new QPixmap(p_width, p_height);
	}
	
	qobject_cast<QGLWidget *>(viewport())->makeCurrent();
	fbo = new QGLFramebufferObject(p_width, p_height,
			 QGLFramebufferObject::CombinedDepthStencil);

	engine()->addImageProvider(QLatin1String("gen"), this);

	setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/real.qml"));
	rootObject()->setProperty("contentWidth", fwidth);
	rootObject()->setProperty("contentHeight", height);
	rootObject()->setProperty("width", width);
	rootObject()->setProperty("height", height);

	dec =  qobject_cast<QDeclarativeItem*>(rootObject());
	tmp = dec->children();


	for(j = 0; j < NUM_R; j++) {
		cur_width = 0;
		for(k = 0; k < NUM_C; k++) {
			tmp.at(total)->setProperty("width", p_width);
			tmp.at(total)->setProperty("height", p_height);
			tmp.at(total)->setProperty("x", cur_width); 
			tmp.at(total)->setProperty("y", cur_height); 
	
			QString meh = QString(ISRC);
			meh += QString::number(total-1);
			tmp.at(total)->setProperty("source", meh);

			cur_width += p_width;
			total++;
		}
		cur_height += p_height;
	}
	
	
	setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

	setOptimizationFlags(
		QGraphicsView::DontSavePainterState | 
		QGraphicsView::DontAdjustForAntialiasing);
	
	setCacheMode(QGraphicsView::CacheBackground);
	scene()->setItemIndexMethod(QGraphicsScene::NoIndex);
	viewport()->setAttribute(Qt::WA_OpaquePaintEvent);

	
	create_bg();
}

PanelView::~PanelView(void)
{
	int i; 

	for(i = 0; i < NUM_P; i++) {
		delete cache[i];
	}

	delete r;
	delete background;
	delete bg_window;

	delete fbo;
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
	
	bg_window->mousePressEvent(e);
	r->mousePressEvent(&k);
	QDeclarativeView::mousePressEvent(e);
	return;
}

void PanelView::mouseReleaseEvent(QMouseEvent *e)
{
	bg_window->mouseReleaseEvent(e);
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
	int i = id.toInt(); 
	while(i >= NUM_P) { i -= NUM_P; } 
	return *cache[i];
}


static QRectF consolidate_width(const QList<QRectF> &region)
{
	qreal l,r, t, b;
	int i;

	l = region.at(0).x();
	r = region.at(0).x() + region.at(0).width();
	t = region.at(0).y();
	b = region.at(0).y() + region.at(0).height();
	
	for(i = 1; i < region.count(); i++) {
		const double _l = region.at(i).x();
		const double _r = region.at(i).x() + region.at(i).width();
		const double _t = region.at(i).y();
		const double _b = region.at(i).y() + region.at(i).height();
		if(_l < l) {
			l = _l;
		}
		if(_r> r) {
			r = _r;
		}
		if(_t < t) {
			t = _t;
		}
		if(_b > b) {
			b = _b;
		}
	}
	return QRectF(l, t, r -l, b - t); 
}

void PanelView::invalidate(const QList<QRectF> &region)
{
	int i, j, c_width, c_height, total=0;

	const int height = qApp->desktop()->rect().height();
	const int p_width = fwidth /  NUM_C;
	const int p_height = height / NUM_R; 

	if(region.count() >= 1) {
		QRectF n = consolidate_width(region);
		for(i = 0, c_height =0; i < NUM_R; i++, c_height += p_height) {
			for(j =0, c_width=0; j < NUM_C; j++, c_width += p_width) {
				QRectF d(c_width, c_height , p_width, p_height);
				if(d.intersects(n)) {	
					draw_single(total); 
				}
				total++;
			}
		}
	}
	return;
}

inline void PanelView::draw_single(int i)
{
	const int height = qApp->desktop()->rect().height();
	const int p_width = fwidth /  NUM_C;
	const int p_height = height / NUM_R; 

	static const QDeclarativeItem *dec = qobject_cast<QDeclarativeItem *>
		(rootObject());
	static const QList<QObject *> kids = dec->children();

	int c, _r=0,k;
	QImage *old;
	QString src;
	QPainter p;

	c = i;
	while(c >= NUM_C) {
		_r++;
		c -= NUM_C;
	}

	qobject_cast<QGLWidget *>(viewport())->makeCurrent();
	p.begin(fbo);	
		glClear(GL_COLOR_BUFFER_BIT);
		r->viewport()->render(&p, QPoint(), QRegion(
			p_width * c, p_height *_r, 
			p_width, p_height));
	p.end();

	old = cache[i];
	cache[i] = new QImage(fbo->toImage());
	delete old;
	

	src = kids.at(i+1)->property("source").toString();
	k = src.right(src.size() - ISRC_LEN).toInt();
	src.truncate(ISRC_LEN);
	k += NUM_P;
	src += QString::number(k);
	kids.at(i+1)->setProperty("source", src);

	return;		
}


void PanelView::create_bg(void)
{
	const int width = qApp->desktop()->rect().width();
	const int height = qApp->desktop()->rect().height();

	bg_window = new PMonitor();
	bg_window->setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/background.qml"));
	background = new QPixmap(width, height); 
	
	QObject::connect(bg_window->scene(), SIGNAL(changed(const QList<QRectF>&)), 
			this, SLOT(bg_changed(void)));
}

void PanelView::bg_changed(void)
{
	QPainter bg_painter(background);
	bg_window->viewport()->render(&bg_painter); 
	bg_painter.end();
	QBrush br(*background);	
	br.setStyle(Qt::TexturePattern);
	setBackgroundBrush(br); 

	scene()->invalidate(QRectF(0,0,0,0), QGraphicsScene::BackgroundLayer); 
}


