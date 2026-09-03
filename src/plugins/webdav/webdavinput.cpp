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

#include "webdavinput.h"

#include "webdavurl.h"

using namespace Qt::StringLiterals;

namespace Fooyin::Webdav {

namespace {
QStringList webdavSchemes()
{
    return {QString::fromLatin1(Scheme), QString::fromLatin1(SecureScheme)};
}

/*!
 * Builds the AudioSource handed to the wrapped backend: the webdav(s) URI is
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

WebdavDecoder::WebdavDecoder(WebdavClient* client)
    : m_client{client}
{ }

QStringList WebdavDecoder::extensions() const
{
    return m_decoder ? m_decoder->extensions() : QStringList{};
}

QStringList WebdavDecoder::supportedSchemes() const
{
    return webdavSchemes();
}

bool WebdavDecoder::isSeekable() const
{
    return m_decoder && m_device && !m_device->isSequential() && m_decoder->isSeekable();
}

int WebdavDecoder::bitrate() const
{
    return m_decoder ? m_decoder->bitrate() : 0;
}

bool WebdavDecoder::trackHasChanged() const
{
    return m_decoder && m_decoder->trackHasChanged();
}

Track WebdavDecoder::changedTrack() const
{
    return m_decoder ? m_decoder->changedTrack() : Track{};
}

std::optional<AudioFormat> WebdavDecoder::init(const AudioSource& source, const Track& track, DecoderOptions options)
{
    const auto url = toHttpUrl(source.filepath);
    if(!url) {
        return {};
    }

    // Drop any state left over from a previous playback session.
    m_decoder.reset();
    m_device.reset();

    m_device = std::make_unique<WebdavDevice>(m_client, url.value());
    if(!m_device->open(QIODevice::ReadOnly)) {
        m_device.reset();
        return {};
    }

    // Construct lazily: FFmpegDecoder is move-only and cheap to build.
    m_decoder = std::make_unique<FFmpegDecoder>();

    return m_decoder->init(rebindSource(source.filepath, m_device.get()), track, options);
}

void WebdavDecoder::start()
{
    if(m_decoder) {
        m_decoder->start();
    }
}

void WebdavDecoder::stop()
{
    if(m_decoder) {
        m_decoder->stop();
    }
    m_decoder.reset();
    m_device.reset();
}

void WebdavDecoder::seek(uint64_t pos)
{
    if(m_decoder) {
        m_decoder->seek(pos);
    }
}

AudioBuffer WebdavDecoder::readBuffer(size_t bytes)
{
    return m_decoder ? m_decoder->readBuffer(bytes) : AudioBuffer{};
}

WebdavReader::WebdavReader(WebdavClient* client)
    : m_client{client}
{ }

QStringList WebdavReader::extensions() const
{
    return m_reader ? m_reader->extensions() : QStringList{};
}

QStringList WebdavReader::supportedSchemes() const
{
    return webdavSchemes();
}

bool WebdavReader::canReadCover() const
{
    return m_reader && m_reader->canReadCover();
}

bool WebdavReader::canWriteMetaData() const
{
    return false;
}

bool WebdavReader::init(const AudioSource& source)
{
    const auto url = toHttpUrl(source.filepath);
    if(!url) {
        return false;
    }

    m_reader.reset();
    m_device.reset();

    m_device = std::make_unique<WebdavDevice>(m_client, url.value());
    if(!m_device->open(QIODevice::ReadOnly)) {
        m_device.reset();
        return false;
    }

    m_reader   = std::make_unique<TagLibReader>();
    m_filepath = source.filepath;
    return true;
}

bool WebdavReader::readTrack(const AudioSource& source, Track& track)
{
    if(!m_device || !m_reader) {
        return false;
    }

    // TagLibReader wraps source.device in an IODeviceStream that seeks to 0 on
    // construction, so no explicit rewind is needed here.
    return m_reader->readTrack(rebindSource(m_filepath.isEmpty() ? source.filepath : m_filepath, m_device.get()),
                               track);
}

QByteArray WebdavReader::readCover(const AudioSource& source, const Track& track, Track::Cover cover)
{
    if(!m_device || !m_reader) {
        return {};
    }

    return m_reader->readCover(rebindSource(m_filepath.isEmpty() ? source.filepath : m_filepath, m_device.get()), track,
                               cover);
}

} // namespace Fooyin::Webdav
