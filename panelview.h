#ifndef _PANELVIEW_H
#define _PANELVIEW_H

#include<QRectF>
#include<QList>

#include"dialog.h"

#define NUM_C 6
#define NUM_R 2

#define ISRC "image://gen/"
#define ISRC_LEN (sizeof(ISRC)-1)

class QGraphicsPixmapItem;
class QGLFramebufferObject;
class QKeyEvent;
class QMouseEvent;
class QTabletEvent;
class QHideEvent;
class QShowEvent;
class QImage;

class PMonitor;
class PanelView;

class PMonitor : public Dialog {
    Q_OBJECT
public:
    PMonitor(void);
    friend class PanelView;
};

class PanelView : public Dialog { 
    Q_OBJECT
public:
    PanelView(void);
    ~PanelView(void);

    void keyPressEvent(QKeyEvent *);
    void keyReleaseEvent(QKeyEvent *);
    void mouseDoubleClickEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);

    void hideEvent(QHideEvent *);
    void showEvent(QShowEvent *);

public slots:
    void invalidate(const QList<QRectF> &);
    void bg_changed(void);
    void panel_snap(void);
    void panel_snap_flip(int);
private:
    void create_bg(void);
    inline void draw_single(int);

    int fwidth;
    int num_panels;
    int panel_width; 
    int panel_outer_spacing; 

    QGraphicsPixmapItem **items;

    QImage *background;
    QGLFramebufferObject *fbo;

    PMonitor *r;
    PMonitor *bg_window;
};

#endif

