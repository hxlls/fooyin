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

#include "sftpplugin.h"

#include "sftpinput.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Sftp {

SftpPlugin::SftpPlugin() = default;

SftpPlugin::~SftpPlugin() = default;

void SftpPlugin::initialise(const CorePluginContext& context)
{
    m_manager = new SftpClientManager(context.settingsManager, this);

    m_dirProvider = new SftpDirProvider(m_manager);
    libraryDirProviderRegistry()->addProvider(m_dirProvider);
}

QString SftpPlugin::inputName() const
{
    return u"SFTP"_s;
}

InputCreator SftpPlugin::inputCreator() const
{
    InputCreator creator;
    creator.decoder = [manager = m_manager]() {
        return std::make_unique<SftpDecoder>(manager);
    };
    creator.reader = [manager = m_manager]() {
        return std::make_unique<SftpReader>(manager);
    };
    return creator;
}

} // namespace Fooyin::Sftp

#include "moc_sftpplugin.cpp"
