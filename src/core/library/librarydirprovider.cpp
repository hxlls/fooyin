#include "librarydirprovider.h"

#include <QHash>
#include <QStringList>

namespace Fooyin {

namespace {
/*!
 * Concrete registry storing providers by scheme. Not exported; accessed
 * through the LibraryDirProviderRegistry interface held by LibraryManager.
 */
class DefaultLibraryDirProviderRegistry : public LibraryDirProviderRegistry
{
public:
    void addProvider(LibraryDirProvider* provider) override
    {
        if(!provider) {
            return;
        }
        const QStringList schemes = provider->schemes();
        for(const QString& scheme : schemes) {
            m_providers.insert(scheme, provider);
        }
    }

    [[nodiscard]] LibraryDirProvider* providerForScheme(const QString& scheme) const override
    {
        return m_providers.value(scheme);
    }

    [[nodiscard]] QStringList supportedSchemes() const override
    {
        QStringList schemes = m_providers.keys();
        schemes.sort();
        return schemes;
    }

private:
    QHash<QString, LibraryDirProvider*> m_providers;
};
} // namespace

/*!
 * Returns the process-wide registry. Kept as a function-local singleton so that
 * LibraryManager (and the scanner) can reach it without owning the registry
 * type; plugins receive it indirectly through LibraryManager.
 */
LibraryDirProviderRegistry* libraryDirProviderRegistry()
{
    static DefaultLibraryDirProviderRegistry registry;
    return &registry;
}

} // namespace Fooyin
