#ifndef NOTIFICATIONITEM_H
#define NOTIFICATIONITEM_H

#include <QObject>
#include <QDateTime>

class NotificationItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(uint userId READ getUserId WRITE setUserId);
    Q_PROPERTY(uint notificationId READ getNotificationId WRITE setNotificationId);
    Q_PROPERTY(uint groupId READ getGroupId WRITE setGroupId);
    Q_PROPERTY(QString eventType READ getEventType WRITE setEventType);
    Q_PROPERTY(QString summary READ getSummary WRITE setSummary);
    Q_PROPERTY(QString body READ getBody WRITE setBody);
    Q_PROPERTY(QString action READ getAction WRITE setAction);
    Q_PROPERTY(QString imageURI READ getImageURI WRITE setImageURI);
    Q_PROPERTY(QString identifier READ getIdentifier WRITE setIdentifier);
    Q_PROPERTY(uint count READ getCount WRITE setCount);
    Q_PROPERTY(QString timestamp READ getTimestamp);

public:
    explicit NotificationItem(QObject *parent = 0) :
            QObject(parent),
            m_userId(0),
            m_notificationId(0),
            m_groupId(0),
            m_count(0)
    {
        m_timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    }

    explicit NotificationItem(uint userId, uint notificationId, uint groupId, const QString &eventType, const QString &summary, const QString &body, const QString &action, const QString &imageURI, uint count, const QString &identifier, QObject *parent = NULL) :
            QObject(parent),
            m_userId(userId),
            m_notificationId(notificationId),
            m_groupId(groupId),
            m_eventType(eventType),
            m_summary(summary),
            m_body(body),
            m_action(action),
            m_imageURI(imageURI),
            m_identifier(identifier),
            m_count(count)
    {
        m_timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    }

    enum Role{
        UserId = Qt::UserRole + 1,
        NotificationId = Qt::UserRole + 2,
        GroupId = Qt::UserRole + 4,
        EventType = Qt::UserRole + 5,
        Summary = Qt::UserRole + 6,
        Body = Qt::UserRole + 7,
        Action = Qt::UserRole + 8,
        ImageURI = Qt::UserRole + 9,
        Identifier = Qt::UserRole + 10,
        Count = Qt::UserRole + 11,
        Timestamp = Qt::UserRole + 12
     };

    uint getUserId() {
        return m_userId;
    }
    void setUserId(uint id) {
        m_userId = id;
    }

    uint getNotificationId() {
        return m_notificationId;
    }
    void setNotificationId(uint id) {
        m_notificationId = id;
    }

    uint getGroupId() {
        return m_groupId;
    }
    void setGroupId(uint id) {
        m_groupId = id;
    }

    QString getEventType() const {
        return m_eventType;
    }
    void setEventType(const QString etype) {
        m_eventType = etype;
    }

    QString getSummary() const {
        return m_summary;
    }
    void setSummary(const QString summary) {
        m_summary = summary;
    }

    QString getBody() const {
        return m_body;
    }
    void setBody(const QString body) {
        m_body = body;
    }

    QString getAction() const {
        return m_action;
    }
    void setAction(const QString action) {
        m_action = action;
    }

    QString getImageURI() const {
        return m_imageURI;
    }
    void setImageURI(const QString imageURI) {
        m_imageURI = imageURI;
    }

    QString getIdentifier() const {
        return m_identifier;
    }
    void setIdentifier(const QString identifier) {
        m_identifier = identifier;
    }

    uint getCount() {
        return m_count;
    }
    void setCount(uint count) {
        m_count = count;
    }

    QString getTimestamp() {
        return m_timestamp;
    }

private:
    uint m_userId;
    uint m_notificationId;
    uint m_groupId;
    QString m_eventType;
    QString m_summary;
    QString m_body;
    QString m_action;
    QString m_imageURI;
    QString m_identifier;
    uint m_count;
    QString m_timestamp;
};

#endif // NOTIFICATIONITEM_H
