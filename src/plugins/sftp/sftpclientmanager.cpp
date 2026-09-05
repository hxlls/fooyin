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

#include "sftpclientmanager.h"

#include <utils/settings/settingsmanager.h>

#include <QThread>
#include <QVariantMap>

#include <future>
#include <mutex>

using namespace Qt::StringLiterals;

namespace Fooyin::Sftp {

namespace {
//! Settings prefix holding per-server credentials, keyed by "sftp/<host:port>".
constexpr auto SettingsPrefix = "sftp/";
} // namespace

QString settingsKeyFor(const QString& authority)
{
    return QStringLiteral("sftp/") + authority;
}

SftpClientManager::SftpClientManager(SettingsManager* settings, QObject* parent)
    : QObject{parent}
    , m_settings{settings}
{ }

SftpClientManager::~SftpClientManager()
{
    qDeleteAll(m_clients);
}

QString SftpClientManager::authority(const QUrl& url) const
{
    if(url.port() > 0) {
        return QStringLiteral("%1:%2").arg(url.host()).arg(url.port());
    }
    return url.host();
}

SftpClient* SftpClientManager::createClient(const QString& key, const QUrl& url)
{
    SftpClientConfig config;
    config.host = url.host();
    config.port = url.port(22);

    const QVariantMap entry = m_settings->fileValue(settingsKeyFor(key)).toMap();
    if(entry.isEmpty()) {
        qWarning() << "No stored credentials for SFTP server" << key
                   << "- add the library through the settings UI first.";
    }
    else {
        config.user     = entry.value(QStringLiteral("user")).toString();
        config.password = entry.value(QStringLiteral("password")).toString();
    }

    return new SftpClient(config, this);
}

SftpClient* SftpClientManager::clientFor(const QUrl& url)
{
    const QString key = authority(url);

    {
        const std::scoped_lock lock{m_mutex};
        if(auto* existing = m_clients.value(key)) {
            return existing;
        }
    }

    // A client owns a QThread and its session, so it must be created on this
    // object's thread. Callers run on scanner or decoder worker threads, so the
    // creation request is queued here and waited for (same pattern as the
    // WebDAV client manager).
    if(QThread::currentThread() == thread()) {
        const std::scoped_lock lock{m_mutex};
        if(auto* existing = m_clients.value(key)) {
            return existing;
        }
        auto* client = createClient(key, url);
        m_clients.insert(key, client);
        return client;
    }

    std::promise<SftpClient*> created;
    std::future<SftpClient*> future = created.get_future();

    QMetaObject::invokeMethod(
        this,
        [this, key, url, &created]() {
            SftpClient* client{nullptr};
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

} // namespace Fooyin::Sftp
