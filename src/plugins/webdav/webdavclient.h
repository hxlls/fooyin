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

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPair>
#include <QThread>
#include <QUrl>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>

namespace Fooyin::Webdav {

struct WebdavResponse
{
    int status{0};
    QByteArray body;
    QString error;
    QList<QPair<QByteArray, QByteArray>> headers;

    [[nodiscard]] bool ok() const
    {
        return status >= 200 && status < 300;
    }
    //! 206 Partial Content — server honours Range requests.
    [[nodiscard]] bool isPartial() const
    {
        return status == 206;
    }

    [[nodiscard]] QByteArray header(const QByteArray& name) const;
    [[nodiscard]] qint64 contentLength() const;
    [[nodiscard]] bool acceptsRanges() const;
    [[nodiscard]] std::optional<qint64> rangeTotal() const;
};

struct WebdavEntry
{
    QString path;
    QString displayName;
    bool isDir{false};
    qint64 size{0};
    QDateTime modified;
    QString etag;
};

class NetworkWorker;

//! A single outbound request plus its completion latch. Owned by the caller of
//! the synchronous API; the network worker fills it and notifies `cv`.
struct WebdavClientRequest
{
    QUrl url;
    QByteArray verb{"GET"};
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;
    qint64 rangeStart{-1};
    qint64 rangeEnd{-1};

    std::mutex mutex;
    std::condition_variable cv;
    bool done{false};
    WebdavResponse response;
};

/*!
 * Synchronous WebDAV/HTTP transport.
 *
 * fooyin's decoders run on a worker thread without a running Qt event loop,
 * so they cannot own a QNetworkAccessManager. This class therefore owns a
 * dedicated network thread that runs the event loop and all network I/O.
 *
 * Requests are dispatched to the worker with a Qt::QueuedConnection functor
 * invocation (safe from any thread; only the receiving thread needs an event
 * loop) and the caller blocks on a condition variable until the worker fills
 * the response. No blocking cross-thread Qt call is used.
 *
 * A single shared client serves both decoding and scanning; the network thread
 * serialises requests, which is fine since one track is handled at a time.
 */
class WebdavClient : public QObject
{
    Q_OBJECT

public:
    explicit WebdavClient(QObject* parent = nullptr);
    ~WebdavClient() override;

    WebdavClient(const WebdavClient&)            = delete;
    WebdavClient& operator=(const WebdavClient&) = delete;

    void setCredentials(const QString& user, const QString& password);
    void setTimeout(std::chrono::milliseconds timeout);
    /*!
     * When enabled, TLS peer hostname verification is disabled. Needed to reach
     * servers whose certificate does not match the target host — e.g. a NAS
     * reached by raw LAN IP. Off by default.
     */
    void setInsecureSsl(bool insecure);

    [[nodiscard]] WebdavResponse head(const QUrl& url);
    [[nodiscard]] WebdavResponse getRange(const QUrl& url, qint64 start, qint64 end);
    [[nodiscard]] std::optional<QList<WebdavEntry>> listDirectory(const QUrl& url);

private:
    friend class NetworkWorker;

    //! Dispatch @p req to the worker and block until it completes.
    WebdavResponse exec(WebdavClientRequest* req);

    QString m_user;
    QString m_password;
    std::chrono::milliseconds m_timeout{30000};
    bool m_insecureSsl{false};

    QThread* m_thread{nullptr};
    NetworkWorker* m_worker{nullptr};
};

} // namespace Fooyin::Webdav
