#pragma once

namespace Installer
{
// Copies the running executable to its standard per-OS install location (where
// applicable) and registers autostart-on-login. If a self-copy happens, this
// relaunches the installed copy and terminates the current process.
void ensureInstalled();

// Applies the current autostart/enabled preference (QSettings) to the OS.
// Call once at startup, after ensureInstalled().
void syncAutostart();

bool isAutostartEnabled();
void setAutostartEnabled(bool enabled);
}
