#ifndef MYTCPSERVER_H
#define MYTCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include "commandhandler.h"
#include "database.h"

class MyTcpServer : public QObject
{
    Q_OBJECT

public:
    explicit MyTcpServer(QObject *parent = nullptr);
    ~MyTcpServer();

public slots:
    void slotNewConnection();
    void slotClientDisconnected();
    void slotServerRead();

private:
    QTcpServer *mTcpServer;
    QTcpSocket *mTcpSocket;
    CommandHandler mCommandHandler;
    QMap<QTcpSocket*, QString> mClientRoles; // Хранение ролей клиентов
    QMap<QTcpSocket*, int> mClientUserIds;  // Хранение ID пользователей
    QSet<QTcpSocket*> mClients; // Новая коллекция для хранения клиентов
};

#endif // MYTCPSERVER_H
