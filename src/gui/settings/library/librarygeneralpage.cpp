/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <luket@pm.me>
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

#include "librarygeneralpage.h"

#include "librarymodel.h"

#include "core/internalcoresettings.h"
#include <gui/iconloader.h>

#include "core/library/libraryinfo.h"
#include <core/coresettings.h>
#include <core/library/musiclibrary.h>
#include <gui/guiconstants.h>
#include <gui/widgets/extendabletableview.h>
#include <utils/settings/settingsmanager.h>
#include <utils/utils.h>

#include <QAction>
#include <QCheckBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariantMap>

using namespace Qt::StringLiterals;

namespace Fooyin {
class LibraryTableView : public ExtendableTableView
{
    Q_OBJECT

public:
    using ExtendableTableView::ExtendableTableView;

protected:
    void setupContextActions(QMenu* menu, const QPoint& pos) override;

Q_SIGNALS:
    void refreshLibrary(const Fooyin::LibraryInfo& info);
    void rescanLibrary(const Fooyin::LibraryInfo& info);
    void cancelLibraryScan(int id);
};

void LibraryTableView::setupContextActions(QMenu* menu, const QPoint& pos)
{
    const QModelIndex index = indexAt(pos);
    if(!index.isValid()) {
        return;
    }

    const auto library    = index.data(LibraryModel::Info).value<LibraryInfo>();
    const int scanId      = index.data(LibraryModel::ScanRequestId).toInt();
    const bool isPending  = library.status == LibraryInfo::Status::Pending;
    const bool isScanning = library.status == LibraryInfo::Status::Scanning && scanId >= 0;

    auto* refresh = new QAction(tr("&Scan for changes"), menu);
    refresh->setEnabled(!isPending && !isScanning);
    QObject::connect(refresh, &QAction::triggered, this, [this, library]() { Q_EMIT refreshLibrary(library); });

    auto* rescan = new QAction(tr("&Reload tracks"), menu);
    rescan->setEnabled(!isPending && !isScanning);
    QObject::connect(rescan, &QAction::triggered, this, [this, library]() { Q_EMIT rescanLibrary(library); });

    menu->addAction(refresh);
    menu->addAction(rescan);

    if(isScanning) {
        auto* cancel = new QAction(tr("&Cancel scan"), menu);
        Gui::setThemeIcon(cancel, Constants::Icons::Close);
        QObject::connect(cancel, &QAction::triggered, this, [this, scanId]() { Q_EMIT cancelLibraryScan(scanId); });
        menu->addSeparator();
        menu->addAction(cancel);
    }
}

class LibraryGeneralPageWidget : public SettingsPageWidget
{
    Q_OBJECT

public:
    explicit LibraryGeneralPageWidget(LibraryManager* libraryManager, MusicLibrary* library, SettingsManager* settings);

    void load() override;
    void apply() override;
    void reset() override;

private:
    void addLibrary() const;

    LibraryManager* m_libraryManager;
    MusicLibrary* m_library;
    SettingsManager* m_settings;

    LibraryTableView* m_libraryView;
    LibraryModel* m_model;

    QLineEdit* m_restrictTypes;
    QLineEdit* m_excludeTypes;

    QCheckBox* m_autoRefresh;
    QCheckBox* m_monitorLibraryDirectories;
    QCheckBox* m_monitorTrackFiles;
    QCheckBox* m_markUnavailable;
    QCheckBox* m_markUnavailableStart;
};

LibraryGeneralPageWidget::LibraryGeneralPageWidget(LibraryManager* libraryManager, MusicLibrary* library,
                                                   SettingsManager* settings)
    : m_libraryManager{libraryManager}
    , m_library{library}
    , m_settings{settings}
    , m_libraryView{new LibraryTableView(this)}
    , m_model{new LibraryModel(m_libraryManager, this)}
    , m_restrictTypes{new QLineEdit(this)}
    , m_excludeTypes{new QLineEdit(this)}
    , m_autoRefresh{new QCheckBox(tr("Auto refresh on startup"), this)}
    , m_monitorLibraryDirectories{new QCheckBox(tr("Monitor library directories"), this)}
    , m_monitorTrackFiles{new QCheckBox(tr("Monitor track files"), this)}
    , m_markUnavailable{new QCheckBox(tr("Mark unavailable tracks on playback"), this)}
    , m_markUnavailableStart{new QCheckBox(tr("Mark unavailable tracks on startup"), this)}
{
    m_libraryView->setExtendableModel(m_model);

    // Hide Id column
    m_libraryView->hideColumn(0);

    m_libraryView->setExtendableColumn(1);
    m_libraryView->verticalHeader()->hide();
    m_libraryView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    m_autoRefresh->setToolTip(tr("Scan libraries for changes on startup"));
    m_monitorLibraryDirectories->setToolTip(
        tr("Watch library directories for external changes such as new or removed files"));
    m_monitorTrackFiles->setToolTip(tr("Watch individual track files for tag changes"));

    auto* fileTypesGroup  = new QGroupBox(tr("File Types"), this);
    auto* fileTypesLayout = new QGridLayout(fileTypesGroup);

    int row{0};
    fileTypesLayout->addWidget(new QLabel(tr("Restrict to") + ":"_L1, this), row, 0);
    fileTypesLayout->addWidget(m_restrictTypes, row++, 1);
    fileTypesLayout->addWidget(new QLabel(tr("Exclude") + ":"_L1, this), row, 0);
    fileTypesLayout->addWidget(m_excludeTypes, row++, 1);
    //: Example of semicolon-separated file extensions (e.g. mp3;m4a)
    fileTypesLayout->addWidget(new QLabel(u"🛈 "_s + tr("e.g. \"%1\"").arg("mp3;m4a"_L1), this), row++, 1);
    fileTypesLayout->setColumnStretch(1, 1);

    auto* scanningGroup  = new QGroupBox(tr("Scanning"), this);
    auto* scanningLayout = new QGridLayout(scanningGroup);

    row = 0;
    scanningLayout->addWidget(m_autoRefresh, row++, 0);
    scanningLayout->addWidget(m_monitorLibraryDirectories, row++, 0);
    scanningLayout->addWidget(m_monitorTrackFiles, row++, 0);

    auto* availabilityGroup  = new QGroupBox(tr("Availability"), this);
    auto* availabilityLayout = new QGridLayout(availabilityGroup);

    row = 0;
    availabilityLayout->addWidget(m_markUnavailable, row++, 0);
    availabilityLayout->addWidget(m_markUnavailableStart, row++, 0);

    auto* mainLayout = new QGridLayout(this);

    row = 0;
    mainLayout->addWidget(m_libraryView, row++, 0, 1, 2);
    mainLayout->addWidget(fileTypesGroup, row++, 0, 1, 2);
    mainLayout->addWidget(scanningGroup, row, 0);
    mainLayout->addWidget(availabilityGroup, row++, 1);
    mainLayout->setRowStretch(row, 1);
    mainLayout->setColumnStretch(0, 1);
    mainLayout->setColumnStretch(1, 1);

    QObject::connect(m_model, &LibraryModel::requestAddLibrary, this, &LibraryGeneralPageWidget::addLibrary);
    QObject::connect(m_libraryView, &LibraryTableView::refreshLibrary, m_library, &MusicLibrary::refresh);
    QObject::connect(m_libraryView, &LibraryTableView::rescanLibrary, m_library, &MusicLibrary::rescan);
    QObject::connect(m_libraryView, &LibraryTableView::cancelLibraryScan, m_library, &MusicLibrary::cancelScan);
    QObject::connect(m_library, &MusicLibrary::scanProgress, m_model, &LibraryModel::setScanProgress);
    QObject::connect(m_monitorLibraryDirectories, &QCheckBox::toggled, m_monitorTrackFiles, &QWidget::setEnabled);
}

void LibraryGeneralPageWidget::load()
{
    m_model->populate();

    const QStringList restrictExtensions
        = m_settings->fileValue(Settings::Core::Internal::LibraryRestrictTypes).toStringList();
    const QStringList excludeExtensions
        = m_settings->fileValue(Settings::Core::Internal::LibraryExcludeTypes, QStringList{u"cue"_s}).toStringList();

    m_restrictTypes->setText(restrictExtensions.join(u';'));
    m_excludeTypes->setText(excludeExtensions.join(u';'));

    m_autoRefresh->setChecked(m_settings->value<Settings::Core::AutoRefresh>());
    m_monitorLibraryDirectories->setChecked(m_settings->value<Settings::Core::Internal::MonitorLibraryDirectories>());
    m_monitorTrackFiles->setChecked(m_settings->value<Settings::Core::Internal::MonitorTrackFiles>());
    m_monitorTrackFiles->setEnabled(m_monitorLibraryDirectories->isChecked());
    m_markUnavailable->setChecked(m_settings->fileValue(Settings::Core::Internal::MarkUnavailable, false).toBool());
    m_markUnavailableStart->setChecked(
        m_settings->fileValue(Settings::Core::Internal::MarkUnavailableStartup, false).toBool());
}

void LibraryGeneralPageWidget::apply()
{
    m_model->processQueue();

    m_settings->fileSet(Settings::Core::Internal::LibraryRestrictTypes,
                        m_restrictTypes->text().split(u';', Qt::SkipEmptyParts));
    m_settings->fileSet(Settings::Core::Internal::LibraryExcludeTypes,
                        m_excludeTypes->text().split(u';', Qt::SkipEmptyParts));

    m_settings->set<Settings::Core::AutoRefresh>(m_autoRefresh->isChecked());
    m_settings->set<Settings::Core::Internal::MonitorLibraryDirectories>(m_monitorLibraryDirectories->isChecked());
    m_settings->set<Settings::Core::Internal::MonitorTrackFiles>(m_monitorTrackFiles->isChecked());
    m_settings->fileSet(Settings::Core::Internal::MarkUnavailable, m_markUnavailable->isChecked());
    m_settings->fileSet(Settings::Core::Internal::MarkUnavailableStartup, m_markUnavailableStart->isChecked());
}

void LibraryGeneralPageWidget::reset()
{
    m_settings->fileRemove(Settings::Core::Internal::LibraryRestrictTypes);
    m_settings->fileRemove(Settings::Core::Internal::LibraryExcludeTypes);

    m_settings->reset<Settings::Core::AutoRefresh>();
    m_settings->reset<Settings::Core::Internal::MonitorLibraryDirectories>();
    m_settings->reset<Settings::Core::Internal::MonitorTrackFiles>();
    m_settings->fileRemove(Settings::Core::Internal::MarkUnavailable);
    m_settings->fileRemove(Settings::Core::Internal::MarkUnavailableStartup);
}

void LibraryGeneralPageWidget::addLibrary() const
{
    // ===== Library source-type menu for the "add" action =====
    // The original entry only let the user pick a local folder. This extends it to a
    // set of source types. Currently two are offered: a local directory (default, so
    // existing behaviour is unchanged) and a WebDAV server. Future remote types such as
    // SMB or FTP can be appended here, reusing the same "build a LibraryInfo and add it"
    // flow, which keeps the GUI unaware of any protocol specifics.
    QMenu addMenu;

    // Local directory: keep the system folder picker so existing users see no change.
    QAction* const addLocal = addMenu.addAction(tr("Local &directory\u2026"));
    // WebDAV server: open a connection dialog collecting an URL and credentials, then
    // store the library with a webdav(s):// path.
    QAction* const addWebdav = addMenu.addAction(tr("&WebDAV server\u2026"));
    // SFTP server: same flow as WebDAV but with an sftp:// path and per-server
    // credentials stored under "sftp/<host:port>".
    QAction* const addSftp = addMenu.addAction(tr("&SFTP server\u2026"));
    // FTP/FTPS server: ftp(s):// path, credentials under "ftp/<host:port>".
    QAction* const addFtp = addMenu.addAction(tr("&FTP/FTPS server\u2026"));
    // SMB and NFS shares: OS-provided paths (UNC / mount points) are added as
    // ordinary local directories; fooyin needs no in-app protocol support.
    QAction* const addSmb = addMenu.addAction(tr("&SMB share\u2026"));
    QAction* const addNfs = addMenu.addAction(tr("N&FS share\u2026"));

    // Pop the source-type menu under the cursor (the just-clicked "+"), not centred on
    // the table. addLibrary() runs right after the ExtendableTableView "+" was pressed,
    // so the cursor is on the button at this point.
    QAction* const chosen = addMenu.exec(QCursor::pos());

    // ===== Local directory source =====
    if(chosen == addLocal) {
        const QString dir = QFileDialog::getExistingDirectory(m_libraryView, tr("Select folder"), QDir::homePath(),
                                                              QFileDialog::DontResolveSymlinks);
        if(dir.isEmpty()) {
            m_model->markForAddition({});
            return;
        }
        const QFileInfo info{QDir::cleanPath(dir)};
        m_model->markForAddition({.name = info.fileName(), .path = dir});
        return;
    }

    // ===== WebDAV source: configuration dialog =====
    // The library path is stored as webdavs://host:port/root. Credentials are kept out
    // of the path and the track database; they are saved separately in the settings,
    // keyed by host:port, and applied when the server is scanned or played.
    if(chosen == addWebdav) {
        QDialog dialog(m_libraryView);
        dialog.setWindowTitle(tr("Add WebDAV library"));
        auto* form     = new QFormLayout;
        auto* urlEdit  = new QLineEdit(u"webdavs://"_s, &dialog);
        auto* userEdit = new QLineEdit(&dialog);
        auto* passEdit = new QLineEdit(&dialog);
        auto* nameEdit = new QLineEdit(&dialog);
        passEdit->setEchoMode(QLineEdit::Password);
        form->addRow(tr("Server URL:"), urlEdit);
        form->addRow(tr("User name:"), userEdit);
        form->addRow(tr("Password:"), passEdit);
        form->addRow(tr("Library name:"), nameEdit);
        // Some servers (e.g. a NAS reached by raw LAN IP) present a certificate
        // issued for a hostname, so the TLS peer cannot be verified. When ticked,
        // peer verification is disabled for this server only.
        auto* insecureSsl = new QCheckBox(tr("Ignore certificate errors"), &dialog);
        auto* buttons     = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        auto* layout = new QVBoxLayout(&dialog);
        layout->addLayout(form);
        layout->addWidget(insecureSsl);
        layout->addWidget(buttons);

        if(dialog.exec() == QDialog::Accepted) {
            QString url  = urlEdit->text().trimmed();
            QString name = nameEdit->text().trimmed();
            if(name.isEmpty()) {
                // Default the library name to the last URL path segment, or the host if
                // the path is empty.
                const QUrl parsed{url};
                const QString path = parsed.path();
                name               = path.isEmpty() ? parsed.host() : QFileInfo{path}.fileName();
            }
            if(url.isEmpty() || name.isEmpty()) {
                m_model->markForAddition({});
                return;
            }
            // Persist credentials for the server separately from the library path,
            // keyed by "webdav/<host:port>". The WebDAV plugin reads this entry when
            // it first talks to the server (scanning or playback). Keeping passwords
            // out of LibraryInfo/Track paths avoids leaking them into the database.
            const QUrl urlParsed{url};
            const QString authority = urlParsed.port() > 0
                                        ? QStringLiteral("%1:%2").arg(urlParsed.host()).arg(urlParsed.port())
                                        : urlParsed.host();
            QVariantMap credentials;
            credentials.insert(QStringLiteral("user"), userEdit->text());
            credentials.insert(QStringLiteral("password"), passEdit->text());
            credentials.insert(QStringLiteral("insecureSsl"), insecureSsl->isChecked());
            m_settings->fileSet(QStringLiteral("webdav/") + authority, credentials);

            // Keep the path as a webdavs:// URL; scanning routes it to the WebDAV source.
            m_model->markForAddition({.name = name, .path = url});
        }
        return;
    }

    // ===== SFTP source: configuration dialog =====
    // Identical flow to WebDAV; the library path is an sftp://host[:port]/abs/path URL
    // and credentials are stored separately under "sftp/<host:port>" for the SFTP plugin.
    if(chosen == addSftp) {
        QDialog dialog(m_libraryView);
        dialog.setWindowTitle(tr("Add SFTP library"));
        auto* form     = new QFormLayout;
        auto* urlEdit  = new QLineEdit(u"sftp://"_s, &dialog);
        auto* userEdit = new QLineEdit(&dialog);
        auto* passEdit = new QLineEdit(&dialog);
        auto* nameEdit = new QLineEdit(&dialog);
        passEdit->setEchoMode(QLineEdit::Password);
        form->addRow(tr("Server URL:"), urlEdit);
        form->addRow(tr("User name:"), userEdit);
        form->addRow(tr("Password:"), passEdit);
        form->addRow(tr("Library name:"), nameEdit);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        auto* layout = new QVBoxLayout(&dialog);
        layout->addLayout(form);
        layout->addWidget(buttons);

        if(dialog.exec() == QDialog::Accepted) {
            QString url  = urlEdit->text().trimmed();
            QString name = nameEdit->text().trimmed();
            if(name.isEmpty()) {
                const QUrl parsed{url};
                const QString path = parsed.path();
                name               = path.isEmpty() ? parsed.host() : QFileInfo{path}.fileName();
            }
            if(url.isEmpty() || name.isEmpty()) {
                m_model->markForAddition({});
                return;
            }
            const QUrl urlParsed{url};
            const QString authority = urlParsed.port() > 0
                                        ? QStringLiteral("%1:%2").arg(urlParsed.host()).arg(urlParsed.port())
                                        : urlParsed.host();
            QVariantMap credentials;
            credentials.insert(QStringLiteral("user"), userEdit->text());
            credentials.insert(QStringLiteral("password"), passEdit->text());
            m_settings->fileSet(QStringLiteral("sftp/") + authority, credentials);

            m_model->markForAddition({.name = name, .path = url});
        }
        return;
    }

    // ===== FTP/FTPS source: configuration dialog =====
    // ftp(s)://host[:port]/abs/path; explicit TLS ("FTPS", port 21) and
    // implicit TLS (port 990) are both handled by the FTP plugin, so ftps:// is
    // accepted regardless of the server's TLS mode.
    if(chosen == addFtp) {
        QDialog dialog(m_libraryView);
        dialog.setWindowTitle(tr("Add FTP/FTPS library"));
        auto* form     = new QFormLayout;
        auto* urlEdit  = new QLineEdit(u"ftps://"_s, &dialog);
        auto* userEdit = new QLineEdit(&dialog);
        auto* passEdit = new QLineEdit(&dialog);
        auto* nameEdit = new QLineEdit(&dialog);
        passEdit->setEchoMode(QLineEdit::Password);
        form->addRow(tr("Server URL:"), urlEdit);
        form->addRow(tr("User name:"), userEdit);
        form->addRow(tr("Password:"), passEdit);
        form->addRow(tr("Library name:"), nameEdit);
        auto* insecureSsl = new QCheckBox(tr("Ignore certificate errors"), &dialog);
        auto* buttons     = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        auto* layout = new QVBoxLayout(&dialog);
        layout->addLayout(form);
        layout->addWidget(insecureSsl);
        layout->addWidget(buttons);

        if(dialog.exec() == QDialog::Accepted) {
            QString url  = urlEdit->text().trimmed();
            QString name = nameEdit->text().trimmed();
            if(name.isEmpty()) {
                const QUrl parsed{url};
                const QString path = parsed.path();
                name               = path.isEmpty() ? parsed.host() : QFileInfo{path}.fileName();
            }
            if(url.isEmpty() || name.isEmpty()) {
                m_model->markForAddition({});
                return;
            }
            const QUrl urlParsed{url};
            const QString authority = urlParsed.port() > 0
                                        ? QStringLiteral("%1:%2").arg(urlParsed.host()).arg(urlParsed.port())
                                        : urlParsed.host();
            QVariantMap credentials;
            credentials.insert(QStringLiteral("user"), userEdit->text());
            credentials.insert(QStringLiteral("password"), passEdit->text());
            credentials.insert(QStringLiteral("insecureSsl"), insecureSsl->isChecked());
            m_settings->fileSet(QStringLiteral("ftp/") + authority, credentials);

            m_model->markForAddition({.name = name, .path = url});
        }
        return;
    }

    // ===== SMB / NFS network-share sources =====
    // These are added as OS paths (UNC or mount points); fooyin's local file
    // pipeline handles them. Only a small guidance dialog is shown.
    if(chosen == addSmb || chosen == addNfs) {
        const bool isSmb = chosen == addSmb;

        QDialog dialog(m_libraryView);
        dialog.setWindowTitle(tr("Add %1 share").arg(isSmb ? u"SMB"_s : u"NFS"_s));
        auto* layout = new QVBoxLayout(&dialog);

        QString hint;
        if(isSmb) {
#ifdef Q_OS_WIN
            hint = tr("Enter a UNC path such as \\server\share\music (or map a drive first and pick it below).");
#elif defined(Q_OS_MACOS)
            hint = tr("Connect to the share in Finder first (Go \u2192 Connect to Server), then pick the mounted "
                      "volume below.");
#else
            hint = tr("Mount the share first (e.g. mount -t cifs //server/share /mnt/music), then pick the mount "
                      "directory below.");
#endif
        }
        else {
#ifdef Q_OS_WIN
            hint = tr("Enable the Windows NFS client, mount the share to a drive (mount -o anon \\server\export Z:), "
                      "then pick that drive below.");
#elif defined(Q_OS_MACOS)
            hint = tr("Mount the NFS export first (mount_nfs server:/export /Volumes/x), then pick the mount directory "
                      "below.");
#else
            hint = tr("Mount the NFS export first (e.g. mount -t nfs server:/export /mnt/music), then pick the mount "
                      "directory below.");
#endif
        }
        auto* hintLabel = new QLabel(hint, &dialog);
        hintLabel->setWordWrap(true);
        layout->addWidget(hintLabel);

        auto* pathEdit = new QLineEdit(&dialog);
        auto* browse   = new QPushButton(tr("Browse\u2026"), &dialog);
        layout->addWidget(pathEdit);
        layout->addWidget(browse);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        QObject::connect(browse, &QPushButton::clicked, &dialog, [&dialog, pathEdit]() {
            const QString dir = QFileDialog::getExistingDirectory(&dialog, tr("Select folder"));
            if(!dir.isEmpty()) {
                pathEdit->setText(dir);
            }
        });

        if(dialog.exec() == QDialog::Accepted) {
            const QString path = pathEdit->text().trimmed();
            if(path.isEmpty()) {
                m_model->markForAddition({});
                return;
            }
            const QFileInfo info{QDir::cleanPath(path)};
            m_model->markForAddition({.name = info.fileName(), .path = path});
        }
        return;
    }
}

LibraryGeneralPage::LibraryGeneralPage(LibraryManager* libraryManager, MusicLibrary* library, SettingsManager* settings,
                                       QObject* parent)
    : SettingsPage{settings->settingsDialog(), parent}
{
    setId(Constants::Page::LibraryGeneral);
    setName(tr("General"));
    setCategory({tr("Library")});
    setWidgetCreator([libraryManager, library, settings] {
        return new LibraryGeneralPageWidget(libraryManager, library, settings);
    });
}
} // namespace Fooyin

#include "librarygeneralpage.moc"
#include "moc_librarygeneralpage.cpp"
