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

#include "sftpinput.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Sftp {

namespace {
QStringList sftpSchemes()
{
    return {QStringLiteral("sftp")};
}

/*!
 * Builds the AudioSource handed to the wrapped backend: the sftp URI is
 * preserved for display, while device points at the live remote stream.
 */
AudioSource rebindSource(const QString& filepath, QIODevice* device)
{
    AudioSource source;
    source.filepath = filepath;
    source.device   = device;
    if(device->isSequential()) {
        source.size = 0;
    }
    else {
        source.size = device->size();
    }
    return source;
}
} // namespace

SftpDecoder::SftpDecoder(SftpClientManager* manager)
    : m_manager{manager}
{ }

QStringList SftpDecoder::extensions() const
{
    return m_decoder ? m_decoder->extensions() : QStringList{};
}

QStringList SftpDecoder::supportedSchemes() const
{
    return sftpSchemes();
}

bool SftpDecoder::isSeekable() const
{
    return m_decoder && m_device && !m_device->isSequential() && m_decoder->isSeekable();
}

int SftpDecoder::bitrate() const
{
    return m_decoder ? m_decoder->bitrate() : 0;
}

bool SftpDecoder::trackHasChanged() const
{
    return m_decoder && m_decoder->trackHasChanged();
}

Track SftpDecoder::changedTrack() const
{
    return m_decoder ? m_decoder->changedTrack() : Track{};
}

std::optional<AudioFormat> SftpDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    const QUrl url{source.filepath};
    if(!url.isValid() || url.scheme() != u"sftp"_s) {
        return {};
    }

    // Drop any state left over from a previous playback session.
    m_decoder.reset();
    m_device.reset();

    SftpClient* const client = m_manager ? m_manager->clientFor(url) : nullptr;
    if(!client) {
        return {};
    }

    m_device = std::make_unique<SftpDevice>(client, url.path().isEmpty() ? u"/"_s : url.path());
    if(!m_device->open(QIODevice::ReadOnly)) {
        m_device.reset();
        return {};
    }

    // Construct lazily: FFmpegDecoder is move-only and cheap to build.
    m_decoder = std::make_unique<FFmpegDecoder>();

    return m_decoder->init(rebindSource(source.filepath, m_device.get()), track, options);
}

void SftpDecoder::start()
{
    if(m_decoder) {
        m_decoder->start();
    }
}

void SftpDecoder::stop()
{
    if(m_decoder) {
        m_decoder->stop();
    }
    m_decoder.reset();
    m_device.reset();
}

void SftpDecoder::seek(uint64_t pos)
{
    if(m_decoder) {
        m_decoder->seek(pos);
    }
}

AudioBuffer SftpDecoder::readBuffer(size_t bytes)
{
    return m_decoder ? m_decoder->readBuffer(bytes) : AudioBuffer{};
}

SftpReader::SftpReader(SftpClientManager* manager)
    : m_manager{manager}
{ }

QStringList SftpReader::extensions() const
{
    return m_reader ? m_reader->extensions() : QStringList{};
}

QStringList SftpReader::supportedSchemes() const
{
    return sftpSchemes();
}

bool SftpReader::canReadCover() const
{
    return m_reader && m_reader->canReadCover();
}

bool SftpReader::canWriteMetaData() const
{
    return false;
}

bool SftpReader::init(const AudioSource& source)
{
    const QUrl url{source.filepath};
    if(!url.isValid() || url.scheme() != u"sftp"_s) {
        return false;
    }

    m_reader.reset();
    m_device.reset();

    SftpClient* const client = m_manager ? m_manager->clientFor(url) : nullptr;
    if(!client) {
        return false;
    }

    m_device = std::make_unique<SftpDevice>(client, url.path().isEmpty() ? u"/"_s : url.path());
    if(!m_device->open(QIODevice::ReadOnly)) {
        m_device.reset();
        return false;
    }

    m_reader   = std::make_unique<TagLibReader>();
    m_filepath = source.filepath;
    return true;
}

bool SftpReader::readTrack(const AudioSource& source, Track& track)
{
    if(!m_device || !m_reader) {
        return false;
    }

    return m_reader->readTrack(rebindSource(m_filepath.isEmpty() ? source.filepath : m_filepath, m_device.get()),
                               track);
}

QByteArray SftpReader::readCover(const AudioSource& source, const Track& track, Track::Cover cover)
{
    if(!m_device || !m_reader) {
        return {};
    }

    return m_reader->readCover(rebindSource(m_filepath.isEmpty() ? source.filepath : m_filepath, m_device.get()), track,
                               cover);
}

} // namespace Fooyin::Sftp
