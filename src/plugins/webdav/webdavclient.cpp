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

#include "webdavclient.h"

#include <QAuthenticator>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QThread>
#include <QTimer>
#include <QXmlStreamReader>

using namespace Qt::StringLiterals;

namespace Fooyin::Webdav {

namespace {
constexpr auto PropfindBody = R"(<?xml version="1.0" encoding="utf-8" ?>
<D:propfind xmlns:D="DAV:">
  <D:prop>
    <D:getcontentlength/>
    <D:getlastmodified/>
    <D:getetag/>
    <D:resourcetype/>
  </D:prop>
</D:propfind>)";

//! Parses a 207 Multi-Status body into entries, dropping the requested directory itself.
QList<WebdavEntry> parseMultiStatus(const QByteArray& body, const QUrl& baseUrl)
{
    QList<WebdavEntry> entries;

    QXmlStreamReader xml{body};
    WebdavEntry current;
    bool inResponse{false};
    bool inProp{false};
    QString propName;

    while(!xml.atEnd()) {
        xml.readNext();
        if(!xml.isStartElement() && !xml.isEndElement()) {
            continue;
        }

        const QString name = xml.name().toString();

        if(xml.isStartElement()) {
            if(name.compare(u"response", Qt::CaseInsensitive) == 0) {
                current    = WebdavEntry{};
                inResponse = true;
            }
            else if(inResponse && name.compare(u"href", Qt::CaseInsensitive) == 0) {
                // Keep the href exactly as the server sent it (percent-encoded).
                // Decoding here would be lossy: servers may encode a space as '+'
                // (form-style) which QUrl::fromPercentEncoding does not map back to
                // a space, so a later request for the resource would 404. Storing
                // the encoded href and building encoded webdav(s) URLs keeps the
                // path lossless and round-trip safe.
                current.path = xml.readElementText();
            }
            else if(inResponse && name.compare(u"prop", Qt::CaseInsensitive) == 0) {
                inProp = true;
            }
            else if(inProp && name.compare(u"collection", Qt::CaseInsensitive) == 0) {
                current.isDir = true;
            }
            else if(inProp) {
                propName = name;
            }
        }
        else if(xml.isEndElement()) {
            if(name.compare(u"response", Qt::CaseInsensitive) == 0) {
                inResponse = false;

                if(!current.path.isEmpty() && current.path != baseUrl.path()) {
                    const QStringList parts = current.path.split(u'/', Qt::SkipEmptyParts);
                    current.displayName     = parts.isEmpty() ? current.path : parts.last();

                    // Directory hrefs conventionally carry a trailing slash.
                    if(current.isDir && !current.displayName.isEmpty()) {
                        current.path = current.path.endsWith(u'/') ? current.path : current.path + u'/';
                    }

                    entries.push_back(current);
                }
            }
            else if(name.compare(u"prop", Qt::CaseInsensitive) == 0) {
                inProp = false;
                propName.clear();
            }
        }

        if(xml.isCharacters() && inProp && !propName.isEmpty()) {
            const QString value = xml.text().trimmed().toString();
            if(propName.compare(u"getcontentlength", Qt::CaseInsensitive) == 0) {
                current.size = value.toLongLong();
            }
            else if(propName.compare(u"getlastmodified", Qt::CaseInsensitive) == 0) {
                current.modified = QDateTime::fromString(value, Qt::RFC2822Date);
            }
            else if(propName.compare(u"getetag", Qt::CaseInsensitive) == 0) {
                current.etag = value;
            }
        }
    }

    return entries;
}
} // namespace

QByteArray WebdavResponse::header(const QByteArray& name) const
{
    for(const auto& [key, value] : headers) {
        if(key.compare(name, Qt::CaseInsensitive) == 0) {
            return value;
        }
    }
    return {};
}

qint64 WebdavResponse::contentLength() const
{
    const QByteArray value = header("Content-Length");
    if(value.isEmpty()) {
        return -1;
    }
    return value.toLongLong();
}

bool WebdavResponse::acceptsRanges() const
{
    return header("Accept-Ranges").compare("bytes", Qt::CaseInsensitive) == 0;
}

std::optional<qint64> WebdavResponse::rangeTotal() const
{
    // Content-Range: bytes <start>-<end>/<total>
    const QByteArray value = header("Content-Range");
    const int slash        = value.lastIndexOf('/');
    if(slash < 0) {
        return {};
    }

    bool ok{false};
    const qint64 total = value.sliced(slash + 1).trimmed().toLongLong(&ok);
    if(!ok || total < 0) {
        return {};
    }
    return total;
}

/*!
 * Lives on the network thread. QNetworkAccessManager and all reply/timer
 * QObjects are created and owned here, so their signals are delivered on this
 * thread's event loop.
 *
 * No Q_OBJECT macro is needed: it declares no custom signals/slots and is used
 * only as the QObject context for connections and as the target of functor
 * invocations. Defined in the translation unit and matched to the forward
 * declaration in webdavclient.h.
 */
class NetworkWorker : public QObject
{
public:
    NetworkWorker() = default;

    /*!
     * Creates the network manager. Must run on the worker's own thread (after
     * moveToThread), because a QNetworkAccessManager may only be used from the
     * thread that created it. The client is created on a scanner/decoder worker
     * thread, so constructing the manager there would crash when the network
     * thread later drives it.
     */
    void init()
    {
        m_network = new QNetworkAccessManager(this);
        QObject::connect(m_network, &QNetworkAccessManager::authenticationRequired, this,
                         [this](QNetworkReply*, QAuthenticator* authenticator) {
                             if(!m_user.isEmpty()) {
                                 authenticator->setUser(m_user);
                                 authenticator->setPassword(m_password);
                             }
                         });
    }

    void setCredentials(const QString& user, const QString& password)
    {
        m_user     = user;
        m_password = password;
    }

    void setTimeout(int timeoutMs)
    {
        m_timeoutMs = timeoutMs;
    }

    void setInsecureSsl(bool insecure)
    {
        m_insecureSsl = insecure;
    }

    //! Applies the configured TLS policy to @p request.
    void applySsl(QNetworkRequest* request) const
    {
        if(m_insecureSsl) {
            QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
            ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
            request->setSslConfiguration(ssl);
        }
    }

    //! Runs the network transaction for @p req, then signals its latch.
    void execute(WebdavClientRequest* req)
    {
        if(!req->url.isValid()) {
            req->response.error = u"Invalid URL"_s;
            complete(req);
            return;
        }

        QNetworkRequest request{req->url};
        applySsl(&request);
        for(const auto& [key, value] : req->headers) {
            request.setRawHeader(key, value);
        }
        if(req->rangeStart >= 0) {
            const QString range = req->rangeEnd >= 0 ? u"bytes=%1-%2"_s.arg(req->rangeStart).arg(req->rangeEnd)
                                                     : u"bytes=%1-"_s.arg(req->rangeStart);
            request.setRawHeader("Range", range.toLatin1());
        }

        QNetworkReply* reply{nullptr};
        if(!req->body.isEmpty()) {
            auto* buffer = new QBuffer{};
            buffer->setData(req->body);
            buffer->open(QIODevice::ReadOnly);
            reply = m_network->sendCustomRequest(request, req->verb, buffer);
            buffer->setParent(reply);
        }
        else if(req->verb == QByteArrayLiteral("HEAD")) {
            reply = m_network->head(request);
        }
        else {
            reply = m_network->get(request);
        }

        auto* timer = new QTimer(reply);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);

        QObject::connect(reply, &QNetworkReply::finished, reply, [req, reply] {
            req->response.status  = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            req->response.body    = reply->readAll();
            req->response.headers = reply->rawHeaderPairs();
            if(reply->error() != QNetworkReply::NoError && req->response.status == 0) {
                req->response.error = reply->errorString();
            }

            reply->deleteLater();
            complete(req);
        });

        timer->start(m_timeoutMs);
    }

private:
    static void complete(WebdavClientRequest* req)
    {
        {
            const std::scoped_lock lock{req->mutex};
            req->done = true;
        }
        req->cv.notify_all();
    }

    QNetworkAccessManager* m_network;
    QString m_user;
    QString m_password;
    int m_timeoutMs{30000};
    bool m_insecureSsl{false};
};

WebdavClient::WebdavClient(QObject* parent)
    : QObject{parent}
{
    m_thread = new QThread(this);
    m_thread->setObjectName(u"WebDAV network"_s);

    m_worker = new NetworkWorker();
    m_worker->moveToThread(m_thread);

    QObject::connect(m_thread, &QThread::started, m_thread, [this]() {
        // QNAM must be created on this (network) thread.
        m_worker->init();
        m_worker->setCredentials(m_user, m_password);
        m_worker->setTimeout(static_cast<int>(m_timeout.count()));
        m_worker->setInsecureSsl(m_insecureSsl);
    });
    QObject::connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_thread->start();
}

WebdavClient::~WebdavClient()
{
    if(m_thread) {
        m_thread->quit();
        m_thread->wait(5000);
    }
}

void WebdavClient::setCredentials(const QString& user, const QString& password)
{
    m_user     = user;
    m_password = password;

    if(m_thread && m_thread->isRunning()) {
        QMetaObject::invokeMethod(
            m_worker, [worker = m_worker, user, password]() { worker->setCredentials(user, password); },
            Qt::QueuedConnection);
    }
}

void WebdavClient::setTimeout(std::chrono::milliseconds timeout)
{
    m_timeout = timeout;
    if(m_thread && m_thread->isRunning()) {
        QMetaObject::invokeMethod(
            m_worker, [worker = m_worker, ms = static_cast<int>(timeout.count())]() { worker->setTimeout(ms); },
            Qt::QueuedConnection);
    }
}

void WebdavClient::setInsecureSsl(bool insecure)
{
    m_insecureSsl = insecure;
    if(m_thread && m_thread->isRunning()) {
        QMetaObject::invokeMethod(
            m_worker, [worker = m_worker, insecure]() { worker->setInsecureSsl(insecure); }, Qt::QueuedConnection);
    }
}

void makeRequest(WebdavClientRequest& req, const QUrl& url, const QByteArray& verb, const QByteArray& body,
                 const QList<QPair<QByteArray, QByteArray>>& headers, qint64 start, qint64 end)
{
    req.url        = url;
    req.verb       = verb;
    req.body       = body;
    req.headers    = headers;
    req.rangeStart = start;
    req.rangeEnd   = end;
}

WebdavResponse WebdavClient::exec(WebdavClientRequest* req)
{
    // Functor form runs the lambda on the worker's thread (QueuedConnection)
    // regardless of the calling thread, so it is safe from a decoder thread
    // that has no event loop.
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, req]() { worker->execute(req); }, Qt::QueuedConnection);

    // Wait with a hard timeout so a stalled/hung worker cannot block the
    // calling (decoder) thread forever.
    std::unique_lock lock{req->mutex};
    if(!req->cv.wait_for(lock, m_timeout * 2, [req] { return req->done; })) {
        req->response.status = 0;
        req->response.error  = u"Request timed out waiting for network worker"_s;
    }
    return req->response;
}

WebdavResponse WebdavClient::head(const QUrl& url)
{
    WebdavClientRequest req;
    makeRequest(req, url, QByteArrayLiteral("HEAD"), {}, {}, -1, -1);
    return exec(&req);
}

WebdavResponse WebdavClient::getRange(const QUrl& url, qint64 start, qint64 end)
{
    WebdavClientRequest req;
    makeRequest(req, url, QByteArrayLiteral("GET"), {}, {}, start, end);
    return exec(&req);
}

std::optional<QList<WebdavEntry>> WebdavClient::listDirectory(const QUrl& url, QString* errorDetail)
{
    WebdavClientRequest req;
    makeRequest(req, url, QByteArrayLiteral("PROPFIND"), PropfindBody,
                {{QByteArrayLiteral("Depth"), QByteArrayLiteral("1")}}, -1, -1);

    const WebdavResponse response = exec(&req);
    if(response.status != 207) {
        if(errorDetail) {
            if(response.status > 0) {
                *errorDetail = u"HTTP %1"_s.arg(response.status)
                             + (response.error.isEmpty() ? QString{} : u" - "_s + response.error);
            }
            else {
                *errorDetail = response.error.isEmpty() ? u"Request failed (no response)"_s : response.error;
            }
        }
        return {};
    }

    return parseMultiStatus(response.body, url);
}

} // namespace Fooyin::Webdav

#include "moc_webdavclient.cpp"
