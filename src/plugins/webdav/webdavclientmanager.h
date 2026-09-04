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

#include "webdavclient.h"

#include <QHash>
#include <QObject>
#include <QUrl>

namespace Fooyin {
class SettingsManager;
} // namespace Fooyin

namespace Fooyin::Webdav {

/*!
 * Pool of WebdavClient instances, one per server authority (host:port).
 *
 * A single client cannot serve multiple servers at once: credentials and the
 * TLS policy are per-client state, so switching them on a shared client would
 * race whenever two different servers are scanned or played near-simultaneously.
 * This manager therefore caches one client per authority; each client is
 * initialised once from the settings entry stored for that server (user name,
 * password and the "ignore certificate errors" flag) and keeps those settings
 * for its lifetime. Clients are owned by this object and share one network
 * thread each, which is acceptable since requests are serialised per client.
 *
 * All decode/read/listDirectory paths resolve their WebdavClient through
 * clientFor() once the target URL is known.
 */
class WebdavClientManager : public QObject
{
    Q_OBJECT

public:
    explicit WebdavClientManager(SettingsManager* settings, QObject* parent = nullptr);
    ~WebdavClientManager() override;

    /*!
     * Returns the client for @p url's authority (host:port), creating and
     * configuring it from the settings on first use. Falls back to the raw host
     * when the URL carries no explicit port.
     */
    [[nodiscard]] WebdavClient* clientFor(const QUrl& url);

private:
    [[nodiscard]] QString authority(const QUrl& url) const;

    SettingsManager* m_settings;
    QHash<QString, WebdavClient*> m_clients;
};

} // namespace Fooyin::Webdav
