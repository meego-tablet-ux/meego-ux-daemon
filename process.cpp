/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QTimer>
#include "process.h"

Process::Process(QString cgroup, QObject *parent) :
    QProcess(parent),
    m_cgroup(cgroup)
{
}

void Process::setupChildProcess()
{
    // add process to the cgroup
}

void Process::tryAndDelete()
{
    if (state() == QProcess::NotRunning)
    {
        deleteLater();
    }
    else
    {
        QTimer::singleShot(5000, this, SLOT(tryAndDelete()));
    }
}
