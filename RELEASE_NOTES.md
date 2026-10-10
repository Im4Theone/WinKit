# WinKit 2.1.0

Five new tools, a smarter Dashboard and a smaller download. WinKit is still one `WinKit.exe` of under 1 MB with nothing else to install. Your settings, themes and history carry over.

## New in 2.1.0

- **Disk usage.** Pick a drive or your user folder and click through the biggest folders and files to see where your space went. It only reads, it never deletes.
- **Wi-Fi details and a connection test.** See signal, channel, speed and security for your Wi-Fi, plus nearby networks. Or run the test: it checks your adapter, router, internet and DNS and tells you where the problem is.
- **Open ports.** Every open port and connection with the program that owns it.
- **Health report.** Save or copy a plain-text summary of your PC to send to whoever is helping you. Computer name, addresses and Wi-Fi names are left out unless you tick a box.
- **Hardware health.** The Dashboard now shows each drive's free space and, on laptops, battery charge and how much of its original capacity is left. It also suggests a restart after two weeks of uptime.
- **Right-click menus** on processes, services, apps, startup items, ports and disk usage: copy, show in folder, search online and the usual actions.
- **Quick Open (Ctrl+K)** jumps to any page, tab or action. Every list has a search box (Ctrl+F).
- **Click to copy** IP addresses, ports and system details, or use Copy all on System info.
- **WinKit remembers its window**: same size, position and maximised state next time.
- **Processes start sorted by memory**, biggest first, like Task Manager.
- **Smaller and quieter.** The interface is stored compressed, nothing polls while the window is minimised, and the portable exe is about 870 KB.
- **Fixes.** Better contrast in every theme, correct plurals, crash recovery for the web view, safer updates of renamed portable copies, and many small layout fixes.

## What WinKit is

- **One small file.** The portable download is just `WinKit.exe`. Download it and run it.
- **A calm look.** A warm, flat design with six themes (Dusk is the default) plus Auto, which follows Windows.
- **Same toolkit as 1.x.** System, Network, Cleanup, Diagnostics and Appearance, rebuilt as a native Windows app.
- **Upgrades cleanly.** The installer uses the same product ID as before, so it replaces an existing install.

## What's included

**System**
- Process manager with real memory and CPU figures, including protected Windows services, biggest memory users first. System and Idle can't be ended by mistake.
- Click a program's path (in Processes or Startup) to open its folder in File Explorer
- Startup manager, services manager (running services are shown in green), installed apps with uninstall
- Environment variables editor and hosts file editor
- System information with live uptime

**Network**
- Ping and traceroute with live results and colour-coded latency, DNS lookup
- **Ports**: every open port and connection with the program that owns it, so you can see what is using port 3000 or what is talking to the internet
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
- Results are saved between sessions and summarised on the Dashboard. Opening the page shows your last results instead of scanning again; a scan runs on the first visit, when you press Run Diagnostics, or when the results are over 10 minutes old.

**App**
- Dashboard with system overview, usage meters that turn amber and red as they fill, and quick actions
- Six built-in themes (Dusk, Dawn, Midnight, Carbon, Slate, Light) picked from a visual appearance chooser, plus an Auto option that follows Windows live. A full custom theme editor and theme import/export
- Activity log with severity colours, notifications, start minimized, start with Windows
- Keyboard shortcuts: **Ctrl+K** quick open, **Ctrl+F** search, **Ctrl+R** refresh, **Ctrl+B** hide the sidebar, **Ctrl+,** settings, and **Ctrl+1 to Ctrl+8** for the main pages
- WinKit reopens where you left it, with the same size, position and maximised state
- Click an IP address, port or system detail to copy it, or use **Copy all** on System info for a support request
- The window title shows the page you are on, and the sidebar shows how many issues Diagnostics found
- Quiet in the background: nothing polls while the window is minimised
- Update check with checksum verification (a portable copy is updated in place under its own file name), optional crash reports (sent only when you click Send)

## Downloads

| File | |
|---|---|
| `WinKitSetup-2.1.0.exe` | Installer (Program Files, Start Menu, uninstaller, self-updates) |
| `WinKit-2.1.0-portable.exe` | Portable program, no install |
| `WinKit-2.1.0-portable.zip` | Same portable exe, zipped (used by the built-in updater) |
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
