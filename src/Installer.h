#pragma once

#include <QString>

namespace Installer
{
// Copies the running executable to its standard per-OS install location (where
// applicable). Returns true if a newer/installed copy was launched to replace
// this process — the caller must then release any held locks and exit(0)
// itself rather than calling std::exit() here (which would race a lock
// release against the just-launched child).
bool ensureInstalled();

// Applies the current autostart/enabled preference (QSettings) to the OS.
// Call once at startup, after ensureInstalled().
void syncAutostart();

bool isAutostartEnabled();
void setAutostartEnabled(bool enabled);

// Pure helpers with no execution — exposed for unit testing on any OS.
QString xmlEscape(const QString &s);
bool isUnderApplicationsFolder(const QString &execPath);
QString quotedRegistryPath(const QString &path);
QString quoteExecForDesktopEntry(const QString &path);
}
