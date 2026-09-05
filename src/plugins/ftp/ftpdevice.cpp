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

#include "ftpdevice.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Ftp {

FtpDevice::FtpDevice(FtpClient* client, QString remotePath, QObject* parent)
    : QIODevice{parent}
    , m_client{client}
    , m_remotePath{std::move(remotePath)}
{ }

bool FtpDevice::isSequential() const
{
    return false;
}

qint64 FtpDevice::size() const
{
    return m_local.size();
}

qint64 FtpDevice::bytesAvailable() const
{
    return QIODevice::bytesAvailable();
}

bool FtpDevice::atEnd() const
{
    return pos() >= m_local.size();
}

bool FtpDevice::open(OpenMode mode)
{
    if((mode & QIODevice::ReadOnly) == 0) {
        setErrorString(u"FTP devices are read-only"_s);
        return false;
    }

    // Probe first: fail fast with a clear message if the resource is missing.
    QString error;
    const auto size = m_client->fileSize(m_remotePath, &error);
    if(!size.has_value()) {
        setErrorString(u"Failed to open %1: %2"_s.arg(m_remotePath, error));
        return false;
    }

    if(!m_spool.open()) {
        setErrorString(u"Could not create a local spool file"_s);
        return false;
    }
    const QString spoolName = m_spool.fileName();
    m_spool.close();

    if(!m_client->download(m_remotePath, spoolName, &error)) {
        setErrorString(u"Failed to download %1: %2"_s.arg(m_remotePath, error));
        m_spool.remove();
        return false;
    }

    m_local.setFileName(spoolName);
    if(!m_local.open(QIODevice::ReadOnly)) {
        setErrorString(u"Could not reopen the spool file"_s);
        m_spool.remove();
        return false;
    }

    setOpenMode(mode | QIODevice::ReadOnly);
    QIODevice::seek(0);
    return true;
}

void FtpDevice::close()
{
    m_local.close();
    setOpenMode(QIODevice::NotOpen);
    QIODevice::close();
}

bool FtpDevice::seek(qint64 pos)
{
    if(!isOpen()) {
        return false;
    }
    if(pos < 0 || pos > m_local.size()) {
        return false;
    }
    return QIODevice::seek(pos);
}

qint64 FtpDevice::readData(char* data, qint64 maxSize)
{
    if(maxSize <= 0) {
        return 0;
    }
    if(!m_local.seek(pos())) {
        return -1;
    }
    return m_local.read(data, maxSize);
}

qint64 FtpDevice::writeData(const char*, qint64)
{
    setErrorString(u"FTP devices are read-only"_s);
    return -1;
}

} // namespace Fooyin::Ftp

#include "moc_ftpdevice.cpp"
