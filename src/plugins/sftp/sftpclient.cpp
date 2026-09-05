/*
 * Fooyin
 * Copyright © 2026
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "sftpclient.h"

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <QDateTime>
#include <QTcpSocket>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace Fooyin::Sftp {

/*!
 * Owns the libssh2 session. Lives on the network thread; created there and
 * never used from any other thread. No Q_OBJECT macro is needed (see
 * NetworkWorker in webdavclient.cpp for the same reasoning).
 */
class SftpWorker : public QObject
{
public:
    explicit SftpWorker(SftpClientConfig config)
        : m_config{std::move(config)}
    { }

    ~SftpWorker() override
    {
        disconnect();
    }

    //! Runs the requested operation. Called on the network thread.
    void execute(SftpRequest* req)
    {
        if(!ensureConnected(&req->error)) {
            complete(req);
            return;
        }

        switch(req->type) {
            case SftpRequest::Type::Stat:
                doStat(req);
                break;
            case SftpRequest::Type::List:
                doList(req);
                break;
            case SftpRequest::Type::Read:
                doRead(req);
                break;
        }

        complete(req);
    }

private:
    static void complete(SftpRequest* req)
    {
        {
            const std::scoped_lock lock{req->mutex};
            req->done = true;
        }
        req->cv.notify_all();
    }

    void disconnect()
    {
        if(m_sftp) {
            libssh2_sftp_shutdown(m_sftp);
            m_sftp = nullptr;
        }
        if(m_session) {
            libssh2_session_disconnect(m_session, "fooyin client shutdown");
            libssh2_session_free(m_session);
            m_session = nullptr;
        }
        if(m_socket) {
            m_socket->close();
            delete m_socket;
            m_socket = nullptr;
        }
    }

    //! Establishes TCP + SSH + auth once. On failure fills @p error.
    bool ensureConnected(QString* error)
    {
        if(m_sftp) {
            return true;
        }

        m_socket = new QTcpSocket;
        m_socket->connectToHost(m_config.host, m_config.port);
        if(!m_socket->waitForConnected(static_cast<int>(m_config.timeout.count()))) {
            *error = u"Could not connect to %1:%2: %3"_s.arg(m_config.host, QString::number(m_config.port),
                                                             m_socket->errorString());
            disconnect();
            return false;
        }

        m_session = libssh2_session_init();
        if(!m_session) {
            *error = u"Could not allocate an SSH session"_s;
            disconnect();
            return false;
        }
        libssh2_session_set_timeout(m_session, static_cast<int>(m_config.timeout.count()));
        libssh2_session_set_blocking(m_session, 1);

        if(libssh2_session_handshake(m_session, m_socket->socketDescriptor()) != 0) {
            *error = u"SSH handshake with %1:%2 failed (host key or protocol mismatch)"_s.arg(
                m_config.host, QString::number(m_config.port));
            disconnect();
            return false;
        }

        if(m_config.user.isEmpty()) {
            *error = u"No user name configured for %1:%2"_s.arg(m_config.host, QString::number(m_config.port));
            disconnect();
            return false;
        }

        if(libssh2_userauth_password(m_session, m_config.user.toUtf8().constData(),
                                     m_config.password.toUtf8().constData())
           != 0) {
            *error = u"Authentication failed for %1@%2:%3"_s.arg(m_config.user, m_config.host,
                                                                 QString::number(m_config.port));
            disconnect();
            return false;
        }

        m_sftp = libssh2_sftp_init(m_session);
        if(!m_sftp) {
            *error
                = u"SFTP subsystem could not be started on %1:%2"_s.arg(m_config.host, QString::number(m_config.port));
            disconnect();
            return false;
        }

        return true;
    }

    //! Maps a libssh2 error code to a readable message and logs the last error.
    QString sftpError(const QString& operation) const
    {
        const unsigned long code = m_sftp ? libssh2_sftp_last_error(m_sftp) : 0;
        return u"%1 failed (SFTP error %2)"_s.arg(operation, QString::number(code));
    }

    void doStat(SftpRequest* req)
    {
        LIBSSH2_SFTP_ATTRIBUTES attrs{};
        if(libssh2_sftp_stat(m_sftp, req->path.toUtf8().constData(), &attrs) != 0) {
            req->error = sftpError(u"stat %1"_s.arg(req->path));
            return;
        }

        SftpEntry entry;
        entry.path = req->path;
        entry.name = req->path;
        entry.isDir
            = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0 && (attrs.permissions & LIBSSH2_SFTP_S_IFDIR) != 0;
        entry.size = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0 ? static_cast<qint64>(attrs.filesize) : 0;
        entry.modifiedMs
            = (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0 ? static_cast<qint64>(attrs.mtime) * 1000 : 0;

        req->ok         = true;
        req->statResult = entry;
    }

    void doList(SftpRequest* req)
    {
        const QString dirPath          = req->path.isEmpty() ? u"/"_s : req->path;
        LIBSSH2_SFTP_HANDLE* const dir = libssh2_sftp_opendir(m_sftp, dirPath.toUtf8().constData());
        if(!dir) {
            req->error = sftpError(u"opendir %1"_s.arg(dirPath));
            return;
        }

        const QString basePath = dirPath.endsWith(u'/') ? dirPath : dirPath + u'/';

        char name[2048];
        char longname[4096];
        LIBSSH2_SFTP_ATTRIBUTES attrs{};

        int rc = 0;
        while((rc = libssh2_sftp_readdir_ex(dir, name, sizeof(name), longname, sizeof(longname), &attrs)) > 0) {
            const QString displayName = QString::fromUtf8(name);

            // Skip the "." and ".." entries returned by the server.
            if(displayName == u"."_s || displayName == u".."_s) {
                continue;
            }

            SftpEntry entry;
            entry.name = displayName;
            entry.isDir
                = (attrs.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) != 0 && (attrs.permissions & LIBSSH2_SFTP_S_IFDIR) != 0;
            entry.size = (attrs.flags & LIBSSH2_SFTP_ATTR_SIZE) != 0 ? static_cast<qint64>(attrs.filesize) : 0;
            entry.modifiedMs
                = (attrs.flags & LIBSSH2_SFTP_ATTR_ACMODTIME) != 0 ? static_cast<qint64>(attrs.mtime) * 1000 : 0;

            // Canonical child path: dir path (with trailing slash) + name.
            entry.path = basePath + displayName;
            if(entry.isDir && !entry.path.endsWith(u'/')) {
                entry.path += u'/';
            }

            req->entries.push_back(std::move(entry));
        }

        libssh2_sftp_closedir(dir);

        if(rc < 0) {
            req->error = sftpError(u"readdir %1"_s.arg(dirPath));
            return;
        }

        req->ok = true;
    }

    void doRead(SftpRequest* req)
    {
        if(req->start < 0 || req->length <= 0) {
            req->error = u"Invalid read range"_s;
            return;
        }

        LIBSSH2_SFTP_HANDLE* const handle
            = libssh2_sftp_open(m_sftp, req->path.toUtf8().constData(), LIBSSH2_FXF_READ, 0);
        if(!handle) {
            req->error = sftpError(u"open %1"_s.arg(req->path));
            return;
        }

        // seek64() is void: positioning errors surface as a failed read below.
        libssh2_sftp_seek64(handle, static_cast<libssh2_uint64_t>(req->start));

        req->data.reserve(static_cast<qsizetype>(req->length));

        qint64 remaining = req->length;
        char buffer[64 * 1024];

        while(remaining > 0) {
            const qint64 chunk = std::min<qint64>(remaining, static_cast<qint64>(sizeof(buffer)));
            const auto n       = libssh2_sftp_read(handle, buffer, static_cast<size_t>(chunk));
            if(n > 0) {
                req->data.append(buffer, static_cast<qsizetype>(n));
                remaining -= n;
                continue;
            }
            if(n == 0) {
                break; // EOF
            }
            // n < 0: EAGAIN should not happen in blocking mode; treat as error.
            req->error = sftpError(u"read %1"_s.arg(req->path));
            libssh2_sftp_close_handle(handle);
            return;
        }

        libssh2_sftp_close_handle(handle);
        req->ok = true;
    }

    SftpClientConfig m_config;

    QTcpSocket* m_socket{nullptr};
    LIBSSH2_SESSION* m_session{nullptr};
    LIBSSH2_SFTP* m_sftp{nullptr};
};

SftpClient::SftpClient(const SftpClientConfig& config, QObject* parent)
    : QObject{parent}
    , m_config{config}
{
    m_thread = new QThread(this);
    m_thread->setObjectName(u"SFTP network"_s);

    m_worker = new SftpWorker{config};
    m_worker->moveToThread(m_thread);

    QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_thread->start();
}

SftpClient::~SftpClient()
{
    if(m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
    }
}

void SftpClient::exec(SftpRequest* req)
{
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, req]() { worker->execute(req); }, Qt::QueuedConnection);

    std::unique_lock lock{req->mutex};
    if(!req->cv.wait_for(lock, m_config.timeout * 4, [req] { return req->done; })) {
        req->ok    = false;
        req->error = u"Request timed out waiting for the SFTP worker"_s;
    }
}

std::optional<SftpEntry> SftpClient::stat(const QString& remotePath, QString* errorDetail)
{
    SftpRequest req;
    req.type = SftpRequest::Type::Stat;
    req.path = remotePath;

    exec(&req);
    if(!req.ok && errorDetail) {
        *errorDetail = req.error;
    }
    if(!req.ok) {
        return {};
    }
    return req.statResult;
}

std::optional<QList<SftpEntry>> SftpClient::listDirectory(const QString& remotePath, QString* errorDetail)
{
    SftpRequest req;
    req.type = SftpRequest::Type::List;
    req.path = remotePath;

    exec(&req);
    if(!req.ok && errorDetail) {
        *errorDetail = req.error;
    }
    if(!req.ok) {
        return {};
    }
    return req.entries;
}

QByteArray SftpClient::readAt(const QString& remotePath, qint64 start, qint64 length, QString* errorDetail)
{
    SftpRequest req;
    req.type   = SftpRequest::Type::Read;
    req.path   = remotePath;
    req.start  = start;
    req.length = length;

    exec(&req);
    if(!req.ok && errorDetail) {
        *errorDetail = req.error;
    }
    return req.data;
}

} // namespace Fooyin::Sftp

#include "moc_sftpclient.cpp"
