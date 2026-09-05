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

#include <QFile>
#include <QIODevice>
#include <QTemporaryFile>

namespace Fooyin::Ftp {

/*!
 * Random-access QIODevice over an FTP/FTPS resource.
 *
 * FTP has no HTTP-style range semantics and its data connections are not
 * reusable, so random access by seeking is catastrophically slow (TagLib issues
 * hundreds of tiny reads). This device therefore downloads the whole resource
 * to a temporary spool file once at open() and serves all further reads from
 * it — identical to a local file from then on. The download happens on the
 * network thread of the matching FtpClient; open() blocks until it finishes.
 */
class FtpDevice : public QIODevice
{
    Q_OBJECT

public:
    FtpDevice(FtpClient* client, QString remotePath, QObject* parent = nullptr);

    [[nodiscard]] bool isSequential() const override;
    [[nodiscard]] qint64 size() const override;
    [[nodiscard]] qint64 bytesAvailable() const override;
    [[nodiscard]] bool atEnd() const override;

    bool open(OpenMode mode) override;
    void close() override;
    bool seek(qint64 pos) override;

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    FtpClient* m_client;
    QString m_remotePath;

    QTemporaryFile m_spool; //!< Full-file download target; removed on destruction.
    QFile m_local;          //!< Read handle onto the closed spool.
};

} // namespace Fooyin::Ftp
