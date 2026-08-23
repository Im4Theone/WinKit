# Changelog

All notable changes to WinKit are documented in this file.

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
