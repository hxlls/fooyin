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

#include "sftpclient.h"

#include <QHash>
#include <QObject>
#include <QUrl>

#include <mutex>

namespace Fooyin {
class SettingsManager;
} // namespace Fooyin

namespace Fooyin::Sftp {

/*!
 * Pool of SftpClient instances, one per server authority (host:port).
 *
 * Each client keeps its own SSH session, credentials and host key state, so
 * different servers never share state. Clients are created on this object's
 * thread (which owns their network thread) and reused for scanning, directory
 * listing and playback; requests are serialised per client on its network
 * thread.
 *
 * Credentials are read from the settings entry written by the library GUI,
 * keyed by "sftp/<host:port>".
 */
class SftpClientManager : public QObject
{
    Q_OBJECT

public:
    explicit SftpClientManager(SettingsManager* settings, QObject* parent = nullptr);
    ~SftpClientManager() override;

    /*!
     * Returns the client for @p url's authority, creating and configuring it
     * from the settings on first use.
     */
    [[nodiscard]] SftpClient* clientFor(const QUrl& url);

private:
    [[nodiscard]] QString authority(const QUrl& url) const;
    [[nodiscard]] SftpClient* createClient(const QString& key, const QUrl& url);

    SettingsManager* m_settings;
    QHash<QString, SftpClient*> m_clients;
    std::mutex m_mutex;
};

} // namespace Fooyin::Sftp
