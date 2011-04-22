/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QFile>
#include <QTimer>
#include <libcgroup.h>

#include "application.h"
#include "process.h"

Process::Process(const QString cgroup, const QStringList controllers, QObject *parent) :
    QProcess(parent),
    m_cgroup(cgroup),
    m_controllers(controllers)
{
}

void Process::setupChildProcess()
{
    // add process to a control group
    if (!cgroup_init())
    {
        const char *defaultPath = "unknown";
        const char *defaultController = "freezer";

        struct cgroup_group_spec spec;
        ::memset(&spec, '\0', sizeof(spec));

        if (!m_cgroup.isEmpty())
        {
            ::strncpy(spec.path, m_cgroup.toAscii().constData(), m_cgroup.length() + 1);
        }
        else
        {
            ::strncpy(spec.path, defaultPath, ::strlen(defaultPath));
        }

        if (m_controllers.length() > 0)
        {
            for (int i = 0; i < m_controllers.length() && i < CG_CONTROLLER_MAX; i++)
            {
                spec.controllers[i] = m_controllers[i].toAscii().data();
            }
        }
        else
        {
            spec.controllers[0] = defaultController;
        }

        if (!cgroup_register_unchanged_process(::getpid(), 0))
        {
            if (cgroup_change_cgroup_path(spec.path, ::getpid(), spec.controllers))
            {

                // If the passed in control group path/controllers is invalid
                // then place it in the unknown group
                ::memset(&spec, '\0', sizeof(spec));
                ::strncpy(spec.path, defaultPath, ::strlen(defaultPath));
                spec.controllers[0] = defaultController;
                cgroup_change_cgroup_path(spec.path, ::getpid(), spec.controllers);
            }
        }
    }
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
