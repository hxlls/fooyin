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

#include "sftpdevice.h"

#include <core/engine/audioinput.h>
#include <core/engine/input/ffmpeg/ffmpeginput.h>
#include <core/engine/input/taglibparser.h>

#include <memory>

namespace Fooyin::Sftp {

/*!
 * Decoder for sftp:// URIs.
 *
 * Transport shim only: opens the remote resource as a seekable QIODevice
 * (SftpDevice) and hands it to FFmpegDecoder through AudioSource::device. All
 * demuxing, decoding and seeking behaviour is inherited unchanged, so every
 * format FFmpeg supports works over SFTP.
 */
class SftpDecoder : public AudioDecoder
{
public:
    explicit SftpDecoder(SftpClientManager* manager);

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
    SftpClientManager* m_manager;
    std::unique_ptr<FFmpegDecoder> m_decoder;
    std::unique_ptr<SftpDevice> m_device;
};

/*!
 * Metadata reader for sftp:// URIs.
 *
 * Mirrors SftpDecoder: the remote resource is exposed as a seekable device and
 * TagLibReader does the parsing. Writing is disabled: SFTP POSIX rename has no
 * cross-platform write path in fooyin's tagger, so remote tagging is out of
 * scope for now.
 */
class SftpReader : public AudioReader
{
public:
    explicit SftpReader(SftpClientManager* manager);

    [[nodiscard]] QStringList extensions() const override;
    [[nodiscard]] QStringList supportedSchemes() const override;
    [[nodiscard]] bool canReadCover() const override;
    [[nodiscard]] bool canWriteMetaData() const override;

    bool init(const AudioSource& source) override;
    bool readTrack(const AudioSource& source, Track& track) override;
    QByteArray readCover(const AudioSource& source, const Track& track, Track::Cover cover) override;

private:
    SftpClientManager* m_manager;
    std::unique_ptr<TagLibReader> m_reader;
    std::unique_ptr<SftpDevice> m_device;
    QString m_filepath;
};

} // namespace Fooyin::Sftp
