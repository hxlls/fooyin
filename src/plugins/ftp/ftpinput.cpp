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

#include "ftpinput.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Ftp {

namespace {
QStringList ftpSchemes()
{
    return {QStringLiteral("ftp"), QStringLiteral("ftps")};
}

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

FtpDecoder::FtpDecoder(FtpClientManager* manager)
    : m_manager{manager}
{ }

QStringList FtpDecoder::extensions() const
{
    return m_decoder ? m_decoder->extensions() : QStringList{};
}

QStringList FtpDecoder::supportedSchemes() const
{
    return ftpSchemes();
}

bool FtpDecoder::isSeekable() const
{
    return m_decoder && m_device && !m_device->isSequential() && m_decoder->isSeekable();
}

int FtpDecoder::bitrate() const
{
    return m_decoder ? m_decoder->bitrate() : 0;
}

bool FtpDecoder::trackHasChanged() const
{
    return m_decoder && m_decoder->trackHasChanged();
}

Track FtpDecoder::changedTrack() const
{
    return m_decoder ? m_decoder->changedTrack() : Track{};
}

std::optional<AudioFormat> FtpDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    const QUrl url{source.filepath};
    if(!url.isValid() || (url.scheme() != u"ftp"_s && url.scheme() != u"ftps"_s)) {
        return {};
    }

    m_decoder.reset();
    m_device.reset();

    FtpClient* const client = m_manager ? m_manager->clientFor(url) : nullptr;
    if(!client) {
        return {};
    }

    // open() spools the full file to a temporary location.
    m_device = std::make_unique<FtpDevice>(client, url.path().isEmpty() ? u"/"_s : url.path());
    if(!m_device->open(QIODevice::ReadOnly)) {
        m_device.reset();
        return {};
    }

    m_decoder = std::make_unique<FFmpegDecoder>();

    return m_decoder->init(rebindSource(source.filepath, m_device.get()), track, options);
}

void FtpDecoder::start()
{
    if(m_decoder) {
        m_decoder->start();
    }
}

void FtpDecoder::stop()
{
    if(m_decoder) {
        m_decoder->stop();
    }
    m_decoder.reset();
    m_device.reset();
}

void FtpDecoder::seek(uint64_t pos)
{
    if(m_decoder) {
        m_decoder->seek(pos);
    }
}

AudioBuffer FtpDecoder::readBuffer(size_t bytes)
{
    return m_decoder ? m_decoder->readBuffer(bytes) : AudioBuffer{};
}

FtpReader::FtpReader(FtpClientManager* manager)
    : m_manager{manager}
{ }

QStringList FtpReader::extensions() const
{
    return m_reader ? m_reader->extensions() : QStringList{};
}

QStringList FtpReader::supportedSchemes() const
{
    return ftpSchemes();
}

bool FtpReader::canReadCover() const
{
    return m_reader && m_reader->canReadCover();
}

bool FtpReader::canWriteMetaData() const
{
    return false;
}

bool FtpReader::init(const AudioSource& source)
{
    const QUrl url{source.filepath};
    if(!url.isValid() || (url.scheme() != u"ftp"_s && url.scheme() != u"ftps"_s)) {
        return false;
    }

    m_reader.reset();
    m_device.reset();

    FtpClient* const client = m_manager ? m_manager->clientFor(url) : nullptr;
    if(!client) {
        return false;
    }

    m_device = std::make_unique<FtpDevice>(client, url.path().isEmpty() ? u"/"_s : url.path());
    if(!m_device->open(QIODevice::ReadOnly)) {
        m_device.reset();
        return false;
    }

    m_reader   = std::make_unique<TagLibReader>();
    m_filepath = source.filepath;
    return true;
}

bool FtpReader::readTrack(const AudioSource& source, Track& track)
{
    if(!m_device || !m_reader) {
        return false;
    }

    return m_reader->readTrack(rebindSource(m_filepath.isEmpty() ? source.filepath : m_filepath, m_device.get()),
                               track);
}

QByteArray FtpReader::readCover(const AudioSource& source, const Track& track, Track::Cover cover)
{
    if(!m_device || !m_reader) {
        return {};
    }

    return m_reader->readCover(rebindSource(m_filepath.isEmpty() ? source.filepath : m_filepath, m_device.get()), track,
                               cover);
}

} // namespace Fooyin::Ftp
