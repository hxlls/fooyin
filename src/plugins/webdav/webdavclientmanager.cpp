#include "webdavclientmanager.h"

#include <utils/settings/settingsmanager.h>

#include <QThread>
#include <QVariantMap>

#include <future>
#include <mutex>

using namespace Qt::StringLiterals;

namespace Fooyin::Webdav {

namespace {
//! Settings prefix holding per-server credentials, keyed by "webdav/<host:port>".
constexpr auto SettingsPrefix = "webdav/";
} // namespace

QString settingsKeyFor(const QString& authority)
{
    return QStringLiteral("webdav/") + authority;
}

WebdavClientManager::WebdavClientManager(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
{ }

WebdavClientManager::~WebdavClientManager()
{
    qDeleteAll(m_clients);
}

QString WebdavClientManager::authority(const QUrl& url) const
{
    if(url.port() > 0) {
        return QStringLiteral("%1:%2").arg(url.host()).arg(url.port());
    }
    return url.host();
}

WebdavClient* WebdavClientManager::createClient(const QString& key)
{
    // Only ever called on this object's thread: a client owns a QThread and a
    // QNetworkAccessManager, and both must live on the thread that created them.
    auto* client = new WebdavClient(this);

    // Configure from the stored per-server entry, written by the library
    // settings UI when a WebDAV source is added.
    const QVariantMap entry = m_settings->fileValue(settingsKeyFor(key)).toMap();
    if(entry.isEmpty()) {
        qWarning() << "No stored credentials for WebDAV server" << key
                   << "- add the library through the settings UI first.";
    }
    else {
        client->setCredentials(entry.value(QStringLiteral("user")).toString(),
                               entry.value(QStringLiteral("password")).toString());
        if(entry.value(QStringLiteral("insecureSsl")).toBool()) {
            // The server certificate does not match the target host (e.g. a NAS
            // reached by raw LAN IP against a hostname certificate); disable peer
            // verification for this server only.
            client->setInsecureSsl(true);
        }
    }

    return client;
}

WebdavClient* WebdavClientManager::clientFor(const QUrl& url)
{
    const QString key = authority(url);

    {
        const std::scoped_lock lock{m_mutex};
        if(auto* existing = m_clients.value(key)) {
            return existing;
        }
    }

    // Clients are always created on this object's thread. Callers run on scanner
    // or decoder worker threads (which have no event loop), so the request is
    // queued to the manager thread and waited for; creating the client there
    // keeps its QThread and QNetworkAccessManager on one well-defined thread.
    if(QThread::currentThread() == thread()) {
        const std::scoped_lock lock{m_mutex};
        if(auto* existing = m_clients.value(key)) {
            return existing;
        }
        auto* client = createClient(key);
        m_clients.insert(key, client);
        return client;
    }

    std::promise<WebdavClient*> created;
    std::future<WebdavClient*> future = created.get_future();

    QMetaObject::invokeMethod(
        this,
        [this, key, &created]() {
            WebdavClient* client{nullptr};
            {
                const std::scoped_lock lock{m_mutex};
                client = m_clients.value(key);
                if(!client) {
                    client = createClient(key);
                    m_clients.insert(key, client);
                }
            }
            created.set_value(client);
        },
        Qt::QueuedConnection);

    return future.get();
}

} // namespace Fooyin::Webdav
