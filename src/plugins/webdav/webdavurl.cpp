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

#include "webdavurl.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Webdav {

namespace {
constexpr auto SchemeLatin       = "webdav";
constexpr auto SecureSchemeLatin = "webdavs";
} // namespace

bool isWebdavPath(const QString& path)
{
    if(path.isEmpty()) {
        return false;
    }

    const QUrl url{path, QUrl::StrictMode};
    if(!url.isValid() || url.scheme().isEmpty()) {
        return false;
    }

    const QString scheme = url.scheme().toLower();
    return scheme == QLatin1String(SchemeLatin) || scheme == QLatin1String(SecureSchemeLatin);
}

std::optional<QUrl> toHttpUrl(const QString& path)
{
    if(!isWebdavPath(path)) {
        return {};
    }

    const QUrl url{path, QUrl::StrictMode};
    const bool secure = url.scheme().compare(QLatin1String(SecureSchemeLatin), Qt::CaseInsensitive) == 0;

    // Rewriting the scheme in place preserves any percent-encoding in the path,
    // which round-tripping through QUrl's setters would risk normalising away.
    QString wire{path};
    wire.replace(0, url.scheme().length(), secure ? "https"_L1 : "http"_L1);

    const QUrl result{wire, QUrl::StrictMode};
    if(!result.isValid()) {
        return {};
    }
    return result;
}

QString fromHttpUrl(const QUrl& url)
{
    if(!url.isValid()) {
        return {};
    }

    const bool secure = url.scheme().compare("https"_L1, Qt::CaseInsensitive) == 0;
    if(!secure && url.scheme().compare("http"_L1, Qt::CaseInsensitive) != 0) {
        return url.toString();
    }

    QUrl dav{url};
    dav.setScheme(secure ? QLatin1String(SecureSchemeLatin) : QLatin1String(SchemeLatin));
    // Credentials must never be persisted in Tracks.FilePath.
    dav.setUserName({});
    dav.setPassword({});
    return dav.toString();
}

QString encodePath(const QString& path)
{
    const QStringList segments = path.split(u'/', Qt::KeepEmptyParts);
    QStringList encoded;
    encoded.reserve(segments.size());

    for(const QString& segment : segments) {
        encoded.push_back(QString::fromUtf8(segment.toUtf8().toPercentEncoding()));
    }

    return encoded.join(u'/');
}

QString joinPath(const QString& dir, const QString& name)
{
    if(dir.isEmpty()) {
        return name;
    }
    if(dir.endsWith(u'/')) {
        return dir + name;
    }
    return dir + u'/' + name;
}

} // namespace Fooyin::Webdav
