#include "webdavdirprovider.h"

#include "webdavurl.h"

#include <QUrl>

using namespace Qt::StringLiterals;

namespace Fooyin::Webdav {

WebdavDirProvider::WebdavDirProvider(WebdavClientManager* manager)
    : m_manager{manager}
{ }

QStringList WebdavDirProvider::schemes() const
{
    return {QString::fromLatin1(Scheme), QString::fromLatin1(SecureScheme)};
}

std::optional<QList<LibraryEntry>> WebdavDirProvider::listDirectory(const QUrl& url, QString* error) const
{
    if(!m_manager) {
        return {};
    }

    // The URL reaching this provider uses the webdav(s) scheme; translate it to
    // the http(s) URL used on the wire, like the decoder does.
    const auto httpUrl = toHttpUrl(url.toString());
    if(!httpUrl) {
        if(error) {
            *error = u"Not a WebDAV URL"_s;
        }
        return {};
    }

    WebdavClient* const client = m_manager->clientFor(httpUrl.value());
    if(!client) {
        if(error) {
            *error = u"Could not create a connection to "_s + httpUrl.value().toString();
        }
        return {};
    }

    const auto children = client->listDirectory(httpUrl.value());
    if(!children.has_value()) {
        if(error) {
            *error = u"Failed to list directory"_s;
        }
        return {};
    }

    // Map protocol entries to LibraryEntry, keeping the webdav(s) scheme so the
    // rest of fooyin (audio input routing, path normalisation) treats them as
    // virtual paths. Parent paths are rebuilt from the listing's href where the
    // server gives absolute URLs, otherwise re-joined onto the requested root.
    QList<LibraryEntry> entries;
    entries.reserve(static_cast<qsizetype>(children->size()));
    for(const auto& child : children.value()) {
        LibraryEntry entry;
        entry.isDir      = child.isDir;
        entry.size       = child.size;
        entry.modifiedMs = child.modified.isValid() ? static_cast<qint64>(child.modified.toMSecsSinceEpoch()) : 0;

        // Build the canonical webdav(s) URL for this child. Prefer the href from
        // the server when it is absolute and shares our scheme.
        const QUrl hrefUrl{child.path};
        if(hrefUrl.isValid()
           && (hrefUrl.scheme() == QLatin1String(Scheme) || hrefUrl.scheme() == QLatin1String(SecureScheme))
           && !hrefUrl.host().isEmpty()) {
            entry.path = child.path;
        }
        else {
            const QString base   = url.toString();
            const QString joined = joinPath(base, child.path);
            entry.path           = joined;
        }
        entries.push_back(entry);
    }
    return entries;
}

} // namespace Fooyin::Webdav
