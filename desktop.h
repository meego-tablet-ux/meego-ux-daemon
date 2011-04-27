/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef DESKTOP_H
#define DESKTOP_H

#include <QDebug>
#include <QFile>
#include <QObject>
#include <QIcon>
#include <mdesktopentry.h>

#include "process.h"

class DesktopDatabase;

class Desktop : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type READ type NOTIFY typeChanged);
    Q_PROPERTY(QString title READ title NOTIFY titleChanged);
    Q_PROPERTY(QString comment READ comment NOTIFY commentChanged);
    Q_PROPERTY(QString icon READ icon NOTIFY iconChanged);
    Q_PROPERTY(QString exec READ exec NOTIFY execChanged);
    Q_PROPERTY(QString filename READ filename);
    Q_PROPERTY(int pid READ pid WRITE setPid);
    Q_PROPERTY(int wid READ wid WRITE setWid);
    Q_PROPERTY(QString cgroup READ cgroup);
    Q_PROPERTY(QStringList controllers READ controllers);

public:
    Desktop(const QString &filename, QObject *parent = 0);
    ~Desktop();

    enum Role {
        Type = Qt::UserRole + 1,
        Title = Qt::UserRole + 2,
        Comment = Qt::UserRole + 3,
        Icon = Qt::UserRole + 4,
        Exec = Qt::UserRole + 5,
        Filename = Qt::UserRole + 6,
        Pid = Qt::UserRole + 7,
        Wid = Qt::UserRole + 8
    };

    bool isValid() const {
        QStringList onlyShowIn = m_entry->onlyShowIn();
        if (!onlyShowIn.isEmpty() && !onlyShowIn.contains("X-MEEGO") &&
            !onlyShowIn.contains("X-MEEGO-HS"))
            return false;

        QStringList notShowIn = m_entry->notShowIn();
        if (!notShowIn.isEmpty() && (notShowIn.contains("X-MEEGO") ||
                                     notShowIn.contains("X-MEEGO-HS")))
            return false;

        return m_entry->isValid();
    }

    QString type() const {
        return m_entry->type();
    }

    QString title() const {
        return m_entry->name();
    }

    QString comment() const {
        return m_entry->comment();
    }

    QString icon() const {
        return QString("image://systemicon/") + m_entry->icon();
    }

    QString exec() const {
        return m_entry->exec();
    }

    QString filename() const {
        return m_filename;
    }

    int pid() {
        return m_pid;
    }
    void setPid(int pid) {
        m_pid = pid;
    }

    int wid() {
        return m_wid;
    }
    void setWid(int wid) {
        m_wid = wid;
    }

    QString cgroup() const {
        if (contains("Desktop Entry/X-MEEGO-CGROUP-PATH"))
            return value("Desktop Entry/X-MEEGO-CGROUP-PATH");
        else
            return QString("unknown");
    }

    QStringList controllers() const {
        QStringList list;
        if (contains("Desktop Entry/X-MEEGO-CGROUP-CONTROLLERS"))
        {
            list = value("Desktop Entry/X-MEEGO-CGROUP-CONTROLLERS").split(',');
        }
        else
        {
            list << "freezer";
        }
        return list;
    }

public slots:

    QString value(QString key) const {
        return m_entry->value(key);
    }

    bool contains(QString val) const {
        return m_entry->contains(val);
    }

    void launch();

    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void started();
    void terminate();

signals:
    void typeChanged();
    void titleChanged();
    void iconChanged();
    void execChanged();
    void commentChanged();
    void launched(int pid);

private:
    QString m_filename;
    MDesktopEntry *m_entry;
    int m_pid;
    int m_wid;
    Process *m_process;

    Q_DISABLE_COPY(Desktop)
};

#endif // DESKTOP_H
