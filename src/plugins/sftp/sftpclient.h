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

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QThread>
#include <QUrl>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

namespace Fooyin::Sftp {

struct SftpEntry
{
    QString path; //!< Full remote path (used for opendir/stat/read).
    QString name; //!< Display name (last path segment).
    bool isDir{false};
    qint64 size{0};
    qint64 modifiedMs{0};
};

class SftpWorker;

//! One request plus its completion latch. Owned by the caller of the
//! synchronous API; the network worker fills it and notifies `cv`.
struct SftpRequest
{
    enum class Type
    {
        Stat,
        List,
        Read,
    };

    Type type{Type::Stat};
    QString path;
    qint64 start{-1};
    qint64 length{-1};

    bool ok{false};
    QString error;
    SftpEntry statResult;
    QList<SftpEntry> entries;
    QByteArray data;

    std::mutex mutex;
    std::condition_variable cv;
    bool done{false};
};

struct SftpClientConfig
{
    QString host;
    int port{22};
    QString user;
    QString password;
    std::chrono::milliseconds timeout{30000};
};

/*!
 * Synchronous SFTP transport backed by libssh2.
 *
 * fooyin decoders and the library scanner run on worker threads without a Qt
 * event loop, so this class owns a dedicated network thread (mirroring
 * WebdavClient). All libssh2 state lives on that thread: requests are queued to
 * it with a Qt::QueuedConnection functor invocation and the caller blocks on a
 * condition variable until the result is filled.
 *
 * libssh2 is not thread safe, but every operation runs serialised on the single
 * network thread, so no session state is ever touched concurrently.
 */
class SftpClient : public QObject
{
    Q_OBJECT

public:
    explicit SftpClient(const SftpClientConfig& config, QObject* parent = nullptr);
    ~SftpClient() override;

    SftpClient(const SftpClient&)            = delete;
    SftpClient& operator=(const SftpClient&) = delete;

    //! Stats @p remotePath. Returns nullopt on failure (error in @p errorDetail).
    [[nodiscard]] std::optional<SftpEntry> stat(const QString& remotePath, QString* errorDetail = nullptr);
    //! Lists @p remotePath's direct children. nullopt on failure.
    [[nodiscard]] std::optional<QList<SftpEntry>> listDirectory(const QString& remotePath,
                                                                QString* errorDetail = nullptr);
    //! Reads up to @p length bytes at @p start. Empty byte array on failure.
    [[nodiscard]] QByteArray readAt(const QString& remotePath, qint64 start, qint64 length,
                                    QString* errorDetail = nullptr);

private:
    //! Dispatch @p req to the worker and block until it completes.
    void exec(SftpRequest* req);

    SftpClientConfig m_config;

    QThread* m_thread{nullptr};
    SftpWorker* m_worker{nullptr};
};

} // namespace Fooyin::Sftp
