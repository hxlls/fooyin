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

#include <QIODevice>
#include <QUrl>

namespace Fooyin::Sftp {

/*!
 * Random-access QIODevice over an SFTP file.
 *
 * Data is fetched from the server in read-ahead windows with offset reads
 * (sftp_read after a seek). The window is essential: TagLib resolves tags with
 * hundreds of tiny 4–16 byte reads, and each one must not become a network
 * round trip. SFTP supports native offset reads, so seeking is cheap and no
 * whole-file download is needed (unlike FTP).
 */
class SftpDevice : public QIODevice
{
    Q_OBJECT

public:
    SftpDevice(SftpClient* client, QString remotePath, QObject* parent = nullptr);

    [[nodiscard]] bool isSequential() const override;
    [[nodiscard]] qint64 size() const override;
    [[nodiscard]] qint64 bytesAvailable() const override;
    [[nodiscard]] bool atEnd() const override;

    bool open(OpenMode mode) override;
    void close() override;
    //! Move the read cursor. Public like QFile::seek for random-access devices.
    bool seek(qint64 pos) override;

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    //! Ensures the read-ahead window covers @p pos. Returns false on transport failure.
    bool ensureBuffered(qint64 pos);
    //! Fetches @p length bytes at @p start and replaces the window.
    bool fetchRange(qint64 start, qint64 length);

    SftpClient* m_client;
    QString m_remotePath;

    qint64 m_size{-1};

    QByteArray m_window;
    qint64 m_windowStart{0};
    qint64 m_windowEnd{0};

    //! Bytes requested per network round trip.
    static constexpr qint64 ReadAheadBytes = 512 * 1024;
};

} // namespace Fooyin::Sftp
