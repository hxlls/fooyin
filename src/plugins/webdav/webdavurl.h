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

#include <QUrl>

#include <optional>

namespace Fooyin::Webdav {

//! URI scheme handled by this plugin. Registered via AudioDecoder/AudioReader::supportedSchemes().
constexpr auto Scheme       = "webdav";
constexpr auto SecureScheme = "webdavs";

//! True when @p path uses one of the schemes owned by this plugin.
[[nodiscard]] bool isWebdavPath(const QString& path);

/*!
 * Normalises a webdav(s) URI into the http(s) URL used on the wire.
 *
 * webdav://host/path -> http://host/path
 * webdavs://host/path -> https://host/path
 *
 * @returns an empty optional when @p path is not a webdav URI.
 */
[[nodiscard]] std::optional<QUrl> toHttpUrl(const QString& path);

/*!
 * Converts an http(s) URL back into the canonical webdav(s) form stored in the library.
 *
 * Credentials are deliberately stripped so that they are never persisted in
 * Tracks.FilePath.
 */
[[nodiscard]] QString fromHttpUrl(const QUrl& url);

//! Percent-encodes @p path while keeping '/' separators intact.
[[nodiscard]] QString encodePath(const QString& path);

//! Appends @p name to @p dir, ensuring exactly one '/' between them.
[[nodiscard]] QString joinPath(const QString& dir, const QString& name);

} // namespace Fooyin::Webdav
