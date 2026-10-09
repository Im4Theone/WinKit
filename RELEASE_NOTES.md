# WinKit 2.0.0

WinKit has been rebuilt from the ground up as a native Windows app. It is now a single `WinKit.exe` with no .NET runtime to bundle, and the interface is HTML/CSS/JS shown in an embedded Microsoft Edge WebView2 window. The tools, behaviour and your saved settings carry over from 1.x.

## Highlights

- **One small file.** The portable download is just `WinKit.exe`. Download it and run it.
- **Same toolkit, same behaviour.** All five tool categories are ported from the 1.x source: System, Network, Cleanup, Diagnostics and Appearance.
- **A calmer new look.** A warm, flat design with hairline-bordered cards and a muted palette. The new default theme is **Dusk**, with **Dawn** as its light companion.
- **Your data carries over.** Settings, profile, activity history and custom themes use the same files and format as 1.x (`%LOCALAPPDATA%\WinKit`).
- **Upgrades cleanly.** The installer uses the same product ID as 1.x, so 2.0 replaces an existing install and removes the old .NET files.

## What's included

**System**
- Process manager with real memory and CPU figures, including protected Windows services. System and Idle can't be ended by mistake.
- Startup manager, services manager (running services are shown in green), installed apps with uninstall
- Environment variables editor and hosts file editor
- System information with live uptime

**Network**
- Ping and traceroute with live results and colour-coded latency, DNS lookup
- IP configuration showing IPv4, IPv6, gateway, DNS servers and DHCP for every adapter
- Flush DNS, renew/release DHCP, reset Winsock and TCP/IP (each asks for administrator approval)

**Cleanup**
- Scans user and Windows temp files, the thumbnail cache, the Windows Update cache and the Recycle Bin
- You review before anything is deleted, with a size bar per category, and the exact space freed is shown afterwards
- Never follows junctions or symlinks, and refuses to clean drive roots or system folders

**Diagnostics**
- 11 checks: disk health, disk space, DNS, drivers, internet, network adapters, services, startup, temp files, Windows integrity, Windows Update
- 5 one-click fixers: flush DNS, clean temp files, restart update services, Device Manager shortcut, SFC/DISM repair with live output
- Without administrator rights the Windows integrity check says so and offers **Check as Administrator**. It never reports false corruption.
- The internet check falls back to a real connection test, so networks that block ping aren't reported as offline
- Results are saved between sessions and summarised on the Dashboard

**App**
- Dashboard with system overview, usage meters that turn amber and red as they fill, and quick actions
- Six built-in themes (Dusk, Dawn, Midnight, Carbon, Slate, Light), a full custom theme editor with a live preview strip, theme import/export
- Activity log with severity colours, notifications, start minimized, start with Windows
- Keyboard shortcuts: **Ctrl+1 to Ctrl+8** jump straight to Dashboard, System, Network, Cleanup, Diagnostics, Activity, Settings and About
- Update check with checksum verification, optional crash reports (sent only when you click Send)

## Downloads

| File | |
|---|---|
| `WinKitSetup-2.0.0.exe` | Installer (Program Files, Start Menu, uninstaller, self-updates) |
| `WinKit-2.0.0-portable.exe` | Portable program, no install |
| `WinKit-2.0.0-portable.zip` | Same portable exe, zipped (used by the built-in updater) |
| `SHA256SUMS.txt` | Checksums |

## Requirements

- Windows 10 version 1809 or later (64-bit)
- Microsoft Edge WebView2 Runtime. It is already included with Windows 11 and up-to-date Windows 10. If it's missing, WinKit offers the download link on first launch.

## Changes from 1.x

- The window uses the standard Windows title bar (dark themed) instead of the custom one.
- The default theme is now Dusk. If you were on the old default (Midnight), WinKit switches you to Dusk once on first launch. You can choose Midnight again in Settings and it stays. Any other theme you picked is left alone.
- Cleanup shows a progress bar and a Cancel button while scanning.
- Diagnostics shows a live log while a fix is running.
- Freeze in the process manager still only pauses the refresh, as in 1.x. It does not suspend the process.

## Known issues

- Some checks need administrator rights to give a full answer: Windows integrity (DISM) and Disk Health (SMART). Without them WinKit reports that instead of guessing. Use **Run as Administrator** in Settings, or the **Check as Administrator** button on the result.
- This is a full rewrite, so please report anything that behaves differently from 1.x: https://github.com/Im4Theone/WinKit/issues
