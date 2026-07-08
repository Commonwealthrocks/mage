// mage_ipc.cpp
// last updated: 09/07/2026
#include "../.hpp/mage_ipc.hpp"
#include <QDataStream>
#include <QCryptographicHash>
namespace pk::ipc
{
    static QString get_ipc_name()
    {
        QString user = qgetenv("USERNAME");
        if (user.isEmpty())
            user = qgetenv("USER");
        if (user.isEmpty())
            user = "unknown";
        return "MAGE_IPC_" + QString(QCryptographicHash::hash(user.toUtf8(), QCryptographicHash::Sha512).toHex());
    }
    bool s_t_prim_instance(const QString &mode, const QString &path)
    {
        QLocalSocket socket;
        socket.connectToServer(get_ipc_name());
        if (socket.waitForConnected(500))
        {
            QByteArray block;
            QDataStream out(&block, QIODevice::WriteOnly);
            out.setVersion(QDataStream::Qt_6_0);
            out << mode << path;
            socket.write(block);
            socket.waitForBytesWritten(500);
            socket.disconnectFromServer();
            return true;
        }
        return false;
    }
    ipc_server::ipc_server(QObject *parent) : QObject(parent), m_server(new QLocalServer(this))
    {
        connect(m_server, &QLocalServer::newConnection, this, &ipc_server::hn_connection);
    }

    ipc_server::~ipc_server()
    {
        if (m_server->isListening())
        {
            m_server->close();
            QLocalServer::removeServer(get_ipc_name());
        }
    }
    bool ipc_server::start()
    {
        QString name = get_ipc_name();
        QLocalServer::removeServer(name);
        return m_server->listen(name);
    }
    void ipc_server::hn_connection()
    {
        QLocalSocket *socket = m_server->nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, &ipc_server::read_data);
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    }
    void ipc_server::read_data()
    {
        QLocalSocket *socket = qobject_cast<QLocalSocket *>(sender());
        if (!socket)
            return;
        QDataStream in(socket);
        in.setVersion(QDataStream::Qt_6_0);
        in.startTransaction();
        QString mode;
        QString path;
        in >> mode >> path;
        if (!in.commitTransaction())
            return;
        if (mode == "encrypt")
        {
            emit rq_enc(path);
        }
        else if (mode == "decrypt")
        {
            emit rq_dec(path);
        }
    }
}

// end