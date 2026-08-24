# Changelog

All notable changes to WinKit are documented in this file.

## 1.1.2 — 2026-08-24

Auto-updater simplification — no visible change, but the update pipeline is more robust.

### Changed
- The auto-updater now verifies downloads against the SHA256 digest GitHub already computes for every release asset, instead of fetching a separate `.sha256` file. One fewer request per update, and no chance of a sidecar file ever drifting from the actual asset.
- Release builds no longer generate `.sha256` sidecar files — GitHub's own per-asset digest is the source of truth.

## 1.1.1 — 2026-08-23

Bug fixes: Activity page and window dragging, plus removal of a window material setting that never actually worked.

### Fixed
- The Activity page could throw during rendering — an internal style tried to set its own `Style` property from within one of its own triggers, which WPF doesn't allow. This showed up as an empty Activity page, and the repeated failure also stalled the UI thread badly enough that the window couldn't be dragged while that page was open.

### Removed
- Mica and Acrylic window materials. The underlying DWM calls succeeded, but the translucent surface was never actually composited through WPF's client-area rendering, so the setting had no real visual effect despite appearing to. Rather than keep an option that silently does nothing, it's been removed — windows are now always a solid surface.

## 1.1.0 — 2026-08-23

Adds error reporting and auto-updates, plus reliability fixes found through real use.

### Fixed
- Custom theme saving could crash with a `FileNotFoundException` under concurrent writes (e.g. rapid settings changes while saving a theme). Persistence is now serialized per file with atomic writes and automatic recovery from a corrupted file.
- Saving a custom theme could briefly flash back to a built-in theme immediately after Save, due to the theme dropdown's selection resetting during list refresh.
- The theme editor's "Text on accent" color wasn't part of the theme system, so a light/pastel custom accent could make button text illegible.
- The window's dark/light chrome detection used the theme's name instead of its actual background color.
- Mica/Acrylic window materials had no visible effect — an opaque background was painting over the translucent window surface.
- "Off" animation intensity now actually disables the notification fade instead of always animating.

### Added
- Global error reporting: an unexpected error now offers Preview / Close / Send to Developers, showing the exact diagnostic report before it's sent. Reports never include user files, credentials, settings, activity history, or theme contents.
- Auto-updater: checks GitHub Releases for new versions on startup and periodically, with manual "Check for Updates" (Settings → About), skip-this-version, and an auto-check toggle. Downloads are checksum-verified before being applied, and file replacement always happens from a separate process, never from inside the running app.

### Packaging
- Installer and portable builds now emit a `.sha256` checksum alongside each artifact, verified by the auto-updater before use.

## 1.0.0 — 2026-08-23

Initial release.

### System tools
- System information overview (CPU, memory, GPU, storage, network, uptime)
- Process manager with live auto-refresh and freeze/unfreeze
- Services manager
- Startup manager
- Installed applications, with clean uninstall support
- Environment variables editor
- Hosts file editor

### Network tools
- Ping
- Traceroute
- DNS lookup
- IP configuration
- Network maintenance (flush DNS, reset adapters, etc.)

### Cleanup
- Temporary and cache file cleanup

### Diagnostics
- One-click system diagnostics pass

### Appearance & personalization
- Dark theme system with Mica/Acrylic window materials
- Built-in themes (Carbon, Light, Midnight, Slate) plus custom theme JSON import/export
- Adjustable corner radius, sidebar width, font scale, density, and animation intensity
- First-run onboarding that captures your name and preferences
- Start minimized / start with Windows options
- Notifications
- Run as administrator from Settings → Advanced, with a confirmation prompt

### Persistence
- Username, theme selection, and activity history all persist across restarts, stored locally under `%LOCALAPPDATA%\WinKit` — no cloud sync, no telemetry

### Packaging
- Self-contained win-x64 build — no separate .NET install required
- Windows installer (Program Files install, Start Menu shortcut, optional desktop shortcut, registered uninstaller)
