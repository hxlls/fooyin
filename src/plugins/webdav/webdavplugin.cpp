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

WebdavPlugin::WebdavPlugin()
    : m_client{nullptr}
{ }

WebdavPlugin::~WebdavPlugin() = default;

void WebdavPlugin::initialise(const CorePluginContext& context)
{
    Q_UNUSED(context);

    // Owned by the plugin so that the shared network thread outlives every
    // decoder/reader instance created from inputCreator().
    m_client = new WebdavClient(this);
}

QString WebdavPlugin::inputName() const
{
    return u"WebDAV"_s;
}

InputCreator WebdavPlugin::inputCreator() const
{
    InputCreator creator;
    creator.decoder = [client = m_client]() { return std::make_unique<WebdavDecoder>(client); };
    creator.reader  = [client = m_client]() { return std::make_unique<WebdavReader>(client); };
    return creator;
}

} // namespace Fooyin::Webdav

#include "moc_webdavplugin.cpp"
