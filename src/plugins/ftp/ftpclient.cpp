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

#include "ftpclient.h"

#include <curl/curl.h>

#include <QDateTime>
#include <QFile>
#include <QUrl>

#include <mutex>

using namespace Qt::StringLiterals;

namespace Fooyin::Ftp {

namespace {
//! MLSD fact lines look like: type=dir;size=123;modify=20240101120000; name
FtpEntry parseMlsdLine(const QString& line, const QString& basePath)
{
    FtpEntry entry;
    const QStringList parts = line.split(u';');

    QString name;
    for(const auto& part : parts) {
        const QString trimmed = part.trimmed();
        if(trimmed.compare(u"type=dir", Qt::CaseInsensitive) == 0) {
            entry.isDir = true;
        }
        else if(trimmed.startsWith(u"size=", Qt::CaseInsensitive)) {
            entry.size = trimmed.section(u'=', 1, 1).toLongLong();
        }
        else if(trimmed.startsWith(u"modify=", Qt::CaseInsensitive)) {
            // modify=YYYYMMDDHHMMSS[.sss]
            const QString t    = trimmed.section(u'=', 1, 1).left(14);
            const QDateTime dt = QDateTime::fromString(t, u"yyyyMMddHHmmss"_s);
            if(dt.isValid()) {
                entry.modifiedMs = dt.toMSecsSinceEpoch();
            }
        }
        else if(!trimmed.isEmpty()) {
            name = trimmed; // last fragment is the display name (may contain spaces)
        }
    }

    if(name.isEmpty()) {
        return entry;
    }
    entry.name = name;
    entry.path = basePath + name;
    if(entry.isDir && !entry.path.endsWith(u'/')) {
        entry.path += u'/';
    }
    return entry;
}

size_t listWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* buffer = static_cast<QByteArray*>(userdata);
    buffer->append(ptr, static_cast<qsizetype>(size * nmemb));
    return size * nmemb;
}

size_t fileWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* file           = static_cast<QFile*>(userdata);
    const qint64 written = file->write(ptr, static_cast<qint64>(size * nmemb));
    return written >= 0 ? static_cast<size_t>(written) : 0;
}

size_t discardWriteCallback(char*, size_t size, size_t nmemb, void*)
{
    return size * nmemb;
}
} // namespace

class FtpClient::Private
{
public:
    explicit Private(FtpClientConfig config)
        : config{std::move(config)}
    {
        // libcurl keeps these pointers for the transfer duration; stable storage.
        userUtf8     = this->config.user.toStdString();
        passwordUtf8 = this->config.password.toStdString();

        // No explicit curl_global_init(): libcurl lazily initialises itself on
        // the first curl_easy_init(). Calling curl_global_init() inside a Qt
        // process that already has OpenSSL state can crash on some builds.
        easy = curl_easy_init();
    }

    ~Private()
    {
        if(easy) {
            curl_easy_cleanup(easy);
        }
    }

    //! Serialises every operation: libcurl handles must not be used concurrently.
    [[nodiscard]] std::unique_lock<std::mutex> lock()
    {
        return std::unique_lock{m};
    }

    [[nodiscard]] QUrl transportUrl(const QString& remotePath) const
    {
        const QString scheme = config.security == FtpSecurity::Implicit ? u"ftps"_s : u"ftp"_s;
        QUrl url;
        url.setScheme(scheme);
        url.setHost(config.host);
        url.setPort(config.port);
        url.setPath(remotePath);
        return url;
    }

    void applyCommon(CURL* h) const
    {
        curl_easy_setopt(h, CURLOPT_USERNAME, userUtf8.c_str());
        curl_easy_setopt(h, CURLOPT_PASSWORD, passwordUtf8.c_str());
        curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(config.timeout.count()));
        curl_easy_setopt(h, CURLOPT_FTP_SKIP_PASV_IP, 1L);
        curl_easy_setopt(h, CURLOPT_ACCEPTTIMEOUT_MS, static_cast<long>(config.timeout.count()));
        curl_easy_setopt(h, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(h, CURLOPT_LOW_SPEED_TIME, 60L);
        curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);

        switch(config.security) {
            case FtpSecurity::Plain:
                curl_easy_setopt(h, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_NONE));
                break;
            case FtpSecurity::Explicit:
                curl_easy_setopt(h, CURLOPT_USE_SSL, static_cast<long>(CURLUSESSL_ALL));
                break;
            case FtpSecurity::Implicit:
                break; // ftps:// transport negotiates TLS implicitly
        }
        if(config.insecureSsl) {
            curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0L);
        }
    }

    std::optional<qint64> fileSize(const QString& remotePath, QString* error)
    {
        const auto guard          = lock();
        const QUrl url            = transportUrl(remotePath);
        const std::string urlUtf8 = url.toString().toStdString();

        curl_easy_setopt(easy, CURLOPT_URL, urlUtf8.c_str());
        curl_easy_setopt(easy, CURLOPT_NOBODY, 1L);   // SIZE
        curl_easy_setopt(easy, CURLOPT_FILETIME, 1L); // MDTM
        // Always route the (empty) body to a discard sink: libcurl's default
        // target when WRITEDATA is null is stdout, and fwrite(stdout) is not
        // safe inside a Qt application.
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &discardWriteCallback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, nullptr);
        applyCommon(easy);

        const CURLcode res = curl_easy_perform(easy);
        curl_easy_setopt(easy, CURLOPT_NOBODY, 0L);
        curl_easy_setopt(easy, CURLOPT_FILETIME, 0L);
        if(res != CURLE_OK) {
            *error = u"Could not stat %1: %2"_s.arg(remotePath, QString::fromLatin1(curl_easy_strerror(res)));
            return {};
        }

        curl_off_t size = 0;
        if(curl_easy_getinfo(easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size) != CURLE_OK || size < 0) {
            *error = u"Server did not report a size for %1"_s.arg(remotePath);
            return {};
        }
        return static_cast<qint64>(size);
    }

    std::optional<QList<FtpEntry>> listDirectory(const QString& remotePath, QString* error)
    {
        const auto guard          = lock();
        const QString dirPath     = remotePath.endsWith(u'/') ? remotePath : remotePath + u'/';
        const QUrl url            = transportUrl(dirPath);
        const std::string urlUtf8 = url.toString().toStdString();

        curl_easy_setopt(easy, CURLOPT_URL, urlUtf8.c_str());
        curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, "MLSD");
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &listWriteCallback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &listBuffer);
        applyCommon(easy);

        listBuffer.clear();
        const CURLcode res = curl_easy_perform(easy);
        curl_easy_setopt(easy, CURLOPT_CUSTOMREQUEST, nullptr);

        if(res != CURLE_OK) {
            *error = u"Could not list %1: %2"_s.arg(dirPath, QString::fromLatin1(curl_easy_strerror(res)));
            return {};
        }
        if(listBuffer.isEmpty()) {
            *error = u"Server returned no listing for %1 (MLSD unsupported?)"_s.arg(dirPath);
            return {};
        }

        QList<FtpEntry> entries;
        const QStringList lines = QString::fromUtf8(listBuffer).split(u'\n', Qt::SkipEmptyParts);
        for(const auto& line : lines) {
            FtpEntry entry = parseMlsdLine(line, dirPath);
            if(!entry.name.isEmpty()) {
                entries.push_back(std::move(entry));
            }
        }
        return entries;
    }

    bool download(const QString& remotePath, const QString& localFile, QString* error)
    {
        const auto guard = lock();

        QFile out{localFile};
        if(!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            *error = u"Could not open local spool file %1"_s.arg(localFile);
            return false;
        }

        const QUrl url            = transportUrl(remotePath);
        const std::string urlUtf8 = url.toString().toStdString();
        curl_easy_setopt(easy, CURLOPT_URL, urlUtf8.c_str());
        curl_easy_setopt(easy, CURLOPT_NOBODY, 0L);
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &fileWriteCallback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &out);
        applyCommon(easy);

        const CURLcode res = curl_easy_perform(easy);
        out.close();

        if(res != CURLE_OK) {
            *error = u"Download of %1 failed: %2"_s.arg(remotePath, QString::fromLatin1(curl_easy_strerror(res)));
            QFile::remove(localFile);
            return false;
        }
        return true;
    }

    FtpClientConfig config;
    std::string userUtf8;
    std::string passwordUtf8;
    CURL* easy{nullptr};
    QByteArray listBuffer;

private:
    std::mutex m;
};

FtpClient::FtpClient(const FtpClientConfig& config)
    : d{new Private{config}}
{ }

FtpClient::~FtpClient()
{
    delete d;
}

std::optional<qint64> FtpClient::fileSize(const QString& remotePath, QString* errorDetail)
{
    return d->fileSize(remotePath, errorDetail);
}

std::optional<QList<FtpEntry>> FtpClient::listDirectory(const QString& remotePath, QString* errorDetail)
{
    return d->listDirectory(remotePath, errorDetail);
}

bool FtpClient::download(const QString& remotePath, const QString& localFile, QString* errorDetail)
{
    return d->download(remotePath, localFile, errorDetail);
}

} // namespace Fooyin::Ftp
