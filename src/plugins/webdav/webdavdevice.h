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

#include <QIODevice>
#include <QUrl>

namespace Fooyin::Webdav {

/*!
 * Random-access QIODevice over a WebDAV resource.
 *
 * Data is fetched with HTTP Range requests and held in a read-ahead window.
 * The window is essential: TagLib resolves tags with hundreds of tiny
 * 4–16 byte reads, and issuing one HTTP request per read would make scanning a
 * remote library unusable.
 *
 * Servers that ignore Range requests (returning a 200 body instead of 206)
 * cannot support the random access that audio decoding and tag parsing
 * require, so open() refuses them outright.
 */
class WebdavDevice : public QIODevice
{
    Q_OBJECT

public:
    WebdavDevice(WebdavClient* client, QUrl url, QObject* parent = nullptr);

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
    //! Fetches [start, end] and replaces the window. Returns false on transport failure.
    bool fetchRange(qint64 start, qint64 end);

    WebdavClient* m_client;
    QUrl m_url;

    qint64 m_size{0};

    QByteArray m_window;
    qint64 m_windowStart{0};
    qint64 m_windowEnd{0};

    //! Bytes requested per HTTP round trip.
    static constexpr qint64 ReadAheadBytes = 512 * 1024;
};

} // namespace Fooyin::Webdav
