#include "GalleryWindow.h"
#include "BingClient.h"
#include "WallpaperSetter.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRunnable>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QThreadPool>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int kThumbWidth = 240;
constexpr int kThumbHeight = 135;

QString formatSize(qint64 bytes)
{
    const double mb = bytes / (1024.0 * 1024.0);
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}
}

// Decodes and scales one thumbnail off the GUI thread (QThreadPool, no Qt
// Concurrent module needed). `ready` is delivered as a normal queued signal;
// since GalleryWindow is the connection's *receiver*, Qt automatically drops
// the connection if the window is destroyed (WA_DeleteOnClose) before a
// pending job finishes, so this never touches a dangling window pointer.
class ThumbnailJob : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit ThumbnailJob(const QString &path) : m_path(path) { setAutoDelete(true); }

    void run() override
    {
        QImageReader reader(m_path);
        reader.setAutoTransform(true);
        const QSize original = reader.size();
        if (original.isValid()) {
            const QSize scaled = original.scaled(kThumbWidth, kThumbHeight, Qt::KeepAspectRatioByExpanding);
            reader.setScaledSize(scaled);
        }
        const QImage image = reader.read();
        emit ready(m_path, image);
    }

signals:
    void ready(const QString &path, const QImage &image);

private:
    QString m_path;
};

GalleryWindow::GalleryWindow(QWidget *parent)
    : QWidget(parent)
    , m_watcher(new QFileSystemWatcher(this))
    , m_refreshDebounce(new QTimer(this))
{
    setWindowTitle(QStringLiteral("Bing Wallpaper — Gallery"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(900, 600);

    buildUi();

    QSettings settings;
    if (settings.contains("gallery/geometry"))
        restoreGeometry(settings.value("gallery/geometry").toByteArray());
    if (settings.contains("gallery/splitterState"))
        m_splitter->restoreState(settings.value("gallery/splitterState").toByteArray());

    // A single download touches the directory several times (QSaveFile's
    // temp file + rename, then the .json sidecar), each firing its own
    // directoryChanged. Debounce so one download means one rescan/re-queue
    // of every thumbnail, not several.
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(300);
    connect(m_refreshDebounce, &QTimer::timeout, this, &GalleryWindow::refreshList);

    const QString dir = BingClient::saveDir();
    m_watcher->addPath(dir);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this]() { m_refreshDebounce->start(); });

    refreshList();
}

GalleryWindow::~GalleryWindow()
{
    QSettings settings;
    settings.setValue("gallery/geometry", saveGeometry());
    settings.setValue("gallery/splitterState", m_splitter->saveState());
}

void GalleryWindow::buildUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *toolbar = new QToolBar(this);
    QAction *setAction = toolbar->addAction(QStringLiteral("Set as Wallpaper"));
    QAction *deleteAction = toolbar->addAction(QStringLiteral("Delete"));
    QAction *copyAction = toolbar->addAction(QStringLiteral("Copy Description"));
    QAction *folderAction = toolbar->addAction(QStringLiteral("Show in Folder"));
    QAction *refreshAction = toolbar->addAction(QStringLiteral("Refresh Now"));
    layout->addWidget(toolbar);

    m_stack = new QStackedWidget(this);
    layout->addWidget(m_stack, 1);

    // --- Main page: thumbnail grid + preview pane -------------------------
    auto *mainPage = new QWidget(m_stack);
    auto *mainLayout = new QVBoxLayout(mainPage);

    m_splitter = new QSplitter(Qt::Horizontal, mainPage);

    m_list = new QListWidget(m_splitter);
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(kThumbWidth, kThumbHeight));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setUniformItemSizes(true);
    m_list->setSpacing(8);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setAccessibleName(QStringLiteral("Saved wallpapers"));
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    auto *previewPane = new QWidget(m_splitter);
    auto *previewLayout = new QVBoxLayout(previewPane);
    m_previewImage = new QLabel(previewPane);
    m_previewImage->setAlignment(Qt::AlignCenter);
    m_previewImage->setMinimumHeight(200);
    m_previewImage->setScaledContents(false);
    m_previewMeta = new QLabel(previewPane);
    m_previewMeta->setWordWrap(true);
    m_previewMeta->setTextInteractionFlags(Qt::TextSelectableByMouse);
    previewLayout->addWidget(m_previewImage, 1);
    previewLayout->addWidget(m_previewMeta);
    previewLayout->addStretch();

    m_splitter->addWidget(m_list);
    m_splitter->addWidget(previewPane);
    m_splitter->setStretchFactor(0, 2);
    m_splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(m_splitter, 1);

    m_statusLabel = new QLabel(mainPage);
    mainLayout->addWidget(m_statusLabel);

    m_stack->addWidget(mainPage);

    // --- Empty page ---------------------------------------------------------
    m_emptyPage = new QWidget(m_stack);
    auto *emptyLayout = new QVBoxLayout(m_emptyPage);
    auto *emptyLabel = new QLabel(QStringLiteral("No wallpapers saved yet"), m_emptyPage);
    emptyLabel->setAlignment(Qt::AlignCenter);
    auto *emptyRefreshButton = new QPushButton(QStringLiteral("Refresh Now"), m_emptyPage);
    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyLabel);
    emptyLayout->addWidget(emptyRefreshButton, 0, Qt::AlignCenter);
    emptyLayout->addStretch();
    m_stack->addWidget(m_emptyPage);

    connect(setAction, &QAction::triggered, this, &GalleryWindow::setSelectedAsWallpaper);
    connect(deleteAction, &QAction::triggered, this, &GalleryWindow::deleteSelected);
    connect(copyAction, &QAction::triggered, this, &GalleryWindow::copySelectedDescription);
    connect(folderAction, &QAction::triggered, this, &GalleryWindow::showInFolder);
    connect(refreshAction, &QAction::triggered, this, &GalleryWindow::refreshRequested);
    connect(emptyRefreshButton, &QPushButton::clicked, this, &GalleryWindow::refreshRequested);

    connect(m_list, &QListWidget::itemSelectionChanged, this, &GalleryWindow::onSelectionChanged);
    connect(m_list, &QListWidget::itemActivated, this, &GalleryWindow::onItemActivated);

    // Delete/Backspace on the list deletes the selection; Enter/Return
    // (itemActivated, connected above) sets the current selection.
    // QKeySequence::Delete alone maps to forward-Delete/Meta+D on macOS,
    // where most keyboards only have a Backspace-shaped key — add it
    // explicitly so Delete-the-entry works there too.
    QList<QKeySequence> deleteKeys = QKeySequence::keyBindings(QKeySequence::Delete);
    deleteKeys << QKeySequence(Qt::Key_Backspace);
    deleteAction->setShortcuts(deleteKeys);
    deleteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_list->addAction(deleteAction);

    // Right-click context menu — the same QAction objects as the toolbar, so
    // there's one place (buildUi's local variables here) that owns them and
    // wires their behavior; a QAction can happily live in both.
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this, setAction, deleteAction,
                                                                       copyAction, folderAction](const QPoint &pos) {
        QMenu menu(m_list);
        menu.addAction(setAction);
        menu.addAction(deleteAction);
        menu.addAction(copyAction);
        menu.addSeparator();
        menu.addAction(folderAction);
        menu.exec(m_list->viewport()->mapToGlobal(pos));
    });
}

void GalleryWindow::refreshList()
{
    m_entries = WallpaperLibrary::scan(BingClient::saveDir());

    m_list->clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        const WallpaperLibrary::Entry &e = m_entries.at(i);
        QString label = e.title.isEmpty() ? e.date.toString(Qt::ISODate) : e.title;
        if (label.size() > 40)
            label = label.left(37) + QStringLiteral("...");
        label += QStringLiteral("\n%1").arg(e.date.toString(Qt::ISODate));

        auto *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, i);
        item->setSizeHint(QSize(kThumbWidth + 16, kThumbHeight + 48));
        item->setToolTip(e.copyright.isEmpty() ? e.title : e.copyright);
        m_list->addItem(item);
        queueThumbnail(e.path);
    }

    m_stack->setCurrentWidget(m_entries.isEmpty() ? m_emptyPage : m_stack->widget(0));
    updateStatusLine();
    updatePreview();
}

void GalleryWindow::queueThumbnail(const QString &path)
{
    auto *job = new ThumbnailJob(path);
    connect(job, &ThumbnailJob::ready, this, &GalleryWindow::onThumbnailReady, Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(job);
}

void GalleryWindow::onThumbnailReady(const QString &path, const QImage &image)
{
    if (image.isNull())
        return;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_entries.size() && m_entries.at(idx).path == path) {
            item->setIcon(QPixmap::fromImage(image));
            break;
        }
    }
}

void GalleryWindow::updateStatusLine()
{
    const qint64 total = WallpaperLibrary::totalSize(m_entries);
    m_statusLabel->setText(QStringLiteral("%1 image%2 · %3 on disk")
                                .arg(m_entries.size())
                                .arg(m_entries.size() == 1 ? QString() : QStringLiteral("s"))
                                .arg(formatSize(total)));
}

QList<WallpaperLibrary::Entry> GalleryWindow::selectedEntries() const
{
    QList<WallpaperLibrary::Entry> out;
    for (QListWidgetItem *item : m_list->selectedItems()) {
        const int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_entries.size())
            out.append(m_entries.at(idx));
    }
    return out;
}

void GalleryWindow::onSelectionChanged()
{
    updatePreview();
}

void GalleryWindow::updatePreview()
{
    const auto selected = selectedEntries();
    if (selected.size() != 1) {
        m_previewImage->setPixmap({});
        m_previewImage->setText(selected.isEmpty() ? QStringLiteral("No selection")
                                                     : QStringLiteral("%1 images selected").arg(selected.size()));
        m_previewMeta->clear();
        return;
    }

    const WallpaperLibrary::Entry &e = selected.first();
    QImageReader reader(e.path);
    reader.setAutoTransform(true);
    // Decode already scaled down instead of reading the full UHD image (up
    // to ~3840x2160, ~33 MB as an QImage) on the GUI thread just to shrink
    // it afterwards — this runs synchronously on selection, so a full-size
    // decode would stall the UI on every click.
    const int previewWidth = qMax(m_previewImage->width(), 400);
    const QSize original = reader.size();
    if (original.isValid())
        reader.setScaledSize(original.scaled(previewWidth, 400, Qt::KeepAspectRatio));
    const QImage full = reader.read();
    if (!full.isNull())
        m_previewImage->setPixmap(QPixmap::fromImage(full));

    QSettings settings;
    const bool isCurrent = settings.value("currentWallpaperPath").toString() == e.path;
    QString meta = QStringLiteral("<b>%1</b><br>%2").arg(e.title.toHtmlEscaped(), e.date.toString(Qt::ISODate));
    if (!e.market.isEmpty())
        meta += QStringLiteral(" · %1").arg(e.market);
    if (isCurrent)
        meta += QStringLiteral("<br><i>Current wallpaper</i>");
    if (!e.copyright.isEmpty())
        meta += QStringLiteral("<br><br>%1").arg(e.copyright.toHtmlEscaped());
    m_previewMeta->setText(meta);
}

void GalleryWindow::onItemActivated(QListWidgetItem *)
{
    setSelectedAsWallpaper();
}

void GalleryWindow::setSelectedAsWallpaper()
{
    const auto selected = selectedEntries();
    if (selected.size() != 1)
        return;
    const WallpaperLibrary::Entry &e = selected.first();

    if (!WallpaperSetter::setWallpaper(e.path)) {
        QMessageBox::warning(this, QStringLiteral("Bing Wallpaper"),
                              QStringLiteral("Could not set the wallpaper."));
        return;
    }

    QSettings settings;
    settings.setValue("currentWallpaperPath", e.path);
    emit wallpaperApplied(e.path, e.copyright);
    updatePreview();
}

void GalleryWindow::deleteSelected()
{
    const auto selected = selectedEntries();
    if (selected.isEmpty())
        return;

    QSettings settings;
    const QString currentPath = settings.value("currentWallpaperPath").toString();
    const bool includesCurrent = std::any_of(selected.begin(), selected.end(),
        [&](const WallpaperLibrary::Entry &e) { return e.path == currentPath; });

    QString question = selected.size() == 1
        ? QStringLiteral("Move this wallpaper to the Trash?")
        : QStringLiteral("Move these %1 wallpapers to the Trash?").arg(selected.size());
    if (includesCurrent) {
        question += QStringLiteral(
            "\n\nOne of them is your current wallpaper. It stays on screen until you change "
            "it, but some desktops may revert to a default background after logout/restart.");
    }

    if (QMessageBox::question(this, QStringLiteral("Delete Wallpaper"), question,
                               QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }

    for (const WallpaperLibrary::Entry &e : selected) {
        WallpaperLibrary::remove(e);
        WallpaperLibrary::markDeleted(e.baseName);
    }
    refreshList();
}

void GalleryWindow::copySelectedDescription()
{
    const auto selected = selectedEntries();
    if (selected.size() != 1)
        return;
    const QString &text = selected.first().copyright;
    if (!text.isEmpty())
        QApplication::clipboard()->setText(text);
}

void GalleryWindow::showInFolder()
{
    WallpaperSetter::openDir(BingClient::saveDir());
}

#include "GalleryWindow.moc"
