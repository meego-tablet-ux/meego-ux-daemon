#ifndef _PANELVIEW_H
#define _PANELVIEW_H

#include<cmath>
#include<QDeclarativeImageProvider>
#include<QRectF>
#include<QList>

#include"dialog.h"

#define NUM_C 6
#define NUM_R 2

#define ISRC "image://gen/"
#define ISRC_LEN (sizeof(ISRC)-1)

#define IQML    "import Qt 4.7\n"           \
                "Image {\n"                 \
                    "\twidth:%i\n"          \
                    "\theight:%i\n"         \
                    "\tx:%i\n"              \
                    "\ty:%i\n"              \
                    "\tsource: \"%s\"\n"    \
                 "}\n" 
#define IQML_LEN (sizeof(IQML)-1)
#define IQML_INT 4

#define INT_LEN (((size_t)log10(INT_MAX))+2)
#define SRC_MAX (ISRC_LEN + INT_LEN + 1)
#define QML_MAX ((INT_LEN * IQML_INT) + ISRC_LEN + IQML_LEN +  1)

class QGLFramebufferObject;
class QKeyEvent;
class QMouseEvent;
class QTabletEvent;
class QHideEvent;
class QShowEvent;
class QImage;
class QDeclarativeItem; 

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
    void mouseMoveEvent(QMouseEvent *);

    void hideEvent(QHideEvent *);
    void showEvent(QShowEvent *);

    QImage  requestImage(const QString &, QSize *, const QSize &);

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

    QDeclarativeItem **items;

    QImage *background;
    QGLFramebufferObject *fbo;

    PMonitor *r;
    PMonitor *bg_window;
};

#endif

