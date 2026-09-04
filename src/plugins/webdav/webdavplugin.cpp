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

#include "webdavplugin.h"

#include "webdavinput.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Webdav {

WebdavPlugin::WebdavPlugin() = default;

WebdavPlugin::~WebdavPlugin() = default;

void WebdavPlugin::initialise(const CorePluginContext& context)
{
    // One client per server authority is created lazily by the manager (see
    // WebdavClientManager). The manager must outlive every decoder/reader the
    // inputCreator() below produces, hence it is owned here.
    m_manager = new WebdavClientManager(context.settingsManager, this);

    // Register this plugin as the directory provider for webdav/webdavs library
    // roots so the library scanner can enumerate virtual sources.
    m_dirProvider = new WebdavDirProvider(m_manager);
    libraryDirProviderRegistry()->addProvider(m_dirProvider);
}

QString WebdavPlugin::inputName() const
{
    return u"WebDAV"_s;
}

InputCreator WebdavPlugin::inputCreator() const
{
    InputCreator creator;
    creator.decoder = [manager = m_manager]() {
        return std::make_unique<WebdavDecoder>(manager);
    };
    creator.reader = [manager = m_manager]() {
        return std::make_unique<WebdavReader>(manager);
    };
    return creator;
}

} // namespace Fooyin::Webdav

#include "moc_webdavplugin.cpp"
