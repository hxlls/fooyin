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

#include "sftpclientmanager.h"

#include <core/library/librarydirprovider.h>

#include <QUrl>

namespace Fooyin::Sftp {

/*!
 * LibraryDirProvider implementation: lets the core scanner enumerate an
 * sftp:// library root through the shared virtual-source recursion in
 * LibraryFileEnumerator. Each listDirectory call performs one readdir on the
 * network thread of the matching SftpClient.
 */
class SftpDirProvider : public LibraryDirProvider
{
public:
    explicit SftpDirProvider(SftpClientManager* manager);

    [[nodiscard]] QStringList schemes() const override;
    [[nodiscard]] std::optional<QList<LibraryEntry>> listDirectory(const QUrl& url, QString* error) const override;

private:
    SftpClientManager* m_manager;
};

} // namespace Fooyin::Sftp
