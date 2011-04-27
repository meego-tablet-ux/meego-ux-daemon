/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QProcess>
#include <QtDeclarative/qdeclarative.h>
#include <QFile>

#include "desktop.h"

#include <sys/types.h>
#include <signal.h>

Desktop::Desktop(const QString &fileName, QObject *parent):
    QObject(parent),
    m_filename(fileName),
    m_entry(new MDesktopEntry(fileName)),
    m_pid(-1),
    m_wid(-1)
{
    m_process = new Process(cgroup(), controllers());
    connect(m_process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished(int,QProcess::ExitStatus)));
    connect(m_process, SIGNAL(started()), this, SLOT(started()));
}

Desktop::~Desktop()
{
    // ~QProcess will block the UI thread until it's associated process has
    // finished exiting, so we need to tryAndDelete which will setup a timer
    // to keep checking for the process state before destroying itself.
    //
    // This fixes BMC #16286
    m_process->tryAndDelete();
}

void Desktop::launch()
{
    QString cmd = exec();

    // http://standards.freedesktop.org/desktop-entry-spec/latest/ar01s06.html
    cmd.replace(QRegExp("%k"), filename());
    cmd.replace(QRegExp("%i"), QString("--icon ") + icon());
    cmd.replace(QRegExp("%c"), title());
    cmd.replace(QRegExp("%[fFuU]"), filename());
    m_process->start(cmd);
}

void Desktop::started()
{
    m_wid = -1;
    m_pid = m_process->pid();
    emit launched(m_pid);
}

void Desktop::finished(int, QProcess::ExitStatus)
{
    m_pid = -1;
    m_wid = -1;
}

void Desktop::terminate()
{
    if (m_pid == m_process->pid())
    {
        m_process->terminate();
    }
    else if (m_pid > 0)
    {
        // we didn't start this process, so manually send a SIGTERM
        kill(m_pid, SIGTERM);
    }
}

QML_DECLARE_TYPE(Desktop);
