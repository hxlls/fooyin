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

    // PROPFIND targets a collection; ensure the request URL carries the trailing
    // slash servers expect for directories (root "/" already ends with one).
    QUrl dirUrl           = httpUrl.value();
    const QString dirPath = dirUrl.path();
    if(!dirPath.endsWith(u'/')) {
        dirUrl.setPath(dirPath + u'/');
    }

    WebdavClient* const client = m_manager->clientFor(dirUrl);
    if(!client) {
        if(error) {
            *error = u"Could not create a connection to "_s + httpUrl.value().toString();
        }
        return {};
    }

    const auto children = client->listDirectory(dirUrl, error);
    if(!children.has_value()) {
        if(error && error->isEmpty()) {
            *error = u"Failed to list directory"_s;
        }
        return {};
    }

    // Map protocol entries to LibraryEntry, keeping the webdav(s) scheme so the
    // rest of fooyin (audio input routing, path normalisation) treats them as
    // virtual paths.
    //
    // WebDAV servers return hrefs that are absolute paths *relative to the
    // server root*, usually percent-encoded (e.g. "/music/%E9%99%88.flac").
    // Joining them onto the requested library root would double the path
    // ("/music/music/..."), so they are instead rebuilt onto
    // "scheme://authority". The percent-encoding is preserved so a later
    // toHttpUrl() round-trip requests the correct resource.
    QList<LibraryEntry> entries;
    entries.reserve(static_cast<qsizetype>(children->size()));
    for(const auto& child : children.value()) {
        LibraryEntry entry;
        entry.isDir      = child.isDir;
        entry.size       = child.size;
        entry.modifiedMs = child.modified.isValid() ? static_cast<qint64>(child.modified.toMSecsSinceEpoch()) : 0;

        // The child is already an absolute webdav(s) URL (some servers echo the
        // full URL back in the href): use it as-is.
        const QUrl fullHref{child.path};
        if(fullHref.isValid() && !fullHref.host().isEmpty()
           && (fullHref.scheme() == QLatin1String(Scheme) || fullHref.scheme() == QLatin1String(SecureScheme))) {
            entry.path = child.path;
            entries.push_back(entry);
            continue;
        }

        // Relative/root-absolute href: rebuild onto scheme://authority.
        // url is the webdav(s) URL currently being listed.
        QString base = url.toString();
        // Trim any path of the listed URL so a root-absolute href is not doubled.
        const int authorityEnd = base.indexOf(u'/', base.indexOf(u"://"_s) + 3);
        if(authorityEnd >= 0) {
            base = base.left(authorityEnd);
        }
        QString childPath = child.path;
        if(!childPath.startsWith(u'/')) {
            childPath = u'/' + childPath;
        }
        entry.path = base + childPath;
        entries.push_back(entry);
    }
    return entries;
}

} // namespace Fooyin::Webdav
