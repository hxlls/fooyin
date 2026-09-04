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

#include "fycore_export.h"

#include "libraryscantypes.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>

namespace Fooyin {

/*!
 * A source that can enumerate the children of a virtual (non-local) library
 * root. Local libraries are enumerated by LibraryFileEnumerator with QDir and
 * never go through this interface.
 *
 * Implementations are registered by plugins (e.g. the WebDAV audio input
 * plugin) with LibraryDirProviderRegistry. The scanner asks the registry for
 * the provider matching a library root's URL scheme and recursively lists the
 * tree one directory at a time; the recursion itself lives in the core
 * enumerator so every source shares the same traversal logic.
 */
class FYCORE_EXPORT LibraryDirProvider
{
public:
    virtual ~LibraryDirProvider() = default;

    //! URL schemes this provider handles, e.g. {"webdav", "webdavs"}.
    [[nodiscard]] virtual QStringList schemes() const = 0;

    /*!
     * Returns the direct children of @p url (files and directories). Only one
     * level is listed; the scanner recurses into directories. Children keep the
     * same scheme so their own paths stay routable (a webdavs:// child stays
     * webdavs://).
     * @returns nullopt on transport/authentication failure, with @p error set.
     */
    [[nodiscard]] virtual std::optional<QList<LibraryEntry>> listDirectory(const QUrl& url, QString* error) const = 0;
};

/*!
 * Registry of library directory providers, keyed by URL scheme.
 *
 * Owned by LibraryManager so both the scanner (core) and the GUI can query it,
 * while plugins register their provider during CorePlugin::initialise via the
 * CorePluginContext's LibraryManager. This mirrors how audio input backends are
 * registered: core defines the seam, plugins fill it.
 */
class FYCORE_EXPORT LibraryDirProviderRegistry
{
public:
    virtual ~LibraryDirProviderRegistry() = default;

    //! Registers (or replaces) the provider for its schemes.
    virtual void addProvider(LibraryDirProvider* provider) = 0;
    //! Provider handling @p scheme, or nullptr if none is registered.
    [[nodiscard]] virtual LibraryDirProvider* providerForScheme(const QString& scheme) const = 0;
    //! All schemes with a registered provider (drives the GUI source-type menu).
    [[nodiscard]] virtual QStringList supportedSchemes() const = 0;
};

/*!
 * Returns the process-wide registry of library directory providers.
 *
 * A function-local singleton keeps the seam dependency-free: the scanner and the
 * GUI query it directly, and plugins register their provider during initialise.
 * @note Registered providers live for the application lifetime (plugins are not
 *       unloaded at runtime), so the registry never dangles.
 */
FYCORE_EXPORT LibraryDirProviderRegistry* libraryDirProviderRegistry();

} // namespace Fooyin
