#include"panelview.h"

#include<QString>
#include<QSize>
#include<QUrl>

#include<QGraphicsView>
#include<QGraphicsScene>
#include<QWidget>
#include<QDesktopWidget>

#include<QGLFramebufferObject>
#include<QPainter>

#include<QPaintEvent>
#include<QKeyEvent>
#include<QMouseEvent>

#include<QDeclarativeEngine>
#include<QDeclarativeContext>
#include<QDeclarativeView>
#include<QDeclarativeItem>

#include<QApplication>

PMonitor::PMonitor(void) : Dialog(false, false, false)
{
    const int width = qApp->desktop()->rect().width();
    const int height = qApp->desktop()->rect().height();

    setViewportUpdateMode(QGraphicsView::NoViewportUpdate);

    setAttribute(Qt::WA_NoSystemBackground);
    setCacheMode(QGraphicsView::CacheNone);
    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);

    viewport()->setAttribute(Qt::WA_TranslucentBackground, false);

    rootContext()->setContextProperty("screenWidth", width);
    rootContext()->setContextProperty("screenHeight", height);

    setSource(QUrl::fromLocalFile("/usr/share/meego-ux-panels/main.qml"));
}

PanelView::PanelView(void) : Dialog(false, false, true)
{
    const int width = qApp->desktop()->rect().width();
    const int height = qApp->desktop()->rect().height();

    int j, k, total=0, cur_width, cur_height;
    QObject *child;
    QDeclarativeItem *contentItem; 


    r = new PMonitor();

    if((child = r->rootObject()->findChild<QDeclarativeItem *>("panelSize"))
            == NULL) {
        qWarning("Could not determine flickable content spacing");
        panel_outer_spacing = 0;
    }
    else {
        panel_outer_spacing = child->property("panelOuterSpacing").toInt(); 
    }

    if(((child = r->rootObject()->findChild<QDeclarativeItem *>("appPage"))
         == NULL) |
       ((child =  child->findChild<QDeclarativeItem *>("PC")) == NULL) |
       ((child = child->findChild<QDeclarativeItem *>("PLV")) == NULL)) {
        qWarning("Could not determine flickable content width");
        fwidth = width;
        num_panels = NUM_C; 
        panel_width = 0;
    }
    else {
        fwidth = child->property("contentWidth").toInt();
        num_panels = child->property("count").toInt();
        panel_width = child->property("currentItem").value<QDeclarativeItem *>(
                )->property("width").toInt();
        QObject::connect(child, SIGNAL(panelFlip(int)), this, SLOT(panel_snap_flip(int))); 
    }
    r->rootObject()->setProperty("width", fwidth);
    r->rootObject()->setProperty("height", height);

    items = reinterpret_cast<QGraphicsPixmapItem **>(calloc(num_panels,
                 sizeof(void *)));

    const int p_width = fwidth /  num_panels;
    const int p_height = height / NUM_R;

    qobject_cast<QGLWidget *>(viewport())->makeCurrent();
    fbo = new QGLFramebufferObject(p_width, p_height,
             QGLFramebufferObject::CombinedDepthStencil);

    setSource(QUrl::fromLocalFile("/usr/share/meego-ux-daemon/real.qml"));

    contentItem = rootObject()->property("contentItem").value<
            QDeclarativeItem *>(); 

    for(j = 0, cur_height =0; j < NUM_R; j++, cur_height += p_height) {
        for(k = 0, cur_width = 0; k < num_panels; k++, cur_width += p_width) {
            items[total] = scene()->addPixmap(QPixmap(p_width, p_height));
            items[total]->setPos(cur_width, cur_height);
            items[total]->setParentItem(contentItem); 
            items[total]->setCacheMode(QGraphicsItem::DeviceCoordinateCache);

            total++;
        }
    }
    rootObject()->setProperty("contentWidth", fwidth);
    rootObject()->setProperty("contentHeight", height);
    rootObject()->setProperty("width", width);
    rootObject()->setProperty("height", height);

    QObject::connect(rootObject(), SIGNAL(movementEnded()), 
                    this, SLOT(panel_snap(void))); 

    QObject::connect(r->scene(), SIGNAL(changed(const QList<QRectF>&)),
            this, SLOT(invalidate(const QList<QRectF>&)));

    setOptimizationFlags(QGraphicsView::DontAdjustForAntialiasing);
    setCacheMode(QGraphicsView::CacheBackground);
    viewport()->setAttribute(Qt::WA_OpaquePaintEvent);

    create_bg();
}

PanelView::~PanelView(void)
{
    free(items);

    delete r;
    delete background;
    delete bg_window;

    delete fbo;
}

void PanelView::keyPressEvent(QKeyEvent *e)
{
    r->keyPressEvent(e);
}

void PanelView::keyReleaseEvent(QKeyEvent *e)
{
    r->keyReleaseEvent(e);
}

void PanelView::mouseDoubleClickEvent(QMouseEvent *e)
{
    QDeclarativeItem *ro;

    ro = qobject_cast<QDeclarativeItem *>(rootObject());
    QPoint np(e->x() + ro->property("contentX").toDouble(), e->y());
    QMouseEvent ne(e->type(), np, e->button(), e->buttons(), e->modifiers());

    bg_window->mouseDoubleClickEvent(e);
    r->mouseDoubleClickEvent(&ne);
}

void PanelView::mousePressEvent(QMouseEvent *e)
{
    QDeclarativeItem *ro;

    ro = qobject_cast<QDeclarativeItem *>(rootObject());
    QPoint np(e->x() + ro->property("contentX").toDouble(), e->y());
    QMouseEvent ne(e->type(), np, e->button(), e->buttons(), e->modifiers());

    bg_window->mousePressEvent(e);
    r->mousePressEvent(&ne);
    QDeclarativeView::mousePressEvent(e);
}

void PanelView::mouseReleaseEvent(QMouseEvent *e)
{
    QDeclarativeItem *ro;

    ro = qobject_cast<QDeclarativeItem *>(rootObject());
    QPoint np(e->x() + ro->property("contentX").toDouble(), e->y());
    QMouseEvent ne(e->type(), np, e->button(), e->buttons(), e->modifiers());

    bg_window->mouseReleaseEvent(e);
    r->mouseReleaseEvent(&ne);
    QDeclarativeView::mouseReleaseEvent(e);
}

void PanelView::mouseMoveEvent(QMouseEvent *e)
{
    QDeclarativeItem *ro;

    ro = qobject_cast<QDeclarativeItem *>(rootObject());
    QPoint np(e->x() + ro->property("contentX").toDouble(), e->y());
    QMouseEvent ne(e->type(), np, e->button(), e->buttons(), e->modifiers());

    r->mouseMoveEvent(&ne);
    QDeclarativeView::mouseMoveEvent(e);
}

void PanelView::hideEvent(QHideEvent *e)
{
    Q_UNUSED(e);

    QDeclarativeItem *item;

    item = qobject_cast<QDeclarativeItem *>(rootObject()); 
    if(item != NULL) { 
        if((item = item->findChild<QDeclarativeItem *>("statusBar")) != NULL) {
            item->setProperty("active", false); 
        }
    }
}

void PanelView::showEvent(QShowEvent *e)
{
    Q_UNUSED(e);
       
    QDeclarativeItem *item;

    item = qobject_cast<QDeclarativeItem *>(rootObject()); 
    if(item != NULL) { 
        if((item = item->findChild<QDeclarativeItem *>("statusBar")) != NULL) {
            item->setProperty("active", true); 
        }
    }
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
    const int height = qApp->desktop()->rect().height();
    const int p_width = fwidth /  num_panels;
    const int p_height = height / NUM_R;

    int i, j, c_width, c_height, total=0;

    if(region.count() > 0) {
        QRectF n = consolidate_width(region);
        for(i = 0, c_height =0; i < NUM_R; i++, c_height += p_height) {
            for(j =0, c_width=0; j < num_panels; j++, c_width += p_width) {
                QRectF d(c_width, c_height , p_width, p_height);
                if(d.intersects(n)) {
                        draw_single(total);
                }
                total++;
            }
        }
    }
}

inline void PanelView::draw_single(int i)
{
    const int height = qApp->desktop()->rect().height();
    const int p_width = fwidth /  num_panels;
    const int p_height = height / NUM_R;

    QPainter p;
    int c, _r=0;

    if(items[i] == NULL) {
        return;
    }

    c = i;
    while(c >= num_panels) {
        _r++;
        c -= num_panels;
    }

    qobject_cast<QGLWidget *>(viewport())->makeCurrent();
    p.begin(fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        r->viewport()->render(&p, QPoint(), QRegion(
            p_width * c, p_height *_r,
            p_width, p_height));
    p.end();

    items[i]->setPixmap(QPixmap::fromImage(fbo->toImage()));
}

void PanelView::create_bg(void)
{
    const int width = qApp->desktop()->rect().width();
    const int height = qApp->desktop()->rect().height();

    bg_window = new PMonitor();
    bg_window->setSource(QUrl::fromLocalFile(
        "/usr/share/meego-ux-daemon/background.qml"));
    background = new QImage(width, height,
         QImage::Format_ARGB32_Premultiplied);

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

void PanelView::panel_snap(void)
{
    int x, next_p, prev_p, diff_n, diff_p;
    
    x  = rootObject()->property("contentX").toInt(); 

    next_p = 0; 
    while(next_p < x) {
        next_p += panel_width + panel_outer_spacing;
    }

    prev_p = next_p - panel_width - panel_outer_spacing; 

    diff_n = next_p - x;
    diff_p = x - prev_p; 

    diff_p = (diff_p > diff_n) ? next_p : prev_p;

    if(x != diff_p) {
        rootObject()->setProperty("contentX", diff_p); 
    }
   
}

void PanelView::panel_snap_flip(int index)
{
    const int width = qApp->desktop()->rect().width();
   
    int x, l, r;

    x = rootObject()->property("contentX").toInt();

    l = index * (panel_width + panel_outer_spacing); 
    r = l + panel_width; 

    if(l < x) {
        rootObject()->setProperty("contentX", l);
    }
    else if(r > (x + width)) {
        rootObject()->setProperty("contentX", r - width);
    }
}

