#include "mytcpserver.h"
#include <QDebug>
#include <QCoreApplication>

MyTcpServer::MyTcpServer(QObject *parent)
    : QObject(parent)
{
    mTcpServer = new QTcpServer(this);
    mTcpServer->setMaxPendingConnections(100); // Лимит подключений

    connect(mTcpServer, &QTcpServer::newConnection,
            this, &MyTcpServer::slotNewConnection);

    if (!mTcpServer->listen(QHostAddress::Any, 33333)) {
        qDebug() << "Server error:" << mTcpServer->errorString();
    }
}

MyTcpServer::~MyTcpServer()
{
    mTcpServer->close();
    qDebug() << "Server stopped";
}

void MyTcpServer::slotNewConnection()
{
    QTcpSocket* clientSocket = mTcpServer->nextPendingConnection();
    if (!clientSocket) return;

    mClients.insert(clientSocket);

    // Инициализация клиента
    mClientRoles[clientSocket] = "guest";
    mClientUserIds[clientSocket] = -1;

    connect(clientSocket, &QTcpSocket::readyRead,
            this, &MyTcpServer::slotServerRead);
    connect(clientSocket, &QTcpSocket::disconnected,
            this, &MyTcpServer::slotClientDisconnected);

    clientSocket->write("Successful starting, OnLib is working!\r\n");
    qDebug() << "New client connected:" << clientSocket->peerAddress();
}

void MyTcpServer::slotClientDisconnected()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    mClients.remove(clientSocket);
    mClientRoles.remove(clientSocket);
    mClientUserIds.remove(clientSocket);

    clientSocket->deleteLater();
    qDebug() << "Client disconnected:" << clientSocket->peerAddress();
}

void MyTcpServer::slotServerRead()
{
    QTcpSocket* clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket || !mClients.contains(clientSocket)) return;

    QByteArray data = clientSocket->readAll();
    QString request = QString::fromUtf8(data).trimmed();

    // Обработка запроса с учетом текущего клиента
    QStringList parts = request.split('&');
    QString response = mCommandHandler.handleRequest(
        parts.value(0),
        parts,
        mClientRoles[clientSocket],    // Передаем роль
        mClientUserIds[clientSocket]   // Передаем user_id
        );

    // Обновляем данные клиента после аутентификации
    if (parts[0] == "auth" && response.startsWith("auth+")) {
        QStringList authParts = response.split('&');
        if (authParts.size() >= 4) {
            mClientRoles[clientSocket] = authParts[2];
            mClientUserIds[clientSocket] = authParts[3].toInt();
        }
    }

    clientSocket->write(response.toUtf8());
    qDebug() << "Sent response:" << response;
}
