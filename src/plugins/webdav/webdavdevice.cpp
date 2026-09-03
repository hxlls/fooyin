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

#include "webdavdevice.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin::Webdav {

WebdavDevice::WebdavDevice(WebdavClient* client, QUrl url, QObject* parent)
    : QIODevice{parent}
    , m_client{client}
    , m_url{std::move(url)}
{ }

bool WebdavDevice::isSequential() const
{
    // WebDAV audio relies on random access (seek during decode, tag reads from
    // arbitrary offsets). Devices that cannot honour Range requests are
    // rejected at open(), so this device is always seekable.
    return false;
}

qint64 WebdavDevice::size() const
{
    return m_size;
}

qint64 WebdavDevice::bytesAvailable() const
{
    // The full remainder is only available after a network fetch, so do not
    // report it as immediately readable. This mirrors QFile semantics.
    return QIODevice::bytesAvailable();
}

bool WebdavDevice::atEnd() const
{
    if(m_size > 0) {
        return pos() >= m_size;
    }
    return QIODevice::atEnd();
}

bool WebdavDevice::open(OpenMode mode)
{
    if((mode & QIODevice::ReadOnly) == 0) {
        setErrorString(u"WebDAV devices are read-only"_s);
        return false;
    }

    // Probe size and Range support with a single request for the first window.
    const WebdavResponse probe = m_client->getRange(m_url, 0, ReadAheadBytes - 1);

    if(!probe.ok() && !probe.isPartial()) {
        setErrorString(u"Failed to open %1: HTTP %2 %3"_s.arg(m_url.toString(), QString::number(probe.status),
                                                              probe.error));
        return false;
    }

    if(!probe.isPartial()) {
        // A 200 here means the server ignored our Range header and returned the
        // whole entity. Random access — required by both FFmpeg seeking and
        // TagLib tag parsing — is impossible, so refuse instead of surfacing a
        // subtly broken sequential device.
        setErrorString(u"Server does not support byte-range requests: %1"_s.arg(m_url.toString()));
        return false;
    }

    // 206 Partial Content. Total size comes from Content-Range.
    m_size        = probe.rangeTotal().value_or(probe.contentLength());
    m_window      = probe.body;
    m_windowStart = 0;
    m_windowEnd   = m_window.size();

    setOpenMode(mode | QIODevice::ReadOnly);
    QIODevice::seek(0);
    return true;
}

void WebdavDevice::close()
{
    m_window.clear();
    m_windowStart = 0;
    m_windowEnd   = 0;
    setOpenMode(QIODevice::NotOpen);
    QIODevice::close();
}

bool WebdavDevice::fetchRange(qint64 start, qint64 end)
{
    const WebdavResponse response = m_client->getRange(m_url, start, end);

    if(!response.isPartial()) {
        setErrorString(u"Range request failed: HTTP %1 %2"_s.arg(QString::number(response.status), response.error));
        return false;
    }

    m_window      = response.body;
    m_windowStart = start;
    m_windowEnd   = m_windowStart + m_window.size();
    return true;
}

bool WebdavDevice::ensureBuffered(qint64 pos)
{
    if(m_size > 0 && pos >= m_size) {
        return false; // beyond EOF; caller treats this as end of stream.
    }

    if(pos >= m_windowStart && pos < m_windowEnd) {
        return true;
    }

    // Clamp the fetch end to the known size so we never issue an out-of-range
    // (416) request past the final byte.
    const qint64 rawEnd = pos + ReadAheadBytes - 1;
    const qint64 end    = (m_size > 0 && rawEnd >= m_size) ? m_size - 1 : rawEnd;
    return fetchRange(pos, std::max(pos, end));
}

qint64 WebdavDevice::readData(char* data, qint64 maxSize)
{
    if(maxSize <= 0) {
        return 0;
    }

    const qint64 current = pos();

    // End-of-file: at or past the known size, do not issue a (possibly
    // out-of-range) request — just signal EOF.
    if(m_size > 0 && current >= m_size) {
        return 0;
    }

    if(!ensureBuffered(current)) {
        return -1;
    }

    const qint64 available = m_windowEnd - current;
    if(available <= 0) {
        return 0;
    }

    const qint64 count = std::min(maxSize, available);
    std::memcpy(data, m_window.constData() + (current - m_windowStart), static_cast<size_t>(count));
    return count;
}

qint64 WebdavDevice::writeData(const char*, qint64)
{
    setErrorString(u"WebDAV devices are read-only"_s);
    return -1;
}

bool WebdavDevice::seek(qint64 pos)
{
    if(!isOpen()) {
        return false;
    }

    if(pos < 0 || (m_size > 0 && pos > m_size)) {
        return false;
    }

    return QIODevice::seek(pos);
}

} // namespace Fooyin::Webdav

#include "moc_webdavdevice.cpp"
