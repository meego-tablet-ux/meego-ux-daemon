/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <QProcess>

class Process : public QProcess
{
    Q_OBJECT
public:
    explicit Process(QString cgroup, QObject *parent = 0);

public slots:
    void tryAndDelete();

protected:
    void setupChildProcess();

private:
    QString m_cgroup;
};

#endif // PROCESS_H
