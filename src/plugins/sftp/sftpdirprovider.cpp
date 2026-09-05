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

#include "sftpdirprovider.h"

#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin::Sftp {

namespace {
//! Rebuilds a child sftp(s) URL: base authority of @p baseUrl + @p remotePath
//! (already the remote absolute path, UTF-8). QUrl::setPath re-encodes so the
//! stored URL round-trips losslessly.
QUrl childUrl(const QUrl& baseUrl, const QString& remotePath)
{
    QUrl url{baseUrl};
    url.setPath(remotePath);
    return url;
}
} // namespace

SftpDirProvider::SftpDirProvider(SftpClientManager* manager)
    : m_manager{manager}
{ }

QStringList SftpDirProvider::schemes() const
{
    return {QStringLiteral("sftp")};
}

std::optional<QList<LibraryEntry>> SftpDirProvider::listDirectory(const QUrl& url, QString* error) const
{
    if(!m_manager) {
        return {};
    }

    SftpClient* const client = m_manager->clientFor(url);
    if(!client) {
        if(error) {
            *error = u"Could not create a connection to "_s + url.toString();
        }
        return {};
    }

    QString dirPath = url.path();
    if(dirPath.isEmpty()) {
        dirPath = u"/"_s;
    }

    QString errorDetail;
    const auto children = client->listDirectory(dirPath, &errorDetail);
    if(!children.has_value()) {
        if(error) {
            *error = errorDetail.isEmpty() ? u"Failed to list %1"_s.arg(url.toString()) : errorDetail;
        }
        return {};
    }

    QList<LibraryEntry> entries;
    entries.reserve(static_cast<qsizetype>(children->size()));
    for(const auto& child : children.value()) {
        LibraryEntry entry;
        entry.isDir      = child.isDir;
        entry.size       = child.size;
        entry.modifiedMs = child.modifiedMs;
        entry.path       = childUrl(url, child.path).toString();
        entries.push_back(entry);
    }

    return entries;
}

} // namespace Fooyin::Sftp
