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

#include "ftpdevice.h"

#include <core/engine/audioinput.h>
#include <core/engine/input/ffmpeg/ffmpeginput.h>
#include <core/engine/input/taglibparser.h>

#include <memory>

namespace Fooyin::Ftp {

/*!
 * Decoder for ftp:// and ftps:// URIs.
 *
 * FtpDevice spools the whole resource to a local temporary file at open(), so
 * FFmpegDecoder sees an ordinary local file afterwards. Playback of a track
 * therefore begins once its download completes.
 */
class FtpDecoder : public AudioDecoder
{
public:
    explicit FtpDecoder(FtpClientManager* manager);

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] QStringList supportedSchemes() const override;
    [[nodiscard]] bool isSeekable() const override;
    [[nodiscard]] int bitrate() const override;
    [[nodiscard]] bool trackHasChanged() const override;
    [[nodiscard]] Track changedTrack() const override;

    std::optional<AudioFormat> init(const AudioSource& source, const Track& track, DecoderOptions options) override;
    void start() override;
    void stop() override;
    void seek(uint64_t pos) override;
    AudioBuffer readBuffer(size_t bytes) override;

private:
    FtpClientManager* m_manager;
    std::unique_ptr<FFmpegDecoder> m_decoder;
    std::unique_ptr<FtpDevice> m_device;
};

/*!
 * Metadata reader for ftp:// and ftps:// URIs.
 */
class FtpReader : public AudioReader
{
public:
    explicit FtpReader(FtpClientManager* manager);

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] QStringList supportedSchemes() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;

    bool init(const AudioSource& source) override;
    bool readTrack(const AudioSource& source, Track& track) override;
    QByteArray readCover(const AudioSource& source, const Track& track, Track::Cover cover) override;

private:
    FtpClientManager* m_manager;
    std::unique_ptr<TagLibReader> m_reader;
    std::unique_ptr<FtpDevice> m_device;
    QString m_filepath;
};

} // namespace Fooyin::Ftp
