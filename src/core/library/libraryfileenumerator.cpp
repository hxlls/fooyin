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

#include "libraryfileenumerator.h"

#include "libraryscanstate.h"
#include "libraryscanutils.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin {

namespace {
/*!
 * Builds a LibraryEntry from a local file-system QFileInfo, carrying the file
 * attributes that a local scan can read cheaply.
 */
LibraryEntry entryFromFileInfo(const QFileInfo& info)
{
    LibraryEntry entry;
    entry.path       = info.absoluteFilePath();
    entry.isDir      = info.isDir();
    entry.size       = info.isFile() ? info.size() : 0;
    const QDateTime modified = info.lastModified();
    entry.modifiedMs = modified.isValid() ? static_cast<qint64>(modified.toMSecsSinceEpoch()) : 0;
    return entry;
}

//! Lower-case file suffix for @p path. Local paths and virtual (scheme) URLs
//! both resolve to a trailing ".ext".
QString entrySuffix(const QString& path)
{
    QString filePart = path;
    if(Track::isVirtualPath(path)) {
        filePart = QUrl{path}.path();
    }
    const int dot = filePart.lastIndexOf(u'.');
    return dot >= 0 ? filePart.sliced(dot + 1).toLower() : QString{};
}
} // namespace

LibraryFileEnumerator::LibraryFileEnumerator(LibraryScanState* state, EnumeratedFileHandler handler)
    : m_state{state}
    , m_handler{std::move(handler)}
{ }

bool LibraryFileEnumerator::enumerateFiles(const QStringList& paths, const QStringList& trackExtensions,
                                           const QStringList& playlistExtensions)
{
    std::set<QString> visitedDirs;

    auto fileType
        = [&trackExtensions, &playlistExtensions](const QString& path) -> std::optional<EnumeratedFileType> {
        const QString suffix = entrySuffix(path);

        if(suffix == "cue"_L1 && trackExtensions.contains(u"cue"_s)) {
            return EnumeratedFileType::Cue;
        }
        if(trackExtensions.contains(suffix)) {
            return EnumeratedFileType::Track;
        }
        if(playlistExtensions.contains(suffix)) {
            return EnumeratedFileType::Playlist;
        }

        return {};
    };

    auto emitFile = [this, &fileType](const LibraryEntry& entry) -> bool {
        if(!m_state->mayRun()) {
            return false;
        }

        if(const auto type = fileType(entry.path)) {
            const QString filepath = normalisePath(entry.path);
            if(!m_state->rememberDiscoveredPath(filepath)) {
                return true;
            }

            m_state->fileDiscovered(filepath);
            return m_handler(entry, type.value());
        }

        return true;
    };

    for(const auto& path : paths) {
        if(!m_state->mayRun()) {
            return false;
        }

        // A library root may be a local directory or a virtual (scheme) path.
        if(Track::isVirtualPath(path)) {
            // Virtual enumeration is supplied by the audio input backend layer;
            // the local enumerator only walks real directories. A caller that
            // adds a virtual library root must provide a directory provider
            // (future work); here we simply accept an empty traversal so a
            // manual scan over a virtual root does not attempt local I/O.
            const QString normalisedRoot = normalisePath(path);
            if(!visitedDirs.emplace(normalisedRoot).second) {
                continue;
            }
            continue;
        }

        const QFileInfo info{path};
        if(!info.exists()) {
            continue;
        }

        const QString normalisedPath = normalisePath(info.absoluteFilePath());
        if(info.isDir()) {
            if(!processDirectory(normalisedPath, trackExtensions, playlistExtensions, visitedDirs)) {
                return false;
            }
            continue;
        }

        const LibraryEntry entry = entryFromFileInfo(info);
        if(const auto type = fileType(entry.path); type && type == EnumeratedFileType::Track) {
            if(const auto cue = findMatchingCue(info)) {
                if(!emitFile(entryFromFileInfo(cue.value()))) {
                    return false;
                }
            }
        }

        if(!emitFile(entry)) {
            return false;
        }
    }

    return true;
}

bool LibraryFileEnumerator::processDirectory(const QString& path, const QStringList& trackExtensions,
                                             const QStringList& playlistExtensions, std::set<QString>& visitedDirs)
{
    if(!m_state->mayRun()) {
        return false;
    }

    const QFileInfo dirInfo{path};
    const QString canonicalPath = dirInfo.canonicalFilePath().isEmpty() ? path : dirInfo.canonicalFilePath();
    if(!visitedDirs.emplace(canonicalPath).second) {
        return true;
    }

    const QDir dir{path};
    const QFileInfoList entries
        = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    QFileInfoList cueFiles;
    for(const auto& entry : entries) {
        if(entry.isFile() && entry.suffix().compare(u"cue"_s, Qt::CaseInsensitive) == 0
           && trackExtensions.contains(u"cue"_s)) {
            cueFiles.push_back(entry);
        }
    }

    auto emitTypedFile = [this](const LibraryEntry& entry, const EnumeratedFileType type) {
        const QString filepath = normalisePath(entry.path);
        if(!m_state->rememberDiscoveredPath(filepath)) {
            return true;
        }

        m_state->fileDiscovered(filepath);
        return m_handler(entry, type);
    };

    for(const auto& cue : cueFiles) {
        if(!emitTypedFile(entryFromFileInfo(cue), EnumeratedFileType::Cue)) {
            return false;
        }
    }

    for(const auto& entryInfo : entries) {
        if(entryInfo.isDir()) {
            if(!processDirectory(normalisePath(entryInfo.absoluteFilePath()), trackExtensions, playlistExtensions,
                                 visitedDirs)) {
                return false;
            }
            continue;
        }

        const LibraryEntry entry = entryFromFileInfo(entryInfo);
        const QString suffix      = entrySuffix(entry.path);
        if(suffix == "cue"_L1 && trackExtensions.contains(u"cue"_s)) {
            continue;
        }

        if(trackExtensions.contains(suffix)) {
            if(const auto cue = findMatchingCue(entryInfo, cueFiles)) {
                if(!emitTypedFile(entryFromFileInfo(cue.value()), EnumeratedFileType::Cue)) {
                    return false;
                }
            }

            if(!emitTypedFile(entry, EnumeratedFileType::Track)) {
                return false;
            }
            continue;
        }

        if(playlistExtensions.contains(suffix)) {
            if(!emitTypedFile(entry, EnumeratedFileType::Playlist)) {
                return false;
            }
        }
    }

    return true;
}
} // namespace Fooyin
