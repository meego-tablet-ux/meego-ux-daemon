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
    Q_PROPERTY(int orientation READ orientation NOTIFY orientationChanged)

public:
    explicit Dialog(bool translucent, bool forceOnTop = false, bool opengl = false, QWidget * parent = 0);
    ~Dialog();

    int winId() const {
        return internalWinId();
    }

    void setSkipAnimation();

    int actualOrientation() {
        return m_actualOrientation;
    }
    void setActualOrientation(int orientation);

    void activateWindow() {
        emit activateContent();
        QWidget::activateWindow();
    }

    int orientation() {
        return m_orientation;
    }
    void setOrientation(int o) {
        m_orientation = o;
        emit orientationChanged();
    }

public slots:
    void triggerSystemUIMenu();
    void goHome();
    void showTaskSwitcher();

signals:
    void requestTaskSwitcher();
    void requestHome();
    void winIdChanged();
    void activateContent();
    void orientationChanged();

protected:
    bool event(QEvent * event);

private:
    void excludeFromTaskBar();
    void changeNetWmState(bool set, Atom one, Atom two = 0);

    bool m_forceOnTop;
    int m_actualOrientation;
    int m_orientation;
};
#endif // DIALOG_H
