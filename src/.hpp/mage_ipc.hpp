// mage_ipc.hpp
// last updated: 08/07/2026
#pragma once
#include <QString>
#include <QObject>
#include <QLocalServer>
#include <QLocalSocket>
namespace pk::ipc
{
    bool s_t_prim_instance(const QString &mode, const QString &path);

    class ipc_server : public QObject
    {
        Q_OBJECT
    public:
        explicit ipc_server(QObject *parent = nullptr);
        ~ipc_server() override;

        bool start();
    signals:
        void rq_enc(const QString &path);
        void rq_dec(const QString &path);
    private slots:
        void hn_connection();
        void read_data();

    private:
        QLocalServer *m_server;
    };
}

// end