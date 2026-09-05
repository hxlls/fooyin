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

#include "sftpdevice.h"

#include <algorithm>

using namespace Qt::StringLiterals;

namespace Fooyin::Sftp {

SftpDevice::SftpDevice(SftpClient* client, QString remotePath, QObject* parent)
    : QIODevice{parent}
    , m_client{client}
    , m_remotePath{std::move(remotePath)}
{ }

bool SftpDevice::isSequential() const
{
    return false;
}

qint64 SftpDevice::size() const
{
    return m_size;
}

qint64 SftpDevice::bytesAvailable() const
{
    return QIODevice::bytesAvailable();
}

bool SftpDevice::atEnd() const
{
    if(m_size > 0) {
        return pos() >= m_size;
    }
    return QIODevice::atEnd();
}

bool SftpDevice::open(OpenMode mode)
{
    if((mode & QIODevice::ReadOnly) == 0) {
        setErrorString(u"SFTP devices are read-only"_s);
        return false;
    }

    QString error;
    const auto info = m_client->stat(m_remotePath, &error);
    if(!info.has_value()) {
        setErrorString(u"Failed to open %1: %2"_s.arg(m_remotePath, error));
        return false;
    }

    m_size = info->size;
    m_window.clear();
    m_windowStart = 0;
    m_windowEnd   = 0;

    if(!fetchRange(0, ReadAheadBytes)) {
        return false;
    }

    setOpenMode(mode | QIODevice::ReadOnly);
    QIODevice::seek(0);
    return true;
}

void SftpDevice::close()
{
    m_window.clear();
    m_windowStart = 0;
    m_windowEnd   = 0;
    setOpenMode(QIODevice::NotOpen);
    QIODevice::close();
}

bool SftpDevice::fetchRange(qint64 start, qint64 length)
{
    QString error;
    const QByteArray data = m_client->readAt(m_remotePath, start, length, &error);
    if(data.isEmpty() && length > 0) {
        // Distinguish "0 bytes returned mid-file" (transport failure) from a
        // legitimate read past EOF, which ensureBuffered never issues.
        setErrorString(u"Range read failed: %1"_s.arg(error));
        return false;
    }

    m_window      = data;
    m_windowStart = start;
    m_windowEnd   = m_windowStart + m_window.size();
    return true;
}

bool SftpDevice::ensureBuffered(qint64 pos)
{
    if(m_size > 0 && pos >= m_size) {
        return false; // beyond EOF; caller treats this as end of stream.
    }

    if(pos >= m_windowStart && pos < m_windowEnd) {
        return true;
    }

    // Clamp the fetch length to the known size so we never read past EOF.
    qint64 length = ReadAheadBytes;
    if(m_size > 0 && pos + length > m_size) {
        length = m_size - pos;
    }
    if(length <= 0) {
        return false;
    }

    return fetchRange(pos, length);
}

qint64 SftpDevice::readData(char* data, qint64 maxSize)
{
    if(maxSize <= 0) {
        return 0;
    }

    const qint64 current = pos();

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

qint64 SftpDevice::writeData(const char*, qint64)
{
    setErrorString(u"SFTP devices are read-only"_s);
    return -1;
}

bool SftpDevice::seek(qint64 pos)
{
    if(!isOpen()) {
        return false;
    }

    if(pos < 0 || (m_size > 0 && pos > m_size)) {
        return false;
    }

    return QIODevice::seek(pos);
}

} // namespace Fooyin::Sftp

#include "moc_sftpdevice.cpp"
