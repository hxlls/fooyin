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

#include "ftpclientmanager.h"
#include "ftpdirprovider.h"

#include <core/engine/inputplugin.h>
#include <core/plugins/coreplugin.h>
#include <core/plugins/plugin.h>

namespace Fooyin::Ftp {

/*!
 * Registers FTP/FTPS as audio input backends and library sources.
 *
 * FtpDevice spools remote files to a local temporary file, so FFmpeg and
 * TagLib see an ordinary local file. The plugin also registers a
 * LibraryDirProvider for ftp/ftps roots.
 */
class FtpPlugin : public QObject,
                  public Plugin,
                  public CorePlugin,
                  public InputPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin" FILE "ftp.json")
    Q_INTERFACES(Fooyin::Plugin Fooyin::CorePlugin Fooyin::InputPlugin)

public:
    FtpPlugin();
    ~FtpPlugin() override;

    void initialise(const CorePluginContext& context) override;

    [[nodiscard]] QString inputName() const override;
    [[nodiscard]] InputCreator inputCreator() const override;

private:
    FtpClientManager* m_manager;
    FtpDirProvider* m_dirProvider;
};

} // namespace Fooyin::Ftp
