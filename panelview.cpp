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

	int j, k, total=1, cur_width, cur_height=0;
	QList<QObject*> tmp;
	QDeclarativeItem *dec;
	QObject *i;

	r = new PMonitor();
	if((i = r->rootObject()->findChild<QDeclarativeItem *>("deviceScreen")) == NULL | 
	   (i = i->findChild<QDeclarativeItem *>("appPage")) == NULL | 
	   (i = i->findChild<QDeclarativeItem *>("PC")) == NULL | 
	   (i = i->findChild<QDeclarativeItem *>("PLV")) == NULL) {
		qFatal("Upgrade your version of MeeGo-UX-Panels");
		exit(-1);
	}
	fwidth = i->property("contentWidth").toInt();
	r->rootObject()->setProperty("width", fwidth);
	r->rootObject()->setProperty("height", height);

	QObject::connect(r->scene(), SIGNAL(changed(const QList<QRectF>&)), 
			this, SLOT(invalidate(void)));

	setSceneRect(0, 0, width, height);

	const int p_width = fwidth /  NUM_C;
	const int p_height = height / NUM_R; 
	for(j = 0; j < NUM_P; j++) {
		cache[j] = new QPixmap(p_width, p_height);
		p[j] = new QPainter(cache[j]); 
		p[j]->setRenderHint(QPainter::Antialiasing, false);
		p[j]->setRenderHint(QPainter::TextAntialiasing, false);
		p[j]->setRenderHint(QPainter::SmoothPixmapTransform, false);
		p[j]->setRenderHint(QPainter::HighQualityAntialiasing, false);
	}
	
	invalidate();
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
	
			QString meh = QString("image://gen/");
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
	viewport()->setAttribute(Qt::WA_PaintUnclipped);

	create_bg();
}

PanelView::~PanelView(void)
{
	int i; 

	for(i = 0; i < NUM_P; i++) {
		delete p[i];
		delete cache[i];
	}

	delete r;
	delete background;
	delete bg_window;

}

void PanelView::paintEvent(QPaintEvent *e)
{
	QDeclarativeItem *dec;
	QList<QObject*> tmp;
	int i, j;
	QString meh;

	if(dirty) {
		dec =  qobject_cast<QDeclarativeItem*>(rootObject());
		tmp = dec->children();
		for(i = 1; i <= NUM_P; i++) {
			meh = tmp.at(i)->property("source").toString();
			j = meh.right( meh.size() - 12).toInt(); 
			meh.truncate(12);
			j += NUM_P;
			meh += QString::number(j);
			tmp.at(i)->setProperty("source", meh);
		}
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

void PanelView::invalidate(void)
{
	int i,j,k,m;

	QPixmap *old, *tmp; 

	const int height = qApp->desktop()->rect().height();
	const int p_width = fwidth /  NUM_C;
	const int p_height = height / NUM_R; 
	const QRgb tran  = QColor(Qt::transparent).rgb();
	QRgb bg_color; 

	int total = 0;
	for(i = 0; i < NUM_R; i++) {
		for(j = 0; j < NUM_C; j++) {
			old = cache[total];

			QImage img(p_width, p_height,QImage::Format_ARGB32);

			QPainter ip(&img);
			r->viewport()->render(&ip, QPoint(),
				 QRegion( p_width * j, p_height * i, 
					  p_width, p_height));
			ip.end(); 

			if(total == 0) {
				bg_color = img.pixel(0,0);
			}

			tmp = new QPixmap(p_width, p_height); 
			tmp->convertFromImage(img,
				Qt::ColorOnly | Qt::NoOpaqueDetection);
			QBitmap mask = QBitmap::fromImage(
					img.createMaskFromColor(bg_color));
			tmp->setMask(mask);

			p[total]->end();

			cache[total] = tmp;
			delete old;

			p[total]->begin(cache[total]);
			
			total++;
		}
	}	
	dirty = true;
	viewport()->update();
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


