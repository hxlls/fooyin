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
#include "webdavdirprovider.h"

#include <core/engine/inputplugin.h>
#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>

namespace Fooyin::Webdav {

/*!
 * Registers WebDAV as an audio input backend.
 *
 * The plugin only supplies the transport. Decoding and tag parsing are
 * delegated to fooyin's FFmpeg and TagLib backends through
 * AudioSource::device, so this plugin inherits the full set of supported
 * formats without duplicating any parsing logic.
 *
 * Registration is handled by the core: application.cpp calls inputCreator()
 * for every InputPlugin and feeds the results to AudioLoader. Because the
 * decoder and reader advertise the webdav/webdavs schemes, AudioLoader routes
 * those URIs here via prepareSchemeSource().
 */
class WebdavPlugin : public QObject,
                     public Plugin,
                     public CorePlugin,
                     public InputPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin" FILE "webdav.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::InputPlugin)

public:
    WebdavPlugin();
    ~WebdavPlugin() override;

    void initialise(const CorePluginContext& context) override;

    [[nodiscard]] QString inputName() const override;
    [[nodiscard]] InputCreator inputCreator() const override;

private:
    WebdavClientManager* m_manager;
    WebdavDirProvider* m_dirProvider;
};

} // namespace Fooyin::Webdav
