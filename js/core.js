/* =====================================================================================================
 * WinKit core.js  -  plain browser script (no modules / bundler / libs, ES2020).  Exposes window.WK.
 * Port of WinKit.Core + WinKit.Infrastructure + WinKit.Themes logic and the UI-side logic of
 * SettingsViewModel, ThemeDraftViewModel, AboutViewModel, OnboardingViewModel, ActivityViewModel,
 * NotificationHostViewModel/NotificationItemViewModel, ThemeApplier, UptimeFormatter, DialogService.
 * Everything that needs the OS is a native command (see API_infra.md); everything persisted goes
 * through the native store commands (store_read/store_write/store_delete/store_list) as JSON files
 * under %LOCALAPPDATA%\WinKit (same file names / JSON shapes as the C# app).
 *
 * PUBLIC API  (all async functions return Promises)
 * -----------------------------------------------------------------------------------------------
 * Bridge / events
 *   WK.native(cmd, args?)            -> Promise<data>   post {id,cmd,...flat string args}; rejects Error(message) on {ok:false}
 *   WK.on(name, fn) -> unsubscribe   WK.off(name, fn)   WK.emit(name, data)
 *   WK.ready                         Promise resolved when init() finished ({onboardingCompleted})   WK.init()  (auto-run once)
 *   WK.version                       core.js API version string
 *   WK.mock                          object {cmd: (args)=>result} used when there is no chrome.webview (browser dev); default mocks for store_*
 * Persistence helper
 *   WK.store.read(name) -> obj|null  WK.store.write(name, obj)  WK.store.remove(name)  WK.store.list(dir) -> string[]
 * App / shell
 *   WK.app.info() -> {version,exePath,exeDir,dataDir,logsDir,themesDir,isAdmin,isInstalledLocation,userName,architecture,windowsVersion,windowsBuild,systemUsesLightTheme}
 *   WK.app.version (string "2.1.0")  WK.app.quit()
 *   WK.shell.openUrl(url)  WK.shell.openFolder(pathOr"@data"|"@logs"|"@themes")  WK.shell.reveal(path)  WK.shell.copy(text) -> bool
 *   WK.admin.isElevated() -> bool    WK.admin.restart() -> Promise<bool>   (confirm dialog + relaunch; shows error dialog on failure)
 *   WK.autostart.get() -> bool       WK.autostart.set(bool)
 * Settings (AppSettingsService + SettingsViewModel behaviour)
 *   WK.settings.get() -> {startMinimized,startWithWindows,notificationsEnabled,themeName,accentOverrideHex,autoCheckForUpdates,skippedUpdateVersion,lastUpdateCheckUtc}
 *   WK.settings.defaults()           WK.settings.set(patch) -> Promise (merge, emit 'settings:changed', save)
 *   WK.settings.setStartMinimized(b) WK.settings.setStartWithWindows(b) WK.settings.setNotificationsEnabled(b) WK.settings.setAutoCheckForUpdates(b)
 *   WK.settings.refreshFromSystem()  (call when the Settings screen opens: re-reads the real auto-start state)
 *   WK.settings.saveName(name) -> bool   (Settings "Save" button: ignores blank, saves profile, toast "Name updated")
 * Profile
 *   WK.profile.current() -> {name,hasCompletedOnboarding}   WK.profile.setName(name) (trims, persists, emits 'profile:changed')
 *   WK.onboarding.defaultName()  WK.onboarding.canContinue(name)  WK.onboarding.submit(name) -> bool
 * Activity log
 *   WK.activity.record(title, kind='Info', detail=null, category='Action')    kind: Info|Success|Warning|Error   category: Action|ThemeAndSettings
 *   WK.activity.entries() newest first (max 50)   WK.activity.filtered('all'|'actions'|'themesAndSettings')   WK.activity.formatTimestamp(entry)
 * Notifications / toasts (rendered by core.js into #wk-toasts; auto-mounted)
 *   WK.notify({title, description?, severity='Info', actionText?, action?:fn, autoDismissMs=3000 (null = stay)}) -> id
 *   WK.toast(title, description?, severity?, opts?) -> id        severity: Success|Error|Warning|Update|Info
 *   WK.notifications.items()  .dismiss(id)  .invoke(id)  .pause(id)  .resume(id)  .dismissAll()
 * Dialogs (modal, rendered by core.js)
 *   WK.confirm({title,message,confirmText='Confirm',cancelText='Cancel',isDestructive=false}) -> Promise<bool>
 *   WK.alert(title, message, technicalDetail?)  (also WK.alert({title,message,technicalDetail}))  -> Promise<void>
 *   WK.showErrorReport(title, message, report) -> Promise<bool sentOK>      WK.updateConfirm(version, releaseNotes) -> Promise<bool>
 * Themes
 *   WK.themes.builtIn() custom() all() names() ("System" first)   WK.themes.current()   WK.themes.find(name)
 *   WK.themes.selected()  (= settings.themeName)   WK.themes.select(name)  (apply + persist + log "Theme changed to X")
 *   WK.themes.apply(name)  WK.themes.preview(theme)  WK.themes.validate(theme) -> {ok,error}   WK.themes.isDark(theme)
 *   WK.themes.saveCustom(theme)  WK.themes.deleteCustom(name)  WK.themes.createDraftFrom(src,newName)  WK.themes.clone(theme)
 *   WK.themes.parse(objectOrJsonText) -> theme|null   WK.themes.serialize(theme) -> JSON text   WK.themes.importFromJson(text) -> theme|null
 *   WK.themes.importFlow() -> {status:'ok'|'cancelled'|'invalid',theme?}   WK.themes.exportFlow()   WK.themes.resetToDefault()   WK.themes.deleteFlow()
 *   WK.themes.applyToDom(theme)  WK.themes.hexToCss(hex) ("#AARRGGBB" -> "#RRGGBBAA")  WK.themes.sanitizeFileName(name)  WK.themes.COLOR_KEYS
 *   WK.themeEditor.openCreate()  .openEditCurrent()  .isOpen()  .draft()  .set(field, value)  .setColor(key, hex)  .save() -> {ok,error?}  .close()
 *       fields: name | density('Comfortable'|'Compact') | cornerRadius(0-24) | sidebarWidth(180-320) | fontScale(0.8-1.4) | animationIntensity('Full'|'Reduced'|'Off')
 *       colors: background surface surfaceElevated border textPrimary textSecondary textMuted accent accentHover onAccent success warning error update
 * Updates / About
 *   WK.updates.check() -> UpdateCheckResult   WK.updates.startAutoChecks()  WK.updates.stopAutoChecks()  (event 'update:review' {result})
 *   WK.about.getState() -> {version,statusText,isChecking,updateAvailable,isInstalling,progress}   WK.about.check()  WK.about.install()
 *   WK.about.skipThisVersion()  WK.about.showPendingUpdate(result)
 * Error reporting
 *   WK.errors.createReport(errorLike, component) -> report   WK.errors.send(report) -> {ok,errorMessage}   WK.errors.show(errorLike, component)
 *   WK.errors.logCrash(text)   (global window error / unhandledrejection handlers are installed automatically; WK.errors.autoHandle=false disables)
 * Formatting (match the C# converters)
 *   WK.fmt.uptime(seconds)  WK.fmt.uptimeMs(ms)  WK.fmt.bytes(n)  WK.fmt.megabytes(mb)  WK.fmt.timestamp(isoString)
 *
 * EVENTS (WK.on): 'ready', 'settings:changed', 'profile:changed', 'activity:changed', 'theme:changed' (theme), 'themes:listChanged',
 *   'themeEditor:changed', 'notifications:changed', 'about:changed', 'update:review' ({result}), plus every native PushEvent by its name
 *   (e.g. 'update_progress' {stage,received,total,percent}).
 * CSS custom properties set on :root by the theme: see API_infra.md ("Theme -> CSS variables").
 * ===================================================================================================== */
(function () {
  'use strict';

  const WK = { version: '1.0.0' };
  window.WK = WK;

  // ------------------------------------------------------------------------------------------------
  // tiny utilities
  // ------------------------------------------------------------------------------------------------
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const isStr = (v) => typeof v === 'string';
  const blank = (s) => s == null || String(s).trim() === '';
  const bg = (p) => { Promise.resolve(p).catch((e) => { try { console.warn('[WK]', e); } catch (_) { /* ignore */ } }); };
  const clone = (o) => JSON.parse(JSON.stringify(o));
  const errMsg = (e) => (e && e.message) ? e.message : String(e);
  const localIso = (d) => { // like DateTimeOffset.Now "O": 2026-10-09T21:44:00.1230000+03:00
    const p = (n, w = 2) => String(n).padStart(w, '0');
    const off = -d.getTimezoneOffset(), a = Math.abs(off);
    return d.getFullYear() + '-' + p(d.getMonth() + 1) + '-' + p(d.getDate()) + 'T' + p(d.getHours()) + ':' + p(d.getMinutes()) + ':' + p(d.getSeconds()) +
      '.' + p(d.getMilliseconds(), 3) + '0000' + (off < 0 ? '-' : '+') + p(Math.floor(a / 60)) + ':' + p(a % 60);
  };
  const ciGet = (obj, key) => { // case-insensitive property lookup (System.Text.Json PropertyNameCaseInsensitive)
    if (!obj || typeof obj !== 'object') return undefined;
    if (Object.prototype.hasOwnProperty.call(obj, key)) return obj[key];
    const lk = key.toLowerCase();
    for (const k of Object.keys(obj)) if (k.toLowerCase() === lk) return obj[k];
    return undefined;
  };

  // ------------------------------------------------------------------------------------------------
  // event bus
  // ------------------------------------------------------------------------------------------------
  const listeners = new Map();
  WK.on = function (name, fn) {
    if (!listeners.has(name)) listeners.set(name, new Set());
    listeners.get(name).add(fn);
    return () => WK.off(name, fn);
  };
  WK.off = function (name, fn) { const s = listeners.get(name); if (s) s.delete(fn); };
  WK.emit = function (name, data) {
    const s = listeners.get(name); if (!s) return;
    for (const fn of Array.from(s)) { try { fn(data); } catch (e) { try { console.error('[WK] listener for ' + name + ' threw', e); } catch (_) { /* ignore */ } } }
  };

  // ------------------------------------------------------------------------------------------------
  // native bridge
  // ------------------------------------------------------------------------------------------------
  let seq = 0;
  const pending = new Map();
  const hasHost = () => !!(window.chrome && window.chrome.webview && typeof window.chrome.webview.postMessage === 'function');
  WK.mock = {};

  function onHostMessage(ev) {
    let m = ev && ev.data;
    if (typeof m === 'string') { try { m = JSON.parse(m); } catch (_) { return; } }
    if (!m || typeof m !== 'object') return;
    if (m.event !== undefined && m.id === undefined) { WK.emit(String(m.event), m.data); return; }
    if (m.id === undefined) return;
    const key = String(m.id), p = pending.get(key);
    if (!p) return;
    pending.delete(key);
    if (m.ok === false || (m.ok === undefined && m.error !== undefined)) p.reject(new Error(m.error != null ? String(m.error) : 'Native call failed'));
    else p.resolve(m.data);
  }
  if (hasHost()) window.chrome.webview.addEventListener('message', onHostMessage);

  function flatArgs(args) {
    const o = {};
    if (!args) return o;
    for (const k of Object.keys(args)) {
      const v = args[k];
      if (v === undefined || v === null) continue;
      o[k] = isStr(v) ? v : (typeof v === 'object' ? JSON.stringify(v) : String(v));
    }
    return o;
  }
  WK.native = function (cmd, args) {
    return new Promise((resolve, reject) => {
      const flat = flatArgs(args);
      if (hasHost()) {          // note: `id` and `cmd` are reserved for the request envelope - never use them as argument names
        const id = String(++seq);
        pending.set(id, { resolve, reject });
        try { window.chrome.webview.postMessage(JSON.stringify(Object.assign({}, flat, { id: id, cmd: cmd }))); }
        catch (e) { pending.delete(id); reject(e); }
        return;
      }
      const h = WK.mock[cmd] || defaultMocks[cmd];
      if (h) { try { Promise.resolve(h(flat)).then(resolve, reject); } catch (e) { reject(e); } }
      else reject(new Error('Native bridge unavailable (' + cmd + ')'));
    });
  };

  // browser-dev mocks (only used when window.chrome.webview does not exist)
  const mockLS = (k) => 'wk-mock:' + k;
  const defaultMocks = {
    store_read: (a) => { try { const v = localStorage.getItem(mockLS(a.name)); return v == null ? null : JSON.parse(v); } catch (_) { return null; } },
    store_write: (a) => { localStorage.setItem(mockLS(a.name), a.json); return true; },
    store_delete: (a) => { localStorage.removeItem(mockLS(a.name)); return true; },
    store_list: (a) => {
      const pre = mockLS((a.dir ? a.dir + '/' : '')), out = [];
      for (let i = 0; i < localStorage.length; i++) { const k = localStorage.key(i); if (k.startsWith(pre) && !k.slice(pre.length).includes('/')) out.push(k.slice(pre.length)); }
      return out.sort();
    },
    app_info: () => ({ version: '2.1.0', exePath: 'C:\\WinKit\\WinKit.exe', exeDir: 'C:\\WinKit', dataDir: 'C:\\Users\\user\\AppData\\Local\\WinKit', logsDir: '', themesDir: '', isAdmin: false, isInstalledLocation: false, userName: 'user', architecture: 'X64', windowsVersion: 'Windows 11 Pro 24H2 (Build 26100)', windowsBuild: 26100, systemUsesLightTheme: false }),
    is_admin: () => false, system_uses_light_theme: () => false,
    autostart_get: () => localStorage.getItem(mockLS('@autostart')) === '1',
    autostart_set: (a) => { localStorage.setItem(mockLS('@autostart'), a.enabled === 'true' ? '1' : '0'); return true; },
    set_window_material: () => true, set_title_bar_colors: () => true, crash_log: () => true, app_quit: () => true,
    update_check: () => ({ status: 'UpToDate' }),
    clipboard_set: (a) => { try { navigator.clipboard.writeText(a.text); } catch (_) { /* ignore */ } return true; },
    open_url: (a) => { window.open(a.url, '_blank'); return true; },
    relaunch_elevated: () => false,
    error_report_create: (a) => { const id = 'WK-' + Math.random().toString(16).slice(2, 10).toUpperCase(); const ts = new Date().toISOString(); const r = { errorId: id, winKitVersion: '2.1.0', windowsVersion: 'Windows 11', architecture: 'X64', component: a.component || '', exceptionType: a.exceptionType || '', exceptionMessage: a.exceptionMessage || '', stackTrace: a.stackTrace || '(no stack trace available)', timestamp: ts }; r.text = 'Error ID: ' + id + '\r\nComponent: ' + r.component + '\r\nTimestamp: ' + ts + '\r\n\r\nWinKit Version: 2.1.0\r\nWindows Version: Windows 11\r\nArchitecture: X64\r\n\r\nException Type: ' + r.exceptionType + '\r\nException Message: ' + r.exceptionMessage + '\r\n\r\nStack Trace:\r\n' + r.stackTrace + '\r\n'; return r; },
    error_report_send: () => ({ ok: true, errorMessage: null }),
  };

  // ------------------------------------------------------------------------------------------------
  // store (JSON files in %LOCALAPPDATA%\WinKit) - writes to the same name are serialised
  // ------------------------------------------------------------------------------------------------
  const writeTails = new Map();
  WK.store = {
    read: (name) => WK.native('store_read', { name }),
    write: (name, obj) => {
      const json = JSON.stringify(obj, null, 2);
      const tail = writeTails.get(name) || Promise.resolve();
      const p = tail.catch(() => {}).then(() => WK.native('store_write', { name, json }));
      writeTails.set(name, p);
      return p;
    },
    remove: (name) => WK.native('store_delete', { name }),
    list: (dir) => WK.native('store_list', { dir: dir || '' }),
  };

  // ------------------------------------------------------------------------------------------------
  // app info / shell / admin / autostart
  // ------------------------------------------------------------------------------------------------
  let appInfo = null;
  WK.app = {
    version: '1.0.0',
    info: async function () { if (!appInfo) { appInfo = await WK.native('app_info'); if (appInfo && appInfo.version) WK.app.version = appInfo.version; } return appInfo; },
    quit: () => WK.native('app_quit'),
  };
  WK.shell = {
    openUrl: (url) => WK.native('open_url', { url }),
    openFolder: (path) => WK.native('open_folder', { path }),
    reveal: (path) => WK.native('reveal_in_explorer', { path }),
    copy: async function (text) {
      try { await navigator.clipboard.writeText(text); return true; } catch (_) { /* fall through */ }
      try { await WK.native('clipboard_set', { text }); return true; } catch (_) { return false; }
    },
  };
  let elevated = false;
  WK.admin = {
    isElevated: () => elevated,
    restart: async function () {
      const ok = await WK.confirm({ title: 'Restart as Administrator', message: 'Are you sure you want to restart the program via Admin?', confirmText: 'Restart as Administrator' });
      if (!ok) return false;
      let started = false;
      try { started = (await WK.native('relaunch_elevated', { arguments: '' })) === true; } catch (_) { started = false; }
      if (started) return true; // the host closes this instance
      await WK.alert("Couldn't restart as Administrator", 'The elevation request was cancelled or failed. WinKit is still running normally.');
      return false;
    },
  };
  WK.autostart = {
    get: async () => (await WK.native('autostart_get')) === true,
    set: (enabled) => WK.native('autostart_set', { enabled: !!enabled }),
  };

  // ------------------------------------------------------------------------------------------------
  // formatting helpers (UptimeFormatter, BytesToReadableConverter, NullableMegabytesConverter)
  // ------------------------------------------------------------------------------------------------
  const trim0 = (n) => { const s = n.toFixed(1); return s.endsWith('.0') ? s.slice(0, -2) : s; }; // .NET "0.#"
  WK.fmt = {
    uptime: function (seconds) {
      const t = Math.max(0, Math.floor(Number(seconds) || 0));
      const d = Math.floor(t / 86400), h = Math.floor((t % 86400) / 3600), m = Math.floor((t % 3600) / 60), s = t % 60;
      if (t >= 86400) return d + 'd ' + h + 'h ' + m + 'm ' + s + 's';
      if (h > 0) return h + 'h ' + m + 'm ' + s + 's';
      return m + 'm ' + s + 's';
    },
    uptimeMs: (ms) => WK.fmt.uptime((Number(ms) || 0) / 1000),
    bytes: function (value) {
      const units = ['B', 'KB', 'MB', 'GB', 'TB'];
      let size = Number(value); if (!Number.isFinite(size)) size = 0;
      let i = 0;
      while (size >= 1024 && i < units.length - 1) { size /= 1024; i++; }
      return trim0(size) + ' ' + units[i];
    },
    megabytes: function (mb) {
      if (typeof mb !== 'number' || !Number.isFinite(mb)) return '';
      return mb >= 1024 ? trim0(mb / 1024) + ' GB' : mb.toFixed(0) + ' MB';
    },
    timestamp: function (iso) { // approximates .NET "{0:g}" (short date + short time, current culture)
      const d = new Date(iso); if (isNaN(d.getTime())) return '';
      try { return d.toLocaleString([], { dateStyle: 'short', timeStyle: 'short' }); } catch (_) { return d.toLocaleString(); }
    },
  };

  // ------------------------------------------------------------------------------------------------
  // profile  (profile.json  {"Name": "...", "HasCompletedOnboarding": bool})
  // ------------------------------------------------------------------------------------------------
  let profile = { name: '' };
  WK.profile = {
    current: () => ({ name: profile.name, hasCompletedOnboarding: !blank(profile.name) }),
    load: async function () {
      let raw = null; try { raw = await WK.store.read('profile'); } catch (_) { raw = null; }
      const n = ciGet(raw, 'Name');
      profile = { name: isStr(n) ? n : '' };
      WK.emit('profile:changed', WK.profile.current());
    },
    setName: async function (name) {
      profile = { name: String(name == null ? '' : name).trim().slice(0, 40) };
      await WK.store.write('profile', { Name: profile.name, HasCompletedOnboarding: !blank(profile.name) });
      WK.emit('profile:changed', WK.profile.current());
    },
  };
  WK.onboarding = {
    defaultName: () => (appInfo && appInfo.userName) || '',
    canContinue: (name) => !blank(name),
    submit: async function (name) { if (blank(name)) return false; await WK.profile.setName(name); return true; },
  };

  // ------------------------------------------------------------------------------------------------
  // settings  (settings.json, PascalCase keys exactly like the C# SettingsData)
  // ------------------------------------------------------------------------------------------------
  const SETTINGS_DEFAULTS = Object.freeze({
    startMinimized: false, startWithWindows: false, notificationsEnabled: true, themeName: 'Dusk', accentOverrideHex: null,
    autoCheckForUpdates: true, skippedUpdateVersion: null, lastUpdateCheckUtc: null, paletteVersion: '2',
  });
  const SETTINGS_KEYS = { startMinimized: 'StartMinimized', startWithWindows: 'StartWithWindows', notificationsEnabled: 'NotificationsEnabled', themeName: 'ThemeName', accentOverrideHex: 'AccentOverrideHex', autoCheckForUpdates: 'AutoCheckForUpdates', skippedUpdateVersion: 'SkippedUpdateVersion', lastUpdateCheckUtc: 'LastUpdateCheckUtc', paletteVersion: 'PaletteVersion' };
  const SETTINGS_BOOL = new Set(['startMinimized', 'startWithWindows', 'notificationsEnabled', 'autoCheckForUpdates']);
  let settings = Object.assign({}, SETTINGS_DEFAULTS), migratedPalette = false;

  function parseSettings(raw) {
    const out = Object.assign({}, SETTINGS_DEFAULTS);
    if (!raw || typeof raw !== 'object' || Array.isArray(raw)) return out;
    const tmp = {};
    for (const k of Object.keys(SETTINGS_KEYS)) {
      const v = ciGet(raw, SETTINGS_KEYS[k]);
      if (v === undefined) continue;
      if (SETTINGS_BOOL.has(k)) { if (typeof v !== 'boolean') return Object.assign({}, SETTINGS_DEFAULTS); tmp[k] = v; }
      else { if (v !== null && !isStr(v)) return Object.assign({}, SETTINGS_DEFAULTS); tmp[k] = v; }
    }
    if (tmp.themeName === null) delete tmp.themeName;
    if (tmp.paletteVersion !== '2') { if (!tmp.themeName || tmp.themeName === 'Midnight') tmp.themeName = DEFAULT_THEME; tmp.paletteVersion = '2'; migratedPalette = true; }
    return Object.assign(out, tmp);
  }
  const settingsToJson = () => { const o = {}; for (const k of Object.keys(SETTINGS_KEYS)) o[SETTINGS_KEYS[k]] = settings[k]; return o; };
  const saveSettings = () => WK.store.write('settings', settingsToJson());

  WK.settings = {
    defaults: () => Object.assign({}, SETTINGS_DEFAULTS),
    get: () => Object.assign({}, settings),
    load: async function () {
      let raw = null; try { raw = await WK.store.read('settings'); } catch (_) { raw = null; }
      migratedPalette = false; settings = parseSettings(raw);
      if (raw && migratedPalette) { try { await saveSettings(); } catch (_) { /* keep working with the in-memory value */ } }     // remember the switch so Midnight can be chosen again later
      WK.emit('settings:changed', WK.settings.get());
    },
    set: async function (patch) {
      for (const k of Object.keys(patch || {})) if (k in SETTINGS_KEYS) settings[k] = patch[k];
      WK.emit('settings:changed', WK.settings.get());
      await saveSettings();
    },
    setStartMinimized: (v) => WK.settings.set({ startMinimized: !!v }),
    setNotificationsEnabled: (v) => WK.settings.set({ notificationsEnabled: !!v }),
    setAutoCheckForUpdates: (v) => WK.settings.set({ autoCheckForUpdates: !!v }),
    setStartWithWindows: async function (v) {
      v = !!v;
      try { await WK.autostart.set(v); }
      catch (e) {
        await WK.alert("Couldn't change the startup setting", 'WinKit could not update the Windows startup entry.', errMsg(e));
        try { v = await WK.autostart.get(); } catch (_) { v = settings.startWithWindows; }
      }
      await WK.settings.set({ startWithWindows: v });
      return v;
    },
    refreshFromSystem: async function () {
      let v = settings.startWithWindows;
      try { v = await WK.autostart.get(); } catch (_) { /* keep stored */ }
      if (v !== settings.startWithWindows) await WK.settings.set({ startWithWindows: v });
      return WK.settings.get();
    },
    saveName: async function (name) {
      if (blank(name)) return false;
      await WK.profile.setName(name);
      WK.notify({ title: 'Name updated', severity: 'Success' });
      return true;
    },
  };

  // ------------------------------------------------------------------------------------------------
  // activity log  (activity.json: [{Title,Detail,Kind(int),Category(int),Timestamp}], newest first, max 50)
  // ------------------------------------------------------------------------------------------------
  const MAX_ACTIVITY = 50;
  const KINDS = ['Info', 'Success', 'Warning', 'Error'], CATEGORIES = ['Action', 'ThemeAndSettings'];
  let activity = [];
  const enumFrom = (v, names, dflt) => {
    if (typeof v === 'number' && v >= 0 && v < names.length && Number.isInteger(v)) return names[v];
    if (isStr(v)) { const i = names.findIndex((n) => n.toLowerCase() === v.toLowerCase()); if (i >= 0) return names[i]; }
    return dflt;
  };
  WK.activity = {
    entries: () => activity.map((e) => Object.assign({}, e)),
    filtered: function (filter) {
      const f = String(filter || 'all').toLowerCase();
      if (f === 'actions') return WK.activity.entries().filter((e) => e.category === 'Action');
      if (f === 'themesandsettings') return WK.activity.entries().filter((e) => e.category === 'ThemeAndSettings');
      return WK.activity.entries();
    },
    formatTimestamp: (e) => WK.fmt.timestamp(e && e.timestamp),
    load: async function () {
      let raw = null; try { raw = await WK.store.read('activity'); } catch (_) { raw = null; }
      if (!Array.isArray(raw)) return;
      activity = [];
      for (const r of raw) {
        const title = ciGet(r, 'Title'); if (!isStr(title)) continue;
        const detail = ciGet(r, 'Detail'), ts = ciGet(r, 'Timestamp');
        activity.push({ title, detail: isStr(detail) ? detail : null, kind: enumFrom(ciGet(r, 'Kind'), KINDS, 'Info'), category: enumFrom(ciGet(r, 'Category'), CATEGORIES, 'Action'), timestamp: isStr(ts) && !isNaN(new Date(ts).getTime()) ? ts : localIso(new Date()) });
      }
      WK.emit('activity:changed', WK.activity.entries());
    },
    record: function (title, kind, detail, category) {
      activity.unshift({ title: String(title), detail: detail == null ? null : String(detail), kind: enumFrom(kind, KINDS, 'Info'), category: enumFrom(category, CATEGORIES, 'Action'), timestamp: localIso(new Date()) });
      while (activity.length > MAX_ACTIVITY) activity.pop();
      WK.emit('activity:changed', WK.activity.entries());
      const json = activity.map((e) => ({ Title: e.title, Detail: e.detail, Kind: KINDS.indexOf(e.kind), Category: CATEGORIES.indexOf(e.category), Timestamp: e.timestamp }));
      bg(WK.store.write('activity', json));
    },
  };

  // ------------------------------------------------------------------------------------------------
  // themes
  // ------------------------------------------------------------------------------------------------
  const COLOR_KEYS = ['background', 'surface', 'surfaceElevated', 'border', 'textPrimary', 'textSecondary', 'textMuted', 'accent', 'accentHover', 'onAccent', 'success', 'warning', 'error', 'update'];
  const DEFAULT_COLORS = Object.freeze({ background: '#161618', surface: '#1D1D20', surfaceElevated: '#242427', border: '#2B2B2F', textPrimary: '#DCDAD4', textSecondary: '#A29F97', textMuted: '#918E86', accent: '#8EA0B5', accentHover: '#A4B3C5', onAccent: '#161618', success: '#86B58F', warning: '#D4A85F', error: '#D98370', update: '#8EA0B5' });
  const DEFAULT_THEME = 'Dusk';
  const DENSITIES = ['Comfortable', 'Compact'], ANIMS = ['Full', 'Reduced', 'Off'];
  const HEX_RE = /^#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$/;
  const SYSTEM = 'System';

  // built-in themes (alphabetical by name, like the original Directory.EnumerateFiles + OrderBy)
  const BUILTIN_SOURCE = [
    { name: 'Carbon', isBuiltIn: true, density: 'Compact', cornerRadius: 4, sidebarWidth: 224, fontScale: 1, animationIntensity: 'Full',
      colors: { background: '#0C0C0D', surface: '#141415', surfaceElevated: '#1B1B1D', border: '#282829', textPrimary: '#F0F0F1', textSecondary: '#A0A0A4', textMuted: '#86868A', accent: '#8E9BF0', accentHover: '#A5B0F6', onAccent: '#0C0C0D', success: '#4CC38A', warning: '#E9B44C', error: '#EF6B6B', update: '#53B3D4' } },
    { name: 'Dawn', isBuiltIn: true, density: 'Comfortable', cornerRadius: 10, sidebarWidth: 232, fontScale: 1, animationIntensity: 'Full',
      colors: { background: '#F3F2EE', surface: '#FBFAF7', surfaceElevated: '#FFFFFF', border: '#E3E1DB', textPrimary: '#2B2A27', textSecondary: '#66635C', textMuted: '#6E6B64', accent: '#5B6F86', accentHover: '#4D6076', onAccent: '#FFFFFF', success: '#376D45', warning: '#845A0D', error: '#9E4836', update: '#5B6F86' } },
    { name: 'Dusk', isBuiltIn: true, density: 'Comfortable', cornerRadius: 10, sidebarWidth: 232, fontScale: 1, animationIntensity: 'Full',
      colors: { background: '#161618', surface: '#1D1D20', surfaceElevated: '#242427', border: '#2B2B2F', textPrimary: '#DCDAD4', textSecondary: '#A29F97', textMuted: '#918E86', accent: '#8EA0B5', accentHover: '#A4B3C5', onAccent: '#161618', success: '#86B58F', warning: '#D4A85F', error: '#D98370', update: '#8EA0B5' } },
    { name: 'Light', isBuiltIn: true, density: 'Comfortable', cornerRadius: 8, sidebarWidth: 232, fontScale: 1, animationIntensity: 'Full',
      colors: { background: '#F3F4F7', surface: '#FFFFFF', surfaceElevated: '#FFFFFF', border: '#DCE0E6', textPrimary: '#181A1F', textSecondary: '#535A66', textMuted: '#686D77', accent: '#2F6FE4', accentHover: '#4072CC', onAccent: '#FFFFFF', success: '#167342', warning: '#8A5C08', error: '#B33832', update: '#1D8AA6' } },
    { name: 'Midnight', isBuiltIn: true, density: 'Comfortable', cornerRadius: 8, sidebarWidth: 232, fontScale: 1, animationIntensity: 'Full',
      colors: { background: '#0F1115', surface: '#161920', surfaceElevated: '#1D2129', border: '#2A2F39', textPrimary: '#EDEFF3', textSecondary: '#A3A9B5', textMuted: '#848B97', accent: '#4C8DFF', accentHover: '#6AA0FF', onAccent: '#07101F', success: '#4CC38A', warning: '#E9B44C', error: '#EF6B6B', update: '#53B3D4' } },
    { name: 'Slate', isBuiltIn: true, density: 'Comfortable', cornerRadius: 10, sidebarWidth: 236, fontScale: 1, animationIntensity: 'Full',
      colors: { background: '#12151A', surface: '#181C22', surfaceElevated: '#1F242C', border: '#2B313B', textPrimary: '#F0F2F5', textSecondary: '#A6AEBB', textMuted: '#868E9A', accent: '#4FA8E0', accentHover: '#6DBBEB', onAccent: '#0B1620', success: '#4CC38A', warning: '#E9B44C', error: '#EF6E6E', update: '#53B3D4' } },
  ];

  const kebab = (k) => k.replace(/[A-Z]/g, (c) => '-' + c.toLowerCase());
  const nameEq = (a, b) => String(a).toLowerCase() === String(b).toLowerCase();
  const cloneTheme = (t) => ({ name: t.name, isBuiltIn: false, colors: Object.assign({}, t.colors), density: t.density, cornerRadius: t.cornerRadius, sidebarWidth: t.sidebarWidth, fontScale: t.fontScale, animationIntensity: t.animationIntensity });
  const fallbackTheme = () => ({ name: DEFAULT_THEME, isBuiltIn: true, colors: Object.assign({}, DEFAULT_COLORS), density: 'Comfortable', cornerRadius: 10, sidebarWidth: 232, fontScale: 1.0, animationIntensity: 'Full' });

  let builtIn = [], custom = [], current = fallbackTheme();

  function sanitizeFileName(name) {
    // Path.GetInvalidFileNameChars() on Windows (+ Win32 silently drops trailing dots/spaces)
    return String(name).replace(/[\u0000-\u001f"<>|:*?\\/]/g, '').replace(/[. ]+$/, '');
  }
  function fileNameProblem(name) {
    const s = sanitizeFileName(name);
    if (!s) return 'Theme name must contain at least one letter or number.';
    if (s.length > 100) return 'Theme name is too long (100 characters max).';
    if (/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(\..*)?$/i.test(s)) return "That theme name isn't allowed by Windows.";
    return null;
  }
  const themeFile = (name) => 'Themes/' + sanitizeFileName(name);

  function validateTheme(theme) {
    if (!theme || blank(theme.name)) return { ok: false, error: 'Theme name cannot be empty.' };
    const c = theme.colors;
    if (!c || COLOR_KEYS.some((k) => !isStr(c[k]) || blank(c[k]) || !HEX_RE.test(c[k]))) return { ok: false, error: 'All theme colors must be valid hex codes (e.g. #4C8DFF).' };
    const num = (v) => typeof v === 'number' && Number.isFinite(v);
    if (!num(theme.cornerRadius) || theme.cornerRadius < 0 || theme.cornerRadius > 24) return { ok: false, error: 'Corner radius must be between 0 and 24.' };
    if (!num(theme.sidebarWidth) || theme.sidebarWidth < 180 || theme.sidebarWidth > 320) return { ok: false, error: 'Sidebar width must be between 180 and 320.' };
    if (!num(theme.fontScale) || theme.fontScale < 0.8 || theme.fontScale > 1.4) return { ok: false, error: 'Font scale must be between 0.8 and 1.4.' };
    const fp = fileNameProblem(theme.name);
    if (fp) return { ok: false, error: fp };
    return { ok: true, error: null };
  }

  // JSON -> theme, mirroring JsonSerializer.Deserialize<ThemeDefinition> (case-insensitive names, string enums, missing = default,
  // wrong type = failure). Returns null when the document can't be deserialised.
  function parseTheme(input) {
    let o = input;
    if (isStr(o)) { try { o = JSON.parse(o); } catch (_) { return null; } }
    if (!o || typeof o !== 'object' || Array.isArray(o)) return null;
    const t = fallbackTheme(); t.isBuiltIn = false;
    const name = ciGet(o, 'name');
    if (name === undefined) return null;                     // `required`
    if (name !== null && !isStr(name)) return null;
    t.name = name;
    const ib = ciGet(o, 'isBuiltIn'); if (ib !== undefined) { if (typeof ib !== 'boolean') return null; t.isBuiltIn = ib; }
    const cols = ciGet(o, 'colors');
    if (cols !== undefined) {
      if (cols === null || typeof cols !== 'object' || Array.isArray(cols)) return null;
      for (const k of COLOR_KEYS) { const v = ciGet(cols, k); if (v === undefined) continue; if (v !== null && !isStr(v)) return null; t.colors[k] = v; }
    }
    const en = (key, names, dflt) => {
      const v = ciGet(o, key); if (v === undefined) return dflt;
      if (typeof v === 'number' && Number.isInteger(v) && v >= 0 && v < names.length) return names[v];
      if (isStr(v)) { const i = names.findIndex((n) => n.toLowerCase() === v.toLowerCase()); if (i >= 0) return names[i]; }
      return undefined;
    };
    const d = en('density', DENSITIES, 'Comfortable'); if (d === undefined) return null; t.density = d;
    const a = en('animationIntensity', ANIMS, 'Full'); if (a === undefined) return null; t.animationIntensity = a;
    for (const [key, field] of [['cornerRadius', 'cornerRadius'], ['sidebarWidth', 'sidebarWidth'], ['fontScale', 'fontScale']]) {
      const v = ciGet(o, key); if (v === undefined) continue; if (typeof v !== 'number') return null; t[field] = v;
    }
    return t;
  }
  function serializeTheme(t) {
    const colors = {}; for (const k of COLOR_KEYS) colors[k] = t.colors[k];
    return JSON.stringify({ name: t.name, isBuiltIn: !!t.isBuiltIn, colors, density: t.density, cornerRadius: t.cornerRadius, sidebarWidth: t.sidebarWidth, fontScale: t.fontScale, animationIntensity: t.animationIntensity }, null, 2);
  }

  function hexToCss(hex) { // WPF "#AARRGGBB" -> CSS "#RRGGBBAA"
    if (!isStr(hex) || !HEX_RE.test(hex)) return null;
    return hex.length === 9 ? '#' + hex.slice(3) + hex.slice(1, 3) : hex;
  }
  function hexRgb(hex) {
    const h = hexToCss(hex); if (!h) return null;
    return [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];
  }
  function isDarkTheme(t) { // MainWindow.IsDarkBackground
    const rgb = hexRgb(t && t.colors && t.colors.background);
    if (!rgb) return true;
    return (0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]) / 255 < 0.5;
  }

  let windowTimer = null;
  function pushWindowStyling(theme) {
    clearTimeout(windowTimer);
    windowTimer = setTimeout(() => {
      const dark = isDarkTheme(theme), hex = (v) => (hexToCss(v) || '').slice(0, 7);
      bg(WK.native('set_window_material', { mode: dark ? 'dark' : 'light' }));
      bg(WK.native('set_title_bar_colors', { dark, caption: hex(theme.colors.background), text: hex(theme.colors.textPrimary), border: hex(theme.colors.border) }));
    }, 40);
  }

  let animIntensity = 'Full';
  function applyToDom(theme) { // ThemeApplier.Apply
    const root = document.documentElement, st = root.style, c = theme.colors;
    for (const k of COLOR_KEYS) {
      const css = hexToCss(c[k]); if (!css) continue;
      st.setProperty('--wk-' + kebab(k), css);
      const rgb = hexRgb(c[k]); if (rgb) st.setProperty('--wk-' + kebab(k) + '-rgb', rgb.join(', '));
    }
    const fin = (v, d) => (typeof v === 'number' && Number.isFinite(v) ? v : d);
    const radius = fin(theme.cornerRadius, 8);
    st.setProperty('--wk-radius', radius + 'px');
    st.setProperty('--wk-radius-sm', Math.min(radius, 12) + 'px');
    st.setProperty('--wk-sidebar-width', fin(theme.sidebarWidth, 232) + 'px');
    const densityScale = theme.density === 'Compact' ? 0.85 : 1.0, fs = fin(theme.fontScale, 1) * densityScale;
    const px = (n) => (Math.round(n * fs * 100) / 100) + 'px';
    st.setProperty('--wk-font-scale', String(fs));
    st.setProperty('--wk-font-size-small', px(11.5));
    st.setProperty('--wk-font-size-body', px(13));
    st.setProperty('--wk-font-size-title', px(15));
    st.setProperty('--wk-font-size-heading', px(21));
    st.setProperty('--wk-font-size-display', px(28));
    const [fast, normal] = theme.animationIntensity === 'Off' ? [0, 0] : theme.animationIntensity === 'Reduced' ? [60, 80] : [120, 160];
    st.setProperty('--wk-anim-fast', fast + 'ms');
    st.setProperty('--wk-anim-normal', normal + 'ms');
    root.dataset.wkDensity = theme.density === 'Compact' ? 'compact' : 'comfortable';
    root.dataset.wkAnimations = String(theme.animationIntensity || 'Full').toLowerCase();
    root.dataset.wkTheme = theme.name;
    const dark = isDarkTheme(theme);
    const lum = (h) => { const r = hexRgb(h); if (!r) return 0; const l = (v) => { v /= 255; return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); }; return 0.2126 * l(r[0]) + 0.7152 * l(r[1]) + 0.0722 * l(r[2]); };
    const errL = lum(c.error);                            // text on a solid error-coloured button: whichever of white / near-black reads better
    st.setProperty('--wk-on-error', 1.05 / (errL + 0.05) >= (errL + 0.05) / 0.0579 ? '#FFFFFF' : '#161618');       // 0.0579 = luminance of #161618 + 0.05
    st.colorScheme = dark ? 'dark' : 'light';
    st.setProperty('--wk-shadow-md', dark ? '0 10px 30px rgba(0,0,0,.42)' : '0 10px 30px rgba(16,24,40,.16)');
    animIntensity = theme.animationIntensity || 'Full';
    pushWindowStyling(theme);
  }

  function setCurrent(theme) { current = theme; applyToDom(theme); WK.emit('theme:changed', theme); }
  const findByName = (name) => builtIn.find((t) => nameEq(t.name, name)) || custom.find((t) => nameEq(t.name, name)) || null;
  async function resolveSystemThemeName() {
    try { return (await WK.native('system_uses_light_theme')) === true ? 'Dawn' : DEFAULT_THEME; }
    catch (_) { try { return window.matchMedia('(prefers-color-scheme: light)').matches ? 'Dawn' : DEFAULT_THEME; } catch (__) { return DEFAULT_THEME; } }
  }
  const themeNames = () => [SYSTEM].concat(builtIn.map((t) => t.name), custom.map((t) => t.name));
  const findDefinition = (name) => findByName(name) || (name === SYSTEM ? current : null); // SettingsViewModel.FindThemeDefinition
  function generateNewThemeName(baseName) {
    let cand = baseName + ' Copy', n = 2;
    const existing = new Set(custom.map((t) => t.name.toLowerCase()));
    while (existing.has(cand.toLowerCase())) cand = baseName + ' Copy ' + (n++);
    return cand;
  }

  const reservedName = (name) => nameEq(String(name).trim(), SYSTEM) || builtIn.some((t) => nameEq(t.name, String(name).trim()));
  async function saveCustomTheme(theme) {
    const v = validateTheme(theme);
    if (!v.ok) throw new Error(v.error || 'Invalid theme.');
    if (custom.some((t) => !nameEq(t.name, theme.name) && sanitizeFileName(t.name).toLowerCase() === sanitizeFileName(theme.name).toLowerCase()))
      throw new Error('A theme with a nearly identical name already exists ("' + custom.find((t) => sanitizeFileName(t.name).toLowerCase() === sanitizeFileName(theme.name).toLowerCase()).name + '"). Choose a more different name.');
    if (reservedName(theme.name)) throw new Error('"' + String(theme.name).trim() + '" is the name of a built-in theme. Choose a different name.');
    theme.isBuiltIn = false;
    await WK.store.write(themeFile(theme.name), JSON.parse(serializeTheme(theme)));
    const i = custom.findIndex((t) => nameEq(t.name, theme.name));
    if (i >= 0) custom[i] = theme; else custom.push(theme);
    if (nameEq(current.name, theme.name)) setCurrent(theme);
    WK.emit('themes:listChanged', themeNames());
  }
  async function deleteCustomTheme(name) {
    if (!fileNameProblem(name)) await WK.store.remove(themeFile(name));
    custom = custom.filter((t) => !nameEq(t.name, name));
    WK.emit('themes:listChanged', themeNames());
  }

  async function applyThemeSelection(name) { // SettingsViewModel.ApplyThemeSelectionAsync
    await WK.themes.apply(name);
    try { await WK.settings.set({ themeName: name }); } catch (e) { await WK.alert("Couldn't save your theme choice", 'The theme is applied for now, but WinKit could not write its settings file, so it will not be remembered.', errMsg(e)); }
    WK.activity.record('Theme changed to ' + name, 'Info', null, 'ThemeAndSettings');
  }

  WK.themes = {
    COLOR_KEYS: COLOR_KEYS.slice(),
    SYSTEM_NAME: SYSTEM,
    builtIn: () => builtIn.map((t) => clone(t)),
    custom: () => custom.map((t) => clone(t)),
    all: () => builtIn.concat(custom).map((t) => clone(t)),
    names: themeNames,
    current: () => clone(current),
    find: (name) => { const t = findDefinition(name); return t ? clone(t) : null; },
    selected: () => settings.themeName,
    select: (name) => applyThemeSelection(name),
    apply: async function (name) {
      const resolved = name === SYSTEM ? await resolveSystemThemeName() : name;
      setCurrent(findByName(resolved) || findByName(DEFAULT_THEME) || builtIn[0] || fallbackTheme());
      return clone(current);
    },
    preview: (theme) => setCurrent(theme),
    validate: validateTheme,
    isDark: isDarkTheme,
    saveCustom: saveCustomTheme,
    deleteCustom: deleteCustomTheme,
    clone: cloneTheme,
    createDraftFrom: (source, newName) => { const d = cloneTheme(source); d.name = newName; return d; },
    parse: parseTheme,
    serialize: serializeTheme,
    importFromJson: async function (text) { // ThemeService.ImportThemeAsync, minus the file read
      const t = parseTheme(text);
      if (!t || !validateTheme(t).ok) return null;
      t.isBuiltIn = false;
      if (reservedName(t.name)) t.name = String(t.name).trim() + ' (imported)';
      try { await saveCustomTheme(t); } catch (_) { return null; }
      return clone(t);
    },
    applyToDom, hexToCss, sanitizeFileName, generateNewThemeName,
    animationIntensity: () => animIntensity,
    init: async function () {
      builtIn = BUILTIN_SOURCE.map((t) => { const c = clone(t); c.isBuiltIn = true; return c; });
      if (!builtIn.length) builtIn.push(fallbackTheme());
      custom = [];
      let names = []; try { names = await WK.store.list('Themes'); } catch (_) { names = []; }
      for (const n of names || []) {
        let raw = null; try { raw = await WK.store.read('Themes/' + n); } catch (_) { raw = null; }
        const t = parseTheme(raw);
        if (t && validateTheme(t).ok) { t.isBuiltIn = false; custom.push(t); }
      }
      WK.emit('themes:listChanged', themeNames());
    },
    // ---- Settings screen flows (SettingsViewModel) ----
    importFlow: async function () {
      let res; try { res = await WK.native('import_file_dialog', { title: 'Import theme', filter: 'WinKit theme (*.json)|*.json' }); } catch (e) { res = { text: '' }; }
      if (!res) return { status: 'cancelled' };
      const imported = await WK.themes.importFromJson(res.text);
      if (!imported) { await WK.alert('Import failed', "That file isn't a valid WinKit theme."); return { status: 'invalid' }; }
      await applyThemeSelection(imported.name);
      WK.notify({ title: 'Theme imported', description: imported.name, severity: 'Success' });
      return { status: 'ok', theme: imported };
    },
    exportFlow: async function () {
      const theme = findDefinition(settings.themeName); if (!theme) return { status: 'cancelled' };
      let path = null;
      try { path = await WK.native('export_file_dialog', { title: 'Export theme', filter: 'WinKit theme (*.json)|*.json', defaultName: theme.name + '.json', defaultExt: 'json', text: serializeTheme(theme) }); }
      catch (e) { await WK.alert('Export failed', "The theme couldn't be exported.", errMsg(e)); return { status: 'error' }; }
      if (!path) return { status: 'cancelled' };
      WK.notify({ title: 'Theme exported', description: path, severity: 'Success' });
      return { status: 'ok', path };
    },
    resetToDefault: async function () { await applyThemeSelection(DEFAULT_THEME); },
    deleteFlow: async function () {
      const theme = findDefinition(settings.themeName);
      if (!theme || theme.isBuiltIn) return false;
      const ok = await WK.confirm({ title: 'Delete theme', message: 'Delete "' + theme.name + '"? This can\'t be undone.', confirmText: 'Delete', isDestructive: true });
      if (!ok) return false;
      await deleteCustomTheme(theme.name);
      await WK.themes.resetToDefault();
      return true;
    },
  };

  // ---- live theme editor (ThemeDraftViewModel + editor part of SettingsViewModel) ----
  let draft = null, editingOriginalName = null;
  const editorChanged = () => WK.emit('themeEditor:changed', draft ? clone(draft) : null);
  function openEditor(d) { draft = d; editorChanged(); setCurrent(cloneTheme(draft)); }
  WK.themeEditor = {
    isOpen: () => draft !== null,
    draft: () => (draft ? clone(draft) : null),
    isBuiltInSource: () => false,
    openCreate: function () {
      const base = findDefinition(settings.themeName) || builtIn[0];
      editingOriginalName = null;
      openEditor(WK.themes.createDraftFrom(base, generateNewThemeName(base.name)));
    },
    openEditCurrent: function () {
      const t = findDefinition(settings.themeName); if (!t) return;
      editingOriginalName = t.isBuiltIn ? null : t.name;
      openEditor(t.isBuiltIn ? WK.themes.createDraftFrom(t, generateNewThemeName(t.name)) : cloneTheme(t));
    },
    set: function (field, value) {
      if (!draft) return;
      if (field.startsWith('colors.')) field = field.slice(7);
      if (COLOR_KEYS.includes(field)) return WK.themeEditor.setColor(field, value);
      if (field === 'name') draft.name = String(value);
      else if (field === 'density') draft.density = DENSITIES.includes(value) ? value : draft.density;
      else if (field === 'animationIntensity') draft.animationIntensity = ANIMS.includes(value) ? value : draft.animationIntensity;
      else if (field === 'cornerRadius' || field === 'sidebarWidth' || field === 'fontScale') draft[field] = Number(value);
      else return;
      notifyDraft();
    },
    setColor: function (key, hex) { if (!draft || !COLOR_KEYS.includes(key)) return; draft.colors[key] = String(hex); notifyDraft(); },
    save: async function () {
      if (!draft) return { ok: false, error: 'No theme is being edited.' };
      const def = cloneTheme(draft), v = validateTheme(def);
      if (!v.ok) { await WK.alert('Invalid theme', v.error || 'This theme could not be saved.'); return { ok: false, error: v.error }; }
      try {
        await saveCustomTheme(def);
        // rename: remove the old file (compare by file name - a case-only rename maps to the same file and must not delete it)
        if (editingOriginalName !== null && sanitizeFileName(editingOriginalName).toLowerCase() !== sanitizeFileName(def.name).toLowerCase()) await deleteCustomTheme(editingOriginalName);
        setCurrent(def);
        await WK.settings.set({ themeName: def.name });
      } catch (e) {
        await WK.alert('Invalid theme', errMsg(e));
        return { ok: false, error: errMsg(e) };
      }
      draft = null; editingOriginalName = null; editorChanged();
      WK.activity.record('Theme "' + def.name + '" saved', 'Success', null, 'ThemeAndSettings');
      WK.notify({ title: 'Theme saved', description: def.name, severity: 'Success' });
      return { ok: true };
    },
    close: async function () { draft = null; editingOriginalName = null; editorChanged(); await WK.themes.apply(settings.themeName); },
  };
  function notifyDraft() { // ThemeDraftViewModel.Notify: only live-preview when every colour is well-formed
    editorChanged();
    if (draft && COLOR_KEYS.every((k) => isStr(draft.colors[k]) && HEX_RE.test(draft.colors[k]))) setCurrent(cloneTheme(draft));
  }

  // ------------------------------------------------------------------------------------------------
  // injected CSS (dialogs + toasts + animation-off switch)
  // ------------------------------------------------------------------------------------------------
  const CSS = `
:root[data-wk-animations="off"] *, :root[data-wk-animations="off"] *::before, :root[data-wk-animations="off"] *::after { animation: none !important; transition: none !important; }
.wk-overlay { position: fixed; inset: 0; z-index: 10000; display: flex; align-items: center; justify-content: center; background: rgba(0,0,0,.5); backdrop-filter: blur(3px); padding: 16px; font-family: inherit; }
.wk-dialog { box-sizing: border-box; width: 420px; max-width: 100%; max-height: calc(100vh - 32px); overflow: auto; padding: 24px; background: var(--wk-surface-elevated, #1A1A1D); color: var(--wk-text-primary, #F5F5F7); border: 1px solid var(--wk-border, #2A2F39); border-radius: calc(var(--wk-radius, 8px) + 2px); box-shadow: var(--wk-shadow-md, 0 10px 32px rgba(0,0,0,.4)); font-size: var(--wk-font-size-body, 13px); }
.wk-dialog-title { margin: 0; font-size: calc(var(--wk-font-size-title, 15px) + 3px); font-weight: 600; letter-spacing: -.01em; }
.wk-dialog-message { margin: 10px 0 0; color: var(--wk-text-secondary, #A1A1A8); line-height: 1.45; white-space: pre-wrap; }
.wk-dialog-errid { margin: 8px 0 0; color: var(--wk-text-muted, #6F6F76); font-family: "Cascadia Mono", Consolas, monospace; }
.wk-dialog-details { margin-top: 14px; color: var(--wk-text-muted, #6F6F76); }
.wk-dialog-details > summary { cursor: pointer; user-select: none; }
.wk-dialog-detailsbox { margin-top: 8px; padding: 10px; background: var(--wk-surface, #151517); border: 1px solid var(--wk-border, #252529); border-radius: var(--wk-radius-sm, 6px); }
.wk-dialog-detailsbox pre { margin: 0; max-height: 220px; overflow: auto; white-space: pre-wrap; word-break: break-word; font-family: "Cascadia Mono", Consolas, monospace; font-size: var(--wk-font-size-small, 11.5px); color: var(--wk-text-muted, #6F6F76); }
.wk-dialog-status { margin-top: 14px; color: var(--wk-text-secondary, #A1A1A8); }
.wk-dialog-status[data-kind="success"] { color: var(--wk-success, #4ADE80); }
.wk-dialog-status[data-kind="error"] { color: var(--wk-error, #F87171); }
.wk-dialog-buttons { display: flex; justify-content: flex-end; gap: 10px; margin-top: 22px; }
.wk-btn { font: inherit; cursor: pointer; padding: 8px 16px; border-radius: var(--wk-radius-sm, 6px); border: 1px solid var(--wk-border, #252529); background: var(--wk-surface, #151517); color: var(--wk-text-primary, #F5F5F7); }
.wk-btn:hover:not(:disabled) { border-color: var(--wk-accent, #4C8DFF); }
.wk-btn:disabled { opacity: .5; cursor: default; }
.wk-btn:focus-visible { outline: 2px solid var(--wk-accent, #4C8DFF); outline-offset: 2px; }
.wk-btn-primary { background: var(--wk-accent, #4C8DFF); border-color: var(--wk-accent, #4C8DFF); color: var(--wk-on-accent, #fff); }
.wk-btn-primary:hover:not(:disabled) { background: var(--wk-accent-hover, #669DFF); border-color: var(--wk-accent-hover, #669DFF); }
.wk-btn-destructive { background: var(--wk-error, #EF6B6B); border-color: var(--wk-error, #EF6B6B); color: var(--wk-on-error, #161618); }
.wk-btn-destructive:hover:not(:disabled) { filter: brightness(1.08); border-color: var(--wk-error, #EF6B6B); }
.wk-btn-small { padding: 4px 10px; font-size: var(--wk-font-size-small, 11.5px); margin-top: 8px; }
.wk-dialog { animation: wk-dlg-in .16s ease-out; }
@keyframes wk-dlg-in { from { opacity: 0; transform: translateY(6px) scale(.985); } to { opacity: 1; transform: none; } }
:root[data-wk-animations="off"] .wk-dialog { animation: none; }
.wk-hidden { display: none !important; }
#wk-toasts { position: fixed; right: 20px; bottom: 20px; z-index: 9000; display: flex; flex-direction: column; align-items: flex-end; pointer-events: none; font-family: inherit; }
.wk-toast { pointer-events: auto; box-sizing: border-box; width: 340px; max-width: calc(100vw - 40px); margin-top: 10px; padding: 12px 14px; display: flex; gap: 10px; background: var(--wk-surface-elevated, #1A1A1D); color: var(--wk-text-primary, #F5F5F7); border: 1px solid var(--wk-border, #2A2F39); border-radius: var(--wk-radius, 8px); box-shadow: var(--wk-shadow-md, 0 6px 20px rgba(0,0,0,.3)); opacity: 0; font-size: var(--wk-font-size-body, 13px); }
.wk-toast { transform: translateX(14px); }
.wk-toast[data-anim="on"] { transition: opacity .18s ease, transform .18s ease; }
.wk-toast.wk-in { opacity: 1; transform: none; }
.wk-toast.wk-leaving { opacity: 0; }
.wk-toast-bar { flex: none; width: 3px; border-radius: 1.5px; background: var(--wk-text-muted, #6F6F76); }
.wk-toast[data-severity="Success"] .wk-toast-bar { background: var(--wk-success, #4ADE80); }
.wk-toast[data-severity="Error"] .wk-toast-bar { background: var(--wk-error, #F87171); }
.wk-toast[data-severity="Warning"] .wk-toast-bar { background: var(--wk-warning, #FBBF24); }
.wk-toast[data-severity="Update"] .wk-toast-bar { background: var(--wk-update, #53B3D4); }
.wk-toast-body { flex: 1; min-width: 0; }
.wk-toast-title { font-size: var(--wk-font-size-title, 15px); font-weight: 600; word-break: break-word; }
.wk-toast-desc { margin-top: 4px; color: var(--wk-text-secondary, #A1A1A8); word-break: break-word; }
.wk-toast-close { flex: none; align-self: flex-start; width: 22px; height: 22px; margin: -4px -6px 0 0; padding: 0; border: 0; border-radius: var(--wk-radius-sm, 6px); background: transparent; color: var(--wk-text-secondary, #A1A1A8); cursor: pointer; font-size: 14px; line-height: 22px; }
.wk-toast-close:hover { background: var(--wk-border, #252529); }
`;
  function injectCss() {
    if (document.getElementById('wk-core-css')) return;
    const s = document.createElement('style'); s.id = 'wk-core-css'; s.textContent = CSS;
    (document.head || document.documentElement).appendChild(s);
  }

  // ------------------------------------------------------------------------------------------------
  // dialogs
  // ------------------------------------------------------------------------------------------------
  const el = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; };
  let dlgTail = Promise.resolve();
  const enqueue = (fn) => { const p = dlgTail.then(fn); dlgTail = p.catch(() => {}); return p; };

  // o: {title, message, errorId?, details?:{header,text,copyLabel,open}, preview?:bool, cancel?:{label}|null, confirm:{label,kind}, onConfirm?(ctx)}
  function openDialog(o) {
    injectCss();
    return new Promise((resolve) => {
      const prevFocus = document.activeElement;
      const ov = el('div', 'wk-overlay'), dlg = el('div', 'wk-dialog');
      dlg.setAttribute('role', 'dialog'); dlg.setAttribute('aria-modal', 'true');
      const title = el('h2', 'wk-dialog-title', o.title), msg = el('p', 'wk-dialog-message', o.message);
      const uid = 'wk-dlg-' + (++seq); title.id = uid + '-t'; msg.id = uid + '-m';
      dlg.setAttribute('aria-labelledby', title.id); dlg.setAttribute('aria-describedby', msg.id);
      dlg.append(title, msg);
      if (o.extra) dlg.append(o.extra);
      if (o.errorId) dlg.append(el('div', 'wk-dialog-errid', 'Error ID: ' + o.errorId));
      let details = null;
      if (o.details) {
        details = el('details', 'wk-dialog-details');
        const sum = el('summary', null, o.details.header), box = el('div', 'wk-dialog-detailsbox'), pre = el('pre', null, o.details.text);
        const copy = el('button', 'wk-btn wk-btn-small', o.details.copyLabel); copy.type = 'button';
        copy.addEventListener('click', () => bg(WK.shell.copy(o.details.text)));
        box.append(pre, copy); details.append(sum, box); details.open = !!o.details.open; dlg.append(details);
      }
      const status = el('div', 'wk-dialog-status wk-hidden'); dlg.append(status);
      const row = el('div', 'wk-dialog-buttons');
      const mk = (label, cls) => { const b = el('button', 'wk-btn ' + (cls || ''), label); b.type = 'button'; return b; };
      const bPreview = mk('Preview'), bCancel = mk(o.cancel ? o.cancel.label : 'Cancel'), bOk = mk(o.confirm.label, o.confirm.kind === 'destructive' ? 'wk-btn-destructive' : 'wk-btn-primary');
      if (!o.preview) bPreview.classList.add('wk-hidden');
      if (!o.cancel) bCancel.classList.add('wk-hidden');
      row.append(bPreview, bCancel, bOk); dlg.append(row); ov.append(dlg);

      const ctx = {
        details, bPreview, bCancel, bOk, result: false,
        status: (text, kind) => { status.textContent = text; status.dataset.kind = kind || ''; status.classList.toggle('wk-hidden', !text); },
        close: (v) => {
          document.removeEventListener('keydown', onKey, true); ov.remove();
          try { if (prevFocus && prevFocus.focus) prevFocus.focus(); } catch (_) { /* ignore */ }
          resolve(v);
        },
      };
      function onKey(e) {
        if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); (o.cancel && !bCancel.classList.contains('wk-hidden') ? bCancel : bOk).click(); }
        else if (e.key === 'Enter') {
          const t = e.target;
          if (t && (t.tagName === 'BUTTON' || t.tagName === 'SUMMARY' || t.tagName === 'TEXTAREA')) return;
          e.preventDefault(); if (!bOk.disabled) bOk.click();
        } else if (e.key === 'Tab') {
          const f = Array.from(dlg.querySelectorAll('button:not(:disabled):not(.wk-hidden), summary')).filter((x) => x.offsetParent !== null);
          if (!f.length) return;
          const i = f.indexOf(document.activeElement);
          if (e.shiftKey && i <= 0) { e.preventDefault(); f[f.length - 1].focus(); }
          else if (!e.shiftKey && (i === -1 || i === f.length - 1)) { e.preventDefault(); f[0].focus(); }
        }
      }
      bCancel.addEventListener('click', () => ctx.close(o.onCancelResult !== undefined ? ctx.result : false));
      bPreview.addEventListener('click', () => { if (details) details.open = !details.open; });
      bOk.addEventListener('click', () => { if (o.onConfirm) bg(o.onConfirm(ctx)); else ctx.close(true); });
      document.addEventListener('keydown', onKey, true);
      document.body.appendChild(ov);
      (o.confirm.kind === 'destructive' && o.cancel ? bCancel : bOk).focus();
    });
  }

  WK.dialog = (o) => enqueue(() => openDialog(o));            // o: see openDialog; extra = a DOM node shown under the message
  WK.confirm = (req) => enqueue(() => openDialog({
    title: req.title, message: req.message,
    cancel: { label: req.cancelText || 'Cancel' },
    confirm: { label: req.confirmText || 'Confirm', kind: req.isDestructive ? 'destructive' : 'primary' },
  }));
  WK.alert = (title, message, technicalDetail) => {
    if (title && typeof title === 'object') { technicalDetail = title.technicalDetail; message = title.message; title = title.title; }
    return enqueue(() => openDialog({
      title: String(title), message: String(message == null ? '' : message), cancel: null, confirm: { label: 'OK', kind: 'primary' },
      details: blank(technicalDetail) ? null : { header: 'Technical details', text: String(technicalDetail), copyLabel: 'Copy details', open: false },
    })).then(() => undefined);
  };
  WK.updateConfirm = (version, releaseNotes) => enqueue(() => openDialog({
    title: 'WinKit ' + version + ' is available',
    message: "Review what's changed, then choose whether to install it now. Your settings, themes, and activity history are preserved.",
    cancel: { label: 'Cancel' }, confirm: { label: 'Install Update', kind: 'primary' },
    details: blank(releaseNotes) ? null : { header: "What's new", text: String(releaseNotes), copyLabel: 'Copy changelog', open: true },
  }));
  WK.showErrorReport = (title, message, report) => enqueue(() => {
    let sent = false;
    return openDialog({
      title, message, errorId: report.errorId, preview: true,
      details: { header: 'Preview report', text: report.text, copyLabel: 'Copy report', open: false },
      cancel: { label: 'Close' }, confirm: { label: 'Send to Developers', kind: 'primary' }, onCancelResult: true,
      onConfirm: async (ctx) => {
        ctx.bOk.disabled = true; ctx.bPreview.disabled = true; ctx.status('Sending...', '');
        const r = await WK.errors.send(report);
        if (r.ok) {
          sent = true; ctx.result = true;
          ctx.status('Error report sent successfully. Report ID: ' + report.errorId, 'success');
          ctx.bPreview.classList.add('wk-hidden'); ctx.bOk.classList.add('wk-hidden'); ctx.bCancel.textContent = 'Close'; ctx.bCancel.focus();
        } else {
          ctx.status("Couldn't send the report: " + r.errorMessage + " The report hasn't been lost \u2014 you can retry or copy it.", 'error');
          ctx.bOk.textContent = 'Retry'; ctx.bOk.disabled = false; ctx.bPreview.disabled = false;
        }
      },
    }).then(() => sent);
  });

  // ------------------------------------------------------------------------------------------------
  // notifications  (NotificationService + NotificationHostViewModel + NotificationItemViewModel)
  // ------------------------------------------------------------------------------------------------
  const SEVERITIES = ['Success', 'Error', 'Warning', 'Update', 'Info'];
  let notes = [];            // newest first
  let noteSeq = 0;
  const noteById = (id) => notes.find((n) => n.id === id);
  const notesChanged = () => { renderToasts(); WK.emit('notifications:changed', WK.notifications.items()); };

  function startNoteTimer(n) {
    if (n.autoDismissMs == null) return;
    n.timerStarted = Date.now(); clearTimeout(n.timer);
    n.timer = setTimeout(() => { n.timer = null; n.timerActive = false; WK.notifications.dismiss(n.id); }, Math.max(0, n.remaining));
    n.timerActive = true;
  }
  WK.notify = function (req) {
    const r = req || {};
    const sev = SEVERITIES.find((s) => s.toLowerCase() === String(r.severity || 'Info').toLowerCase()) || 'Info';
    if (settings.notificationsEnabled === false && sev !== 'Error') return 0;          // Settings > Notifications is off: only errors still interrupt
    const n = {
      id: ++noteSeq, title: String(r.title == null ? '' : r.title), description: blank(r.description) ? null : String(r.description), severity: sev,
      actionText: blank(r.actionText) ? null : String(r.actionText), action: typeof r.action === 'function' ? r.action : null,
      visible: true, animationsEnabled: animIntensity !== 'Off',
      autoDismissMs: r.autoDismissMs === undefined ? 3000 : (r.autoDismissMs === null ? null : Number(r.autoDismissMs)),
      timer: null, timerActive: false, timerStarted: 0, remaining: 0, tag: r.tag || null,
    };
    if (n.autoDismissMs != null) { n.remaining = n.autoDismissMs; startNoteTimer(n); }
    notes.unshift(n); notesChanged();
    return n.id;
  };
  WK.toast = (title, description, severity, opts) => WK.notify(Object.assign({ title, description, severity }, opts || {}));
  WK.notifications = {
    items: () => notes.map((n) => ({ id: n.id, title: n.title, description: n.description, severity: n.severity, actionText: n.actionText, visible: n.visible, animationsEnabled: n.animationsEnabled, tag: n.tag })),
    pause: function (id) {
      const n = noteById(id); if (!n || !n.timerActive) return;
      clearTimeout(n.timer); n.timer = null; n.timerActive = false;
      n.remaining = Math.max(0, n.remaining - (Date.now() - n.timerStarted));
    },
    resume: function (id) {
      const n = noteById(id); if (!n || n.autoDismissMs == null || n.timerActive || !n.visible) return;
      if (n.remaining <= 0) { WK.notifications.dismiss(id); return; }
      startNoteTimer(n);
    },
    dismiss: async function (id) {
      const n = noteById(id); if (!n || !n.visible) return;
      clearTimeout(n.timer); n.timer = null; n.timerActive = false;
      n.visible = false; notesChanged();
      if (n.animationsEnabled) await sleep(160);
      notes = notes.filter((x) => x !== n); notesChanged();
    },
    invoke: async function (id) {
      const n = noteById(id); if (!n) return;
      if (n.action) { try { n.action(); } catch (e) { try { console.error(e); } catch (_) { /* ignore */ } } }
      await WK.notifications.dismiss(id);
    },
    dismissAll: () => Promise.all(notes.map((n) => WK.notifications.dismiss(n.id))).then(() => undefined),
  };

  const toastEls = new Map();
  function renderToasts() {
    if (!document.body) return;
    injectCss();
    let host = document.getElementById('wk-toasts');
    if (!host) { host = el('div'); host.id = 'wk-toasts'; host.setAttribute('aria-live', 'polite'); document.body.appendChild(host); }
    const ids = new Set(notes.map((n) => n.id));
    for (const [id, e] of Array.from(toastEls)) if (!ids.has(id)) { e.remove(); toastEls.delete(id); }
    let prev = null;
    for (const n of notes) { // notes are newest first => DOM order top -> bottom
      let e = toastEls.get(n.id);
      if (!e) {
        e = el('div', 'wk-toast'); e.dataset.severity = n.severity; e.dataset.anim = n.animationsEnabled ? 'on' : 'off';
        e.setAttribute('role', n.severity === 'Error' ? 'alert' : 'status');
        const body = el('div', 'wk-toast-body');
        body.append(el('div', 'wk-toast-title', n.title));
        if (n.description) body.append(el('div', 'wk-toast-desc', n.description));
        if (n.actionText) { const b = el('button', 'wk-btn wk-btn-small', n.actionText); b.type = 'button'; b.addEventListener('click', () => bg(WK.notifications.invoke(n.id))); body.append(b); }
        const x = el('button', 'wk-toast-close', '\u00d7'); x.type = 'button'; x.setAttribute('aria-label', 'Dismiss'); x.addEventListener('click', () => bg(WK.notifications.dismiss(n.id)));
        e.append(el('div', 'wk-toast-bar'), body, x);
        e.addEventListener('mouseenter', () => WK.notifications.pause(n.id));
        e.addEventListener('mouseleave', () => WK.notifications.resume(n.id));
        toastEls.set(n.id, e);
        if (n.animationsEnabled) requestAnimationFrame(() => requestAnimationFrame(() => e.classList.add('wk-in'))); else e.classList.add('wk-in');
      }
      if (!n.visible) e.classList.add('wk-leaving');
      if (e.parentNode !== host || e.previousSibling !== prev) host.insertBefore(e, prev ? prev.nextSibling : host.firstChild);
      prev = e;
    }
  }

  // ------------------------------------------------------------------------------------------------
  // error reporting
  // ------------------------------------------------------------------------------------------------
  const errParts = (err) => {
    if (err && typeof err === 'object') {
      return { exceptionType: err.name || (err.constructor && err.constructor.name) || 'Error', exceptionMessage: err.message != null ? String(err.message) : String(err), stackTrace: err.stack ? String(err.stack) : '' };
    }
    return { exceptionType: 'Error', exceptionMessage: String(err), stackTrace: '' };
  };
  let reportOpen = false, lastReportAt = 0;
  WK.errors = {
    autoHandle: true,
    createReport: (err, component) => WK.native('error_report_create', Object.assign({ component: component || 'Unhandled Exception' }, errParts(err))),
    send: async function (report) {
      try {
        return await WK.native('error_report_send', { errorId: report.errorId, winKitVersion: report.winKitVersion, windowsVersion: report.windowsVersion, architecture: report.architecture, component: report.component, exceptionType: report.exceptionType, exceptionMessage: report.exceptionMessage, stackTrace: report.stackTrace, timestamp: report.timestamp });
      } catch (e) { return { ok: false, errorMessage: errMsg(e) }; }
    },
    logCrash: (text) => WK.native('crash_log', { text: String(text) }),
    show: async function (err, component) {
      if (reportOpen) return false;
      reportOpen = true;
      try {
        const p = errParts(err);
        bg(WK.errors.logCrash(p.exceptionType + ': ' + p.exceptionMessage + '\n' + p.stackTrace));
        const report = await WK.errors.createReport(err, component);
        return await WK.showErrorReport('Something went wrong', 'WinKit ran into an unexpected error. You can keep using the app, but some features may not work correctly until you restart.', report);
      } catch (_) { return false; }
      finally { reportOpen = false; }
    },
  };
  function installErrorHandlers() {
    const handle = (err, component) => {
      if (!WK.errors.autoHandle) return;
      const m = String(err && err.message || err || '');
      if (/ResizeObserver loop/i.test(m)) return;
      const now = Date.now(); if (now - lastReportAt < 1000) return; lastReportAt = now;
      bg(WK.errors.show(err, component));
    };
    window.addEventListener('error', (e) => { if (e.error || e.message) handle(e.error || new Error(e.message), 'Unhandled Exception'); });
    window.addEventListener('unhandledrejection', (e) => handle(e.reason, 'Unhandled Promise Rejection'));
  }

  // ------------------------------------------------------------------------------------------------
  // updates + About
  // ------------------------------------------------------------------------------------------------
  const UPDATE_INTERVAL_MS = 6 * 3600 * 1000;
  let updateTimer = null;
  const checkNative = async function () {
    try { const r = await WK.native('update_check'); return r && typeof r === 'object' ? r : { status: 'Failed', errorMessage: 'WinKit got no answer.' }; } catch (e) { return { status: 'Failed', errorMessage: errMsg(e) }; }
  };
  WK.updates = {
    check: checkNative,
    backgroundCheck: async function () { // App.CheckForUpdatesInBackgroundAsync
      try {
        const result = await checkNative();
        await WK.settings.set({ lastUpdateCheckUtc: new Date().toISOString() });
        if (result.status !== 'UpdateAvailable') return result;
        if (settings.skippedUpdateVersion && String(result.latestVersion).toLowerCase() === String(settings.skippedUpdateVersion).toLowerCase()) return result;
        if (notes.some((n) => n.tag === 'update:' + result.latestVersion)) return result; // don't stack duplicates every 6 h
        WK.notify({
          title: 'WinKit ' + result.latestVersion + ' is available', description: "Click to review what's changed and install it.", severity: 'Update',
          actionText: 'Update Now', autoDismissMs: null, tag: 'update:' + result.latestVersion,
          action: () => WK.emit('update:review', { result }),
        });
        return result;
      } catch (_) { return null; }
    },
    startAutoChecks: function () {
      WK.updates.stopAutoChecks();
      bg(WK.updates.backgroundCheck());
      updateTimer = setInterval(() => bg(WK.updates.backgroundCheck()), UPDATE_INTERVAL_MS);
    },
    stopAutoChecks: function () { if (updateTimer) clearInterval(updateTimer); updateTimer = null; },
  };

  const aboutState = { version: '1.0.0', statusText: 'Check for updates to see if a newer version is available.', isChecking: false, updateAvailable: false, isInstalling: false, progress: null };
  let lastCheck = null;
  const aboutChanged = () => WK.emit('about:changed', WK.about.getState());
  WK.about = {
    getState: () => Object.assign({}, aboutState),
    lastResult: () => (lastCheck ? clone(lastCheck) : null),
    check: async function () {
      aboutState.isChecking = true; aboutState.statusText = 'Checking for updates...'; aboutChanged();
      try {
        const result = await checkNative(); lastCheck = result;
        bg(WK.settings.set({ lastUpdateCheckUtc: new Date().toISOString() }));
        aboutState.updateAvailable = result.status === 'UpdateAvailable';
        aboutState.statusText = result.status === 'UpToDate' ? "You're on the latest version."
          : result.status === 'UpdateAvailable' ? 'WinKit ' + result.latestVersion + ' is available.'
            : "Couldn't check for updates: " + (result.errorMessage || 'unknown error');
      } finally { aboutState.isChecking = false; aboutChanged(); }
    },
    showPendingUpdate: async function (result) {
      lastCheck = result; aboutState.updateAvailable = result.status === 'UpdateAvailable';
      aboutState.statusText = aboutState.updateAvailable ? 'WinKit ' + result.latestVersion + ' is available.' : "You're on the latest version.";
      aboutChanged();
      if (aboutState.updateAvailable) await WK.about.install();
    },
    install: async function () {
      if (!lastCheck || lastCheck.status !== 'UpdateAvailable') return;
      const ok = await WK.updateConfirm(lastCheck.latestVersion || '', lastCheck.releaseNotes);
      if (!ok) return;
      aboutState.statusText = 'Downloading update...'; aboutState.isInstalling = true; aboutState.progress = null; aboutChanged();
      let res;
      try { res = await WK.native('update_download_apply', { latestVersion: lastCheck.latestVersion || '' }); }
      catch (e) { res = { status: 'Failed', errorMessage: errMsg(e) }; }
      if (!res || typeof res !== 'object') res = { status: 'Failed', errorMessage: 'WinKit got no answer.' };
      aboutState.isInstalling = false; aboutState.progress = null;
      if (res.status === 'Started') {
        WK.activity.record('Updating to ' + lastCheck.latestVersion, 'Info');
        aboutChanged();
        try { await WK.app.quit(); } catch (_) { /* ignore */ }
      } else {
        aboutState.statusText = 'Update failed: ' + (res.errorMessage || 'unknown error'); aboutChanged();
        await WK.alert('Update failed', res.errorMessage || 'The update could not be applied.');
      }
    },
    skipThisVersion: async function () {
      if (!lastCheck || !lastCheck.latestVersion) return;
      await WK.settings.set({ skippedUpdateVersion: lastCheck.latestVersion });
      aboutState.updateAvailable = false;
      aboutState.statusText = 'Skipped version ' + lastCheck.latestVersion + ". You'll be notified again for future releases.";
      aboutChanged();
    },
  };
  WK.on('update_progress', (p) => { if (aboutState.isInstalling) { aboutState.progress = p; aboutChanged(); } });

  // ------------------------------------------------------------------------------------------------
  // init  (mirrors App.OnStartup)
  // ------------------------------------------------------------------------------------------------
  let initPromise = null;
  WK.init = function () {
    if (initPromise) return initPromise;
    initPromise = (async () => {
      const step = async (fn) => { try { await fn(); } catch (e) { try { console.error('[WK] init step failed', e); } catch (_) { /* ignore */ } } };
      if (!document.body) await new Promise((r) => document.addEventListener('DOMContentLoaded', r, { once: true }));
      injectCss();
      await step(async () => { const i = await WK.app.info(); elevated = !!(i && i.isAdmin); aboutState.version = (i && i.version) || aboutState.version; });
      await step(() => WK.profile.load());
      await step(() => WK.settings.load());
      await step(() => WK.activity.load());
      await step(() => WK.themes.init());
      await step(() => WK.themes.apply(settings.themeName));
      try {           // when Windows switches between light and dark, an "Auto" theme follows it straight away
        const mq = window.matchMedia('(prefers-color-scheme: light)');
        const follow = () => { if (settings.themeName === SYSTEM && !draft) bg(WK.themes.apply(SYSTEM)); };
        if (mq.addEventListener) mq.addEventListener('change', follow); else if (mq.addListener) mq.addListener(follow);
      } catch (_) { /* no media query support: keep the theme chosen at start-up */ }
      installErrorHandlers();
      renderToasts();
      if (settings.autoCheckForUpdates) WK.updates.startAutoChecks();
      const info = { onboardingCompleted: WK.profile.current().hasCompletedOnboarding };
      WK.emit('ready', info);
      return info;
    })();
    return initPromise;
  };
  WK.ready = new Promise((resolve) => { WK.on('ready', resolve); });
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', () => bg(WK.init()), { once: true });
  else bg(WK.init());
})();
