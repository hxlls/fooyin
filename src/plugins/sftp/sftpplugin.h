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
#include "sftpdirprovider.h"

#include <core/engine/inputplugin.h>
#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>

namespace Fooyin::Sftp {

/*!
 * Registers SFTP as an audio input backend and library source.
 *
 * Like the WebDAV plugin, this supplies only the transport: decoding and tag
 * parsing are delegated to fooyin's FFmpeg and TagLib backends through
 * AudioSource::device, so all supported formats work unchanged. The plugin also
 * registers a LibraryDirProvider so an sftp:// root can be scanned as a library
 * source (credentials come from the settings UI, keyed by host:port).
 */
class SftpPlugin : public QObject,
                   public Plugin,
                   public CorePlugin,
                   public InputPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin" FILE "sftp.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::InputPlugin)

public:
    SftpPlugin();
    ~SftpPlugin() override;

    void initialise(const CorePluginContext& context) override;

    [[nodiscard]] QString inputName() const override;
    [[nodiscard]] InputCreator inputCreator() const override;

private:
    SftpClientManager* m_manager;
    SftpDirProvider* m_dirProvider;
};

} // namespace Fooyin::Sftp
