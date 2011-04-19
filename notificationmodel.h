/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef NOTIFICATIONMODEL_H
#define NOTIFICATIONMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <MGConfItem>
#include "notificationitem.h"

#define MAXCUSTOMNOTIFICATIONS 4

class NotificationDataStore;

class NotificationModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY modelReset);
public:
    explicit NotificationModel(QObject *parent = 0);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    int getCount();

signals:
    void modelReset();

public slots:
    void trigger(uint userId, uint notificationId);
    void refreshViewableList();
    void addFilter(QString filter);
    void removeFilter(QString filter);
    void clearFilters();
    void applyLockscreenFilters();

private slots:
    void onDataStoreUpdated();

private:
    QList<NotificationItem *> displayData;
    QList<QString> listFilters;
    NotificationDataStore *m_data;
};

#endif // NOTIFICATIONMODEL_H
