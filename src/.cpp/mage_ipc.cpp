// mage_ipc.cpp
// last updated: 30/10/2026
#include "../.hpp/mage_ipc.hpp"
#include <QDataStream>
#include <QCryptographicHash>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace pk::ipc
{
    static QString get_ipc_name()
    {
        QString user;
#ifdef _WIN32
        wchar_t uname[257]{};
        DWORD len = 256;
        if (GetUserNameW(uname, &len) && len > 0)
        {
            user = QString::fromWCharArray(uname);
        }
#else
        user = QString("uid_%1").arg(static_cast<qulonglong>(::getuid()));
#endif
        if (user.isEmpty())
            user = qEnvironmentVariable("USERNAME");
        if (user.isEmpty())
            user = qEnvironmentVariable("USER");
        if (user.isEmpty())
            user = "unknown";
        return "MAGE_IPC_" + QString(QCryptographicHash::hash(user.toUtf8(), QCryptographicHash::Sha512).toHex());
    }
    bool s_t_prim_instance(const QString &mode, const QString &path)
    {
        if (path.isEmpty() || path.size() > 32768 || path.contains(QChar('\0')))
            return false;
        QLocalSocket socket;
        socket.connectToServer(get_ipc_name());
        if (socket.waitForConnected(500))
        {
#ifdef _WIN32
            AllowSetForegroundWindow(ASFW_ANY);
#endif
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
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        connect(m_server, &QLocalServer::newConnection, this, &ipc_server::hn_connection);
    }
    ipc_server::~ipc_server()
    {
        if (m_server->isListening())
        {
            QString srv_name = m_server->serverName();
            m_server->close();
            if (!srv_name.isEmpty())
                QLocalServer::removeServer(srv_name);
        }
    }
    bool ipc_server::start()
    {
        QString name = get_ipc_name();
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        if (m_server->listen(name))
            return true;
        QLocalSocket probe;
        probe.connectToServer(name);
        if (probe.waitForConnected(150))
        {
            probe.disconnectFromServer();
            return false;
        }
        QLocalServer::removeServer(name);
        return m_server->listen(name);
    }
    void ipc_server::hn_connection()
    {
        while (m_server->hasPendingConnections())
        {
            QLocalSocket *socket = m_server->nextPendingConnection();
            if (!socket)
                continue;
            connect(socket, &QLocalSocket::readyRead, this, &ipc_server::read_data);
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        }
    }
    void ipc_server::read_data()
    {
        QLocalSocket *socket = qobject_cast<QLocalSocket *>(sender());
        if (!socket)
            return;
        if (socket->bytesAvailable() > 65536)
        {
            socket->abort();
            return;
        }
        QDataStream in(socket);
        in.setVersion(QDataStream::Qt_6_0);
        in.startTransaction();
        QString mode;
        QString path;
        in >> mode >> path;
        if (!in.commitTransaction())
            return;
        if (path.isEmpty() || path.size() > 32768 || path.contains(QChar('\0')))
        {
            socket->disconnectFromServer();
            return;
        }
        if (mode == "encrypt")
        {
            emit rq_enc(path);
        }
        else if (mode == "decrypt")
        {
            emit rq_dec(path);
        }
        else if (mode == "verify")
        {
            emit rq_ver(path);
        }
    }
}

// end