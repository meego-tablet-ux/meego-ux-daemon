/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef DIALOG_H
#define DIALOG_H

#include <QDeclarativeView>
#include <QUrl>
#include <QWidget>
#include <X11/X.h>

class Dialog : public QDeclarativeView
{
    Q_OBJECT
    Q_PROPERTY(int winId READ winId NOTIFY winIdChanged)
    Q_PROPERTY(int actualOrientation READ actualOrientation WRITE setActualOrientation)
    Q_PROPERTY(bool inhibitScreenSaver READ dummyInhibitScreenSaver WRITE dummySetInhibitScreenSaver)

public:
    explicit Dialog(bool translucent, bool skipAnimation, bool forceOnTop, QWidget * parent = 0);
    ~Dialog();

    int winId() const {
        return internalWinId();
    }

    int actualOrientation() {
        return m_actualOrientation;
    }
    void setActualOrientation(int orientation);

    void activateWindow() {
        emit activateContent();
        QWidget::activateWindow();
    }

    // Just make the components library happy... the reality is we
    // always make each of these windows inhibit the screensaver
    int dummyInhibitScreenSaver() {
        return true;
    }
    void dummySetInhibitScreenSaver(bool) {}

public slots:
    void triggerSystemUIMenu();
    void goHome();
    void showTaskSwitcher();
    void switchToGLRendering();
    void switchToSoftwareRendering();
    void setSystemDialog();

private slots:
    void setGLRendering();

signals:
    void requestTaskSwitcher();
    void requestHome();
    void winIdChanged();
    void activateContent();
    void orientationChanged();
    void requestClose();

protected:
    bool event(QEvent * event);
    void closeEvent(QCloseEvent *);

private:
    void excludeFromTaskBar();
    void changeNetWmState(bool set, Atom one, Atom two = 0);

    bool m_forceOnTop;
    bool m_skipAnimation;
    int m_actualOrientation;
    bool m_translucent;
    bool m_usingGl;
};
#endif // DIALOG_H
