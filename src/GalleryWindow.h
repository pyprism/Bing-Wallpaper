#pragma once

#include <QWidget>
#include <QList>

#include "WallpaperLibrary.h"

class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;
class QStackedWidget;
class QSplitter;
class QFileSystemWatcher;
class QTimer;

// A real, lazily-created app window for browsing every saved Bing wallpaper:
// pick one to set as the current desktop background, delete one or several,
// copy its description, or reveal the save folder. Created on first "Open
// Gallery…" and destroyed on close (Qt::WA_DeleteOnClose) so a tray-only,
// low-RSS app doesn't carry window-manager/graphics-scene overhead when
// nobody asked for it.
class GalleryWindow : public QWidget
{
    Q_OBJECT

public:
    explicit GalleryWindow(QWidget *parent = nullptr);
    ~GalleryWindow() override;

signals:
    // "Refresh Now" in the Gallery toolbar — the window has no BingClient of
    // its own, so it asks the owner (TrayController) to do the fetch.
    void refreshRequested();
    // A cached image was applied directly (no network involved) — lets
    // TrayController keep its "last copyright" / notification state in sync.
    void wallpaperApplied(const QString &path, const QString &copyright);

private slots:
    void onSelectionChanged();
    void onItemActivated(QListWidgetItem *item);
    void onThumbnailReady(const QString &path, const QImage &image);
    void setSelectedAsWallpaper();
    void deleteSelected();
    void copySelectedDescription();
    void showInFolder();

private:
    void buildUi();
    void refreshList();
    void updatePreview();
    void updateStatusLine();
    void queueThumbnail(const QString &path);
    QList<WallpaperLibrary::Entry> selectedEntries() const;

    QStackedWidget *m_stack;
    QWidget *m_emptyPage;
    QSplitter *m_splitter;
    QListWidget *m_list;
    QLabel *m_previewImage;
    QLabel *m_previewMeta;
    QLabel *m_statusLabel;
    QFileSystemWatcher *m_watcher;
    QTimer *m_refreshDebounce;

    QList<WallpaperLibrary::Entry> m_entries;
};
