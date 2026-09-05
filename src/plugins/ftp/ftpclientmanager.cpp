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

#include "ftpclientmanager.h"

#include <utils/settings/settingsmanager.h>

#include <QThread>
#include <QVariantMap>

#include <future>
#include <mutex>

using namespace Qt::StringLiterals;

namespace Fooyin::Ftp {

namespace {
//! Settings prefix holding per-server credentials, keyed by "ftp/<host:port>".
constexpr auto SettingsPrefix = "ftp/";

//! Maps an URL scheme/port onto the TLS mode understood by libcurl.
//!
//! Most "FTPS" servers use explicit AUTH TLS on the control port (21) — the
//! ftps:// scheme in the GUI therefore means explicit TLS there. Implicit TLS
//! (the RFC 4217-less raw-TLS variant) lives on port 990 and keeps the raw
//! ftps:// transport that libcurl implies for the scheme.
FtpSecurity securityForScheme(const QUrl& url)
{
    if(url.scheme() == u"ftps"_s) {
        return url.port(990) == 990 ? FtpSecurity::Implicit : FtpSecurity::Explicit;
    }
    return FtpSecurity::Plain;
}
} // namespace

QString settingsKeyFor(const QString& authority)
{
    return QStringLiteral("ftp/") + authority;
}

FtpClientManager::FtpClientManager(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
{ }

FtpClientManager::~FtpClientManager()
{
    qDeleteAll(m_clients);
}

QString FtpClientManager::authority(const QUrl& url) const
{
    if(url.port() > 0) {
        return QStringLiteral("%1:%2").arg(url.host()).arg(url.port());
    }
    return url.host();
}

FtpClient* FtpClientManager::createClient(const QString& key, const QUrl& url)
{
    FtpClientConfig config;
    config.host     = url.host();
    config.port     = url.port(21);
    config.security = securityForScheme(url);

    const QVariantMap entry = m_settings->fileValue(settingsKeyFor(key)).toMap();
    if(entry.isEmpty()) {
        qWarning() << "No stored credentials for FTP server" << key
                   << "- add the library through the settings UI first.";
    }
    else {
        config.user        = entry.value(QStringLiteral("user")).toString();
        config.password    = entry.value(QStringLiteral("password")).toString();
        config.insecureSsl = entry.value(QStringLiteral("insecureSsl")).toBool();
    }

    return new FtpClient(config);
}

FtpClient* FtpClientManager::clientFor(const QUrl& url)
{
    const QString key = authority(url);

    {
        const std::scoped_lock lock{m_mutex};
        if(auto* existing = m_clients.value(key)) {
            return existing;
        }
    }

    if(QThread::currentThread() == thread()) {
        const std::scoped_lock lock{m_mutex};
        if(auto* existing = m_clients.value(key)) {
            return existing;
        }
        auto* client = createClient(key, url);
        m_clients.insert(key, client);
        return client;
    }

    std::promise<FtpClient*> created;
    std::future<FtpClient*> future = created.get_future();

    QMetaObject::invokeMethod(
        this,
        [this, key, url, &created]() {
            FtpClient* client{nullptr};
            {
                const std::scoped_lock lock{m_mutex};
                client = m_clients.value(key);
                if(!client) {
                    client = createClient(key, url);
                    m_clients.insert(key, client);
                }
            }
            created.set_value(client);
        },
        Qt::QueuedConnection);

    return future.get();
}

} // namespace Fooyin::Ftp
