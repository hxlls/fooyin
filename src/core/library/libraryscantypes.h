/*
 * Fooyin
 * Copyright © 2026, Luke Taylor <luket@pm.me>
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

#include "fycore_export.h"

#include <core/library/libraryinfo.h>
#include <core/track.h>

namespace Fooyin {
struct ScanResult;

/*!
 * A single item discovered during library scan enumeration.
 *
 * Replaces the previous `const QFileInfo&` carried through the enumeration
 * callback. `path` is either a local absolute filesystem path or a scheme URL
 * (e.g. a `webdavs://` virtual path handled by an audio input backend); the
 * remaining fields carry the file attributes known at enumeration time and are
 * only populated for local directories (where they come cheaply from
 * QFileInfo). Virtual/remote enumeration supplies size/modified from the
 * directory listing protocol where available.
 */
struct FYCORE_EXPORT LibraryEntry
{
    QString path;
    bool isDir{false};
    qint64 size{0};
    qint64 modifiedMs{0};

    [[nodiscard]] bool isVirtualPath() const
    {
        return Track::isVirtualPath(path);
    }
};

class FYCORE_EXPORT LibraryScanHost
{
public:
    virtual ~LibraryScanHost() = default;

    [[nodiscard]] virtual bool stopRequested() const                                                    = 0;
    virtual void reportProgress(int current, const QString& file, int total, int phase, int discovered) = 0;
    virtual void reportScanUpdate(const ScanResult& result)                                             = 0;
};

struct FYCORE_EXPORT LibraryScanConfig
{
    QStringList libraryRestrictExt;
    QStringList libraryExcludeExt;
    QStringList externalRestrictExt;
    QStringList externalExcludeExt;
    bool playlistSkipMissing{true};
    bool addFoldersIgnorePlaylists{false};
    bool overwriteRatingOnReload{false};
    bool overwritePlaycountOnReload{false};
};

struct FYCORE_EXPORT LibraryScanFilesResult
{
    TrackList tracksScanned;
    TrackList playlistTracksScanned;
};

enum class EnumeratedFileType : uint8_t
{
    Track = 0,
    Playlist,
    Cue,
};
} // namespace Fooyin
