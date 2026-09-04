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

#include "webdavclientmanager.h"

#include <core/library/librarydirprovider.h>

namespace Fooyin::Webdav {

/*!
 * LibraryDirProvider for webdav/webdavs library roots.
 *
 * Lets the core library scanner enumerate a virtual webdav(s):// root through
 * the registered LibraryDirProviderRegistry: each directory is listed one level
 * at a time with a PROPFIND through the per-server client, mirroring what the
 * decoder/reader do for playback. Children keep their webdav(s):// scheme, so
 * discovered tracks remain routable by AudioLoader when read.
 *
 * Registered by WebdavPlugin during initialise().
 */
class WebdavDirProvider : public LibraryDirProvider
{
public:
    explicit WebdavDirProvider(WebdavClientManager* manager);

    [[nodiscard]] QStringList schemes() const override;
    [[nodiscard]] std::optional<QList<LibraryEntry>> listDirectory(const QUrl& url, QString* error) const override;

private:
    WebdavClientManager* m_manager;
};

} // namespace Fooyin::Webdav
