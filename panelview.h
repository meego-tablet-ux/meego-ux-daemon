#ifndef _PANELVIEW_H
#define _PANELVIEW_H

#include<QDeclarativeImageProvider>
#include<QRectF>
#include<QList>

#include"dialog.h"

#define NUM_P 8
#define NUM_C 4
#define NUM_R 2

#define ISRC "image://gen/"
#define ISRC_LEN strlen(ISRC)

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

    void hideEvent(QHideEvent *);
    void showEvent(QShowEvent *);

    QImage  requestImage(const QString &, QSize *, const QSize &);

public slots:
    void invalidate(const QList<QRectF> &);
    void bg_changed(void);
private:
    void create_bg(void);
    inline void draw_single(int);

    int fwidth;

    QImage *cache[NUM_P];
    QImage *background;
    QGLFramebufferObject *fbo;

    PMonitor *r;
    PMonitor *bg_window;
};

#endif

