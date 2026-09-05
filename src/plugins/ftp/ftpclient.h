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
#include <QString>

#include <chrono>
#include <mutex>
#include <optional>

namespace Fooyin::Ftp {

struct FtpEntry
{
    QString path; //!< Full remote path (used for subsequent requests).
    QString name; //!< Display name.
    bool isDir{false};
    qint64 size{0};
    qint64 modifiedMs{0};
};

//! TLS handling for the control/data connection.
enum class FtpSecurity
{
    Plain,    //!< Plain FTP.
    Explicit, //!< AUTH TLS on the control port (the common "FTPS", port 21).
    Implicit, //!< TLS from the first byte (port 990).
};

struct FtpClientConfig
{
    QString host;
    int port{21};
    QString user;
    QString password;
    FtpSecurity security{FtpSecurity::Plain};
    //! Disable TLS peer verification (self-signed / IP-to-certificate mismatch).
    bool insecureSsl{false};
    std::chrono::milliseconds timeout{30000};
};

/*!
 * Synchronous FTP/FTPS transport backed by libcurl.
 *
 * Unlike WebdavClient/SftpClient this class needs no dedicated network thread:
 * libcurl's blocking easy interface runs directly on the calling thread, which
 * fooyin's scanner and decoder threads can afford (they already block on local
 * file I/O). A per-client mutex serialises operations, because the library
 * scanner and a decoder may touch the same server simultaneously and a libcurl
 * easy handle must not be used concurrently. One blocking call at a time is
 * acceptable: FTP traffic is inherently serial anyway.
 */
class FtpClient
{
public:
    explicit FtpClient(const FtpClientConfig& config);
    ~FtpClient();

    FtpClient(const FtpClient&)            = delete;
    FtpClient& operator=(const FtpClient&) = delete;

    //! Queries size and modification time of @p remotePath. nullopt on failure.
    [[nodiscard]] std::optional<qint64> fileSize(const QString& remotePath, QString* errorDetail = nullptr);
    //! Lists @p remotePath's direct children (MLSD). nullopt on failure.
    [[nodiscard]] std::optional<QList<FtpEntry>> listDirectory(const QString& remotePath,
                                                               QString* errorDetail = nullptr);
    //! Downloads @p remotePath fully into @p localFile. False on failure.
    [[nodiscard]] bool download(const QString& remotePath, const QString& localFile, QString* errorDetail = nullptr);

private:
    class Private;
    Private* d{nullptr};
};

} // namespace Fooyin::Ftp
