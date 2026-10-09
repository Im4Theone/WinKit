# API_infra - WinKit.Core / Infrastructure / Themes / Updater port

Files: `mod_infra.h` (native, `Mod_Infra` + `Updater_Main`), `js/core.js` (`window.WK`). UI developers need only this file and the header comment of `core.js`.

Persisted files (same names/JSON as the C# app, under `%LOCALAPPDATA%\WinKit`): `settings.json` (PascalCase keys), `profile.json`, `activity.json`, `Themes\<name>.json` (camelCase, string enums), `Logs\crash.log`.

## 1. Bridge
JS -> host: `chrome.webview.postMessage(JSON.stringify({id:"7", cmd:"store_read", name:"settings"}))`. All args are flat strings (`true`/`false`, numbers as text, objects JSON-stringified). Reply: `{id, ok:true, data:<json value>}` or `{id, ok:false, error:"msg"}`. Events: `{event:"update_progress", data:{...}}`. `WK.native(cmd,args)` does this; `WK.on(event, fn)` receives events. Reserved arg names: `id`, `cmd`. Without `chrome.webview` (plain browser) core.js uses localStorage mocks (`WK.mock` to override).

## 2. Native commands (`Mod_Infra`)
`out` is the JSON value in the table; failures become `{ok:false,error}`.

| cmd | args | result |
|---|---|---|
| `store_read` | `name` (e.g. `settings`, `Themes/My Theme`) | the file's JSON value, or `null` if missing. A file that is not valid JSON is moved to `<file>.corrupt-<yyyyMMddHHmmss>.json`, logged to crash.log, and `null` returned |
| `store_write` | `name`, `json` (JSON text) | `true`. Validates JSON, temp file + atomic replace, flushes. Names: up to 3 `/`-separated segments, no `..`, no `<>:"\|?*`, no reserved device names, <=100 chars/segment |
| `store_delete` | `name` | `true` (missing = ok) |
| `store_list` | `dir` ("" = root, e.g. `Themes`) | sorted array of names (no `.json`; quarantined files excluded) |
| `crash_log` | `text`, `tag?` | `true` (appends to Logs\crash.log in the C# format) |
| `app_info` | - | `{version,exePath,exeDir,dataDir,logsDir,themesDir,isAdmin,isInstalledLocation,userName,architecture,windowsVersion,windowsBuild,systemUsesLightTheme}` |
| `app_quit` | - | `true` (calls `HostQuit`) |
| `system_uses_light_theme` | - | bool (registry AppsUseLightTheme == 1) |
| `is_admin` | - | bool |
| `relaunch_elevated` | `arguments?` | `true` = elevated copy started, `HostQuit()` is then called; `false` = UAC declined/failed |
| `autostart_get` / `autostart_set` | `enabled` | bool / `true`. HKCU\...\Run value `WinKit` = `"<exe>" --start-minimized` |
| `update_check` | - | `{status:"UpToDate"\|"UpdateAvailable"\|"Failed", latestVersion, releaseUrl, releaseNotes, installerAssetUrl, installerSha256, portableAssetUrl, portableSha256, errorMessage}` (absent = null). Source: GitHub `Im4Theone/WinKit` releases/latest. Result is cached natively |
| `update_download_apply` | `latestVersion` (must match last `update_check`) | `{status:"Started"\|"Failed", errorMessage}`. Installed under Program Files: downloads `WinKitSetup-<v>.exe`, verifies SHA-256, launches `/VERYSILENT /NORESTART /launchafterinstall=1`. Otherwise portable: download zip, verify, extract (tar.exe, PowerShell fallback) to `%TEMP%\WinKitUpdate\staging-*`, start updater (section 5). On `Started` the caller must quit (`WK.about.install()` does) |
| `error_report_create` | `component, exceptionType, exceptionMessage, stackTrace` | `{errorId:"WK-XXXXXXXX", winKitVersion, windowsVersion, architecture, component, exceptionType, exceptionMessage, stackTrace, timestamp, text}` - `text` is the exact preview/body text of `DiagnosticReport.ToReportText()` |
| `error_report_send` | the 9 report fields (no `text`) | `{ok:true,errorMessage:null}` or `{ok:false,errorMessage}`. POST to `https://winkit-error-reports.kaloyankrastev2013.workers.dev/report`, camelCase JSON, only those 9 fields. No redaction exists in the C# and none was added |
| `open_url` | `url` (http/https only) | `true` |
| `open_folder` | `path` or `@data`/`@logs`/`@themes`/`@downloads` | `true` |
| `reveal_in_explorer` | `path` (file selects it) | `true` |
| `clipboard_set` | `text` | `true` (fallback; `WK.shell.copy` prefers navigator.clipboard) |
| `import_file_dialog` | `title, filter` (`Name (*.json)\|*.json`) | `{path,text}` or `null` if cancelled |
| `export_file_dialog` | `title, filter, defaultName, defaultExt, text` | saved path, or `null` if cancelled |
| `set_window_material` | `mode` = `dark`\|`light` (optional `,mica`/`,acrylic`/`,tabbed`/`,none`) | `true` -> `HostSetMaterial` |
| `set_title_bar_colors` | `dark`, `caption`, `text`, `border` (`#RRGGBB` or empty = default) | `true` -> `HostSetTitleBarColors` |

Native events: `update_progress` -> `{stage:"download"|"verify"|"extract"|"launch", received, total, percent}` (bytes; percent 0-100).

## 3. Host (winkit.cpp) must provide
```cpp
void HostQuit();                                    // thread-safe, async (e.g. PostMessage(hwnd, WM_CLOSE))
void HostSetMaterial(const ws &mode);               // "dark"|"light" [",mica"|",acrylic"|",tabbed"|",none"]. C# parity: DWMWA_USE_IMMERSIVE_DARK_MODE(20)=dark?1:0 and DWMWA_WINDOW_CORNER_PREFERENCE(33)=DWMWCP_ROUND(2), only on build >= 22000; backdrop token -> DWMWA_SYSTEMBACKDROP_TYPE(38)
void HostSetTitleBarColors(bool dark, unsigned caption, unsigned text, unsigned border); // COLORREF 0x00BBGGRR; 0xFFFFFFFF = DWMWA_COLOR_DEFAULT (attrs 35/36/34)
void PushEvent(const ws&, const ws&);               // already in common.h contract
```
Called from worker threads: marshal to the UI thread. Also available to the host: `Infra_StartMinimized()` (reads `settings.json` StartMinimized; the C# ignores the `--start-minimized` arg, only the setting counts), `Updater_Main`, `Infra_RunProcess` / `Infra_RunElevated` (ProcessRunner port for other native modules). Build with `/DWINKIT_VERSION=L"x.y.z"` to change the version (default `1.2.0`) and `WINKIT_EXE_NAME` (default `WinKit.exe`). Libs (auto via `#pragma comment`): winhttp, bcrypt, shell32, comdlg32, advapi32. After `relaunch_elevated` the new instance starts while the old one is still closing: do not hard-fail on a single-instance mutex.

## 4. Theme -> CSS variables (set on `:root` by `WK.themes.applyToDom`)
Colours: `--wk-background --wk-surface --wk-surface-elevated --wk-border --wk-text-primary --wk-text-secondary --wk-text-muted --wk-accent --wk-accent-hover --wk-on-accent --wk-success --wk-warning --wk-error --wk-update`, each also as `--wk-<name>-rgb` (`r, g, b`). 8-digit hex is WPF `#AARRGGBB`; it is converted to CSS `#RRGGBBAA`.
Layout: `--wk-radius` (CornerRadius px), `--wk-radius-sm` (min(r,12)), `--wk-sidebar-width`. Fonts (px, = base x fontScale x (Compact ? 0.85 : 1)): `--wk-font-size-small` (11.5) `-body` (13) `-title` (15) `-heading` (21) `-display` (28); `--wk-font-scale`. Animations: `--wk-anim-fast` / `--wk-anim-normal` = Full 120/160ms, Reduced 60/80ms, Off 0/0. Attributes: `data-wk-density="comfortable|compact"`, `data-wk-animations="full|reduced|off"`, `data-wk-theme="<name>"`, `color-scheme` dark/light (luminance of Background < 0.5). `Off` also disables all CSS transitions/animations globally. Use these variables in every stylesheet; dialogs/toasts in core.js already do.
Built-in themes (exact JSON): Carbon (Compact, r4, sidebar 224), Light, Midnight (default, r6, 232), Slate (r8, 236). Lists show `System` first (follows Windows light/dark: Light or Midnight), then built-ins, then custom. Unknown theme name falls back to Carbon (C# behaviour).

## 5. Updater switch
`WinKit.exe --apply-update "<stagingDir>" "<installDir>" <pid>` -> in `wWinMain`, before any window/WebView: `return Updater_Main(GetCommandLineW());`. A standalone `WinKit.Updater.exe` (if shipped in the install dir, used first, like the C#) takes exactly `"<stagingDir>" "<installDir>" <pid>` (switch optional). Otherwise the native update code starts a private copy of the host exe from `%TEMP%\WinKitUpdate\updater-*.exe` with the switch. It waits up to 30 s for `<pid>`, sleeps 500 ms, copies all files in parallel (2-8 threads, 5 retries, a locked running exe is renamed aside), deletes staging, relaunches `<installDir>\WinKit.exe`; returns 0, or 1 after logging to crash.log and still relaunching.

## 6. WK public API
Full list with signatures is the header comment of `js/core.js`. Auto-init at load; `await WK.ready` (`{onboardingCompleted}`). Summary of behaviours the UI relies on:
- **Start**: `WK.ready` -> show Onboarding if `!onboardingCompleted` (hide sidebar/title chrome during onboarding), else Dashboard. Start-minimized is a host concern. If `settings.autoCheckForUpdates`, `WK.updates.startAutoChecks()` runs automatically (now + every 6 h): an `Update` toast (title `WinKit <v> is available`, text `Click to review what's changed and install it.`, button `Update Now`, no auto-dismiss, skipped for the "skipped" version). Clicking emits `WK.on('update:review', ({result}) => { navigate to About; WK.about.showPendingUpdate(result); })` - the UI must wire this.
- **Notifications**: `WK.notify({title,description,severity,actionText,action,autoDismissMs})` default 3000 ms, `null` = persistent. Newest on top, bottom-right, 340 px cards, severity bar (Success/Error/Warning/Update/Info), x button, hover pauses the countdown, leaving resumes it; fade 160 ms (instant if theme animation is Off). Rendered by core.js in `#wk-toasts` (z-index 9000). Note: the C# stores the `NotificationsEnabled` setting but never checks it; ported as-is (UI may gate with `WK.settings.get().notificationsEnabled`).
- **Dialogs**: `WK.confirm`, `WK.alert`, `WK.updateConfirm`, `WK.showErrorReport` (queued, modal, Enter = confirm, Esc = cancel). Unhandled JS errors/rejections open the error-report dialog automatically (title `Something went wrong`, text `WinKit ran into an unexpected error. You can keep using the app, but some features may not work correctly until you restart.`, buttons `Preview` / `Close` / `Send to Developers`; sending: `Sending...`, success `Error report sent successfully. Report ID: <id>`, failure `Couldn't send the report: <msg> The report hasn't been lost - you can retry or copy it.` + button `Retry`).
- **Activity auto-entries**: `Theme changed to <name>` (Info, ThemeAndSettings), `Theme "<name>" saved` (Success, ThemeAndSettings), `Updating to <v>` (Info, Action). Other modules call `WK.activity.record(title, kind, detail, category)`; log keeps the newest 50.

## 7. Screen behaviour / copy

### Settings (title `Settings`; sections as cards, max width 720)
On open call `await WK.settings.refreshFromSystem()` (re-reads the real auto-start state), then bind to `WK.settings.get()`, `WK.themes.names()`.
1. **Appearance**: row `Theme` / muted `Choose a built-in theme, follow Windows, or use a custom one.` + dropdown (180 px) of `WK.themes.names()`, selected = `WK.themes.selected()`; change -> `WK.themes.select(name)`. Buttons: `Create theme` (`WK.themeEditor.openCreate()`), `Edit current theme` (`openEditCurrent()`; built-in/System -> edits a copy named `<name> Copy`, `<name> Copy 2`...), `Import` (`WK.themes.importFlow()`), `Export` (`exportFlow()`), `Reset to default` (`resetToDefault()` -> Midnight). The original has no Delete button but `WK.themes.deleteFlow()` exists (confirm `Delete theme` / `Delete "<name>"? This can't be undone.` / `Delete`, destructive); offer it only for custom themes if wanted.
   - Import failure dialog: `Import failed` / `That file isn't a valid WinKit theme.`; toasts: `Theme imported` / `Theme exported` (description = path) / `Theme saved`, all Success.
   - **Theme editor** (shown while `WK.themeEditor.isOpen()`; re-render on `themeEditor:changed`): top row: name textbox (live), `Cancel` (`close()`), `Save theme` (`save()`). Label `Colors`, 14 swatch + 96 px hex textbox + label: Background, Surface, Elevated surface, Border, Primary text, Secondary text, Muted text, Accent, Accent hover, Text on accent, Success, Warning, Error, Update (keys `background surface surfaceElevated border textPrimary textSecondary textMuted accent accentHover onAccent success warning error update`; swatch colour via `WK.themes.hexToCss(hex) || 'transparent'`). Label `Layout`: sliders `Corner radius` 0-24 (shown `0`), `Sidebar width` 180-320 (`0`), `Font scale` 0.8-1.4 step 0.01 (`0.00`). Dropdowns `Density` (Comfortable, Compact) and `Animation intensity` (Full, Reduced, Off). Every edit calls `WK.themeEditor.set(field,value)`/`setColor` and live-previews the whole app only while all 14 colours are valid hex (`#RRGGBB` or `#AARRGGBB`).
   - Validation (shown by `save()` in an error dialog titled `Invalid theme`): `Theme name cannot be empty.` / `All theme colors must be valid hex codes (e.g. #4C8DFF).` / `Corner radius must be between 0 and 24.` / `Sidebar width must be between 180 and 320.` / `Font scale must be between 0.8 and 1.4.` (+ port additions: name with no file-name characters, > 100 chars, Windows reserved name).
2. **Personalization**: label `Your name`, textbox (max 280 px) bound to `WK.profile.current().name`, button `Save` -> `WK.settings.saveName(text)` (ignores blank; toast `Name updated`).
3. **Behavior** (toggle switches): `Start minimized` (`setStartMinimized`), `Start with Windows` (`setStartWithWindows`, writes the Run key), `Notifications` (`setNotificationsEnabled`), `Automatically check for updates` (`setAutoCheckForUpdates`; takes effect at next launch, like the C#). Defaults: off, off, on, on.
4. **Advanced**: `Running as administrator: True|False` (bold value), muted `WinKit only requests administrator access for the specific action that needs it.`, button `Run as Administrator` (hidden if `WK.admin.isElevated()`) -> `WK.admin.restart()` (confirm `Restart as Administrator` / `Are you sure you want to restart the program via Admin?` / `Restart as Administrator`; failure dialog `Couldn't restart as Administrator` / `The elevation request was cancelled or failed. WinKit is still running normally.`).

### About (max width 560)
`WinKit` (display), `Version <WK.app.version>`. Card 1: `A lightweight Windows toolkit for system information, diagnostics, cleanup, and networking.` + secondary `Every action WinKit takes is a real, explainable operation - no fake optimizations, no invented performance claims.` Card 2 `Privacy`: `No account required. No telemetry. Crash reports are only ever sent when you explicitly click "Send to Developers" - nothing is sent automatically. No permanent background process.` Card 3 `Updates`: status text = `WK.about.getState().statusText` (initial `Check for updates to see if a newer version is available.`; then `Checking for updates...`, `You're on the latest version.`, `WinKit <v> is available.`, `Couldn't check for updates: <err>`, `Downloading update...` (progress in `state.progress`), `Update failed: <err>`, `Skipped version <v>. You'll be notified again for future releases.`). Buttons: `Check for Updates` (disabled while `isChecking`; `WK.about.check()`), `Update` and `Skip this version` (visible when `updateAvailable`; `install()` / `skipThisVersion()`). `install()` shows the update dialog (`WinKit <v> is available`, `Review what's changed, then choose whether to install it now. Your settings, themes, and activity history are preserved.`, expanded `What's new` with notes + `Copy changelog`, buttons `Cancel` / `Install Update`), downloads, then quits the app. Re-render on `about:changed`.

### Onboarding (centered, 360 px, no chrome)
`WinKit` (display, centered), `Welcome to WinKit`, label `What should we call you?`, textbox prefilled with `WK.onboarding.defaultName()` (Windows user name), focused with text selected, Enter = Continue; full-width `Continue` button disabled while blank (`WK.onboarding.canContinue`). Click -> `await WK.onboarding.submit(name)` then navigate to Dashboard.

### Activity (max width 720)
`Activity` (display), `Everything WinKit has done, most recent first.`, filter buttons `All` | `Actions` | `Themes & Settings` (selected one uses accent background / on-accent text) -> `WK.activity.filtered('all'|'actions'|'themesAndSettings')`, re-render on `activity:changed`. Card list (no padding), each row: small muted dot, title, optional muted detail, right-aligned timestamp `WK.activity.formatTimestamp(entry)`. Empty: `No activity in this category yet.`

### Formatting helpers
`WK.fmt.uptime(seconds)` -> `1d 2h 3m 4s` / `2h 3m 4s` / `3m 4s`; `WK.fmt.bytes(n)` -> `1.5 KB` (B/KB/MB/GB/TB, one decimal, trailing `.0` dropped); `WK.fmt.megabytes(mb)` -> `512 MB` / `1.5 GB` (empty for non-numbers).

## 8. Not ported / deviations
- `ProcessRunner`: C++ helpers only (`Infra_RunProcess`, `Infra_RunElevated`); not exposed to JS on purpose (no arbitrary process execution from the page). `OperationResult`, DI wiring, diagnostics/cleanup models are not part of this scope.
- `AccentOverrideHex` is stored in settings but unused (as in the C# UI).
- Error report has no redaction rules because the C# has none; JS stack traces are sent as-is.
- Update download URLs/hashes are not trusted from JS: the native side keeps the last `update_check` result and only downloads https URLs on github.com / githubusercontent.com.
- Rename-save of a theme where only the letter case changes no longer deletes the file it just wrote (the C# would).
