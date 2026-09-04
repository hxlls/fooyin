#include "webdavclientmanager.h"

#include <utils/settings/settingsmanager.h>

#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace Fooyin::Webdav {

namespace {
//! Settings prefix holding per-server credentials, keyed by "webdav/<host:port>".
constexpr auto SettingsPrefix = "webdav/";
//! Keys inside the per-server settings map.
constexpr auto KeyUser     = "user";
constexpr auto KeyPassword = "password";
constexpr auto KeyInsecure = "insecureSsl";
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

WebdavClient* WebdavClientManager::clientFor(const QUrl& url)
{
    const QString key = authority(url);
    if(auto* client = m_clients.value(key)) {
        return client;
    }

    auto* client = new WebdavClient(this);
    m_clients.insert(key, client);

    // Configure from the stored per-server entry, if present. Entries are
    // written by the library settings UI when a WebDAV source is added.
    const QVariantMap entry = m_settings->fileValue(settingsKeyFor(key)).toMap();
    if(!entry.isEmpty()) {
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

} // namespace Fooyin::Webdav
