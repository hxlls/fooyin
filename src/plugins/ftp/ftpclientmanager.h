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

#include "ftpclient.h"

#include <QHash>
#include <QObject>
#include <QUrl>

#include <mutex>

namespace Fooyin {
class SettingsManager;
} // namespace Fooyin

namespace Fooyin::Ftp {

/*!
 * Pool of FtpClient instances, one per server authority (host:port).
 *
 * Same design as the SFTP/WebDAV managers: clients are created on this
 * object's thread and reused for listing, spool downloads and playback. All
 * requests of one client run serialised on its own network thread.
 */
class FtpClientManager : public QObject
{
    Q_OBJECT

public:
    explicit FtpClientManager(SettingsManager* settings, QObject* parent = nullptr);
    ~FtpClientManager() override;

    [[nodiscard]] FtpClient* clientFor(const QUrl& url);

private:
    [[nodiscard]] QString authority(const QUrl& url) const;
    [[nodiscard]] FtpClient* createClient(const QString& key, const QUrl& url);

    SettingsManager* m_settings;
    QHash<QString, FtpClient*> m_clients;
    std::mutex m_mutex;
};

} // namespace Fooyin::Ftp
