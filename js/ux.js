/* ux.js - the "feels like a Mac app" layer. Loads AFTER shell.js, BEFORE pages/*.js.
 *   Quick Open (Ctrl+K)   Spotlight-style jump bar: pages, tabs and common actions, fuzzy matched.
 *   WK.ui.search(...)     the search field every list uses (Ctrl+F focuses it, Esc clears it).
 *   Shortcuts             Ctrl+K quick open, Ctrl+F search, Ctrl+R refresh, Ctrl+, settings, Ctrl+B sidebar, Ctrl+1..8 pages.
 *   Sidebar               collapsible to icons, a badge on Diagnostics when something needs attention.
 *   Window title          follows the page ("Network - WinKit"), like the title of a Mac window.
 */
(function () {
  'use strict';
  const W = window.WK, ui = W.ui, el = ui.el;

  const css = `
.wk-side-top{display:flex;align-items:center;justify-content:space-between;padding-right:10px}
.wk-side-top .brand{padding-right:6px}
.wk-icon-btn{width:28px;height:28px;padding:0;display:inline-flex;align-items:center;justify-content:center;border-color:transparent;background:transparent;color:var(--wk-text-muted);flex:none}
.wk-icon-btn svg{width:16px;height:16px;fill:none;stroke:currentColor;stroke-width:1.5;stroke-linecap:round;stroke-linejoin:round}
.wk-icon-btn:hover:not(:disabled){color:var(--wk-text-primary);border-color:transparent}
.wk-side-search{display:flex;align-items:center;gap:9px;margin:2px 10px 6px;padding:7px 10px;width:calc(100% - 20px);border-color:var(--wk-border);background:var(--wk-background);color:var(--wk-text-muted);font-weight:400;text-align:left}
.wk-side-search svg{width:14px;height:14px;fill:none;stroke:currentColor;stroke-width:1.6;stroke-linecap:round;flex:none}
.wk-side-search span{flex:1}
.wk-kbd{font:11px/1 var(--wk-font);padding:3px 6px;border-radius:5px;border:1px solid var(--wk-border);color:var(--wk-text-muted);background:var(--wk-surface);white-space:nowrap}
.wk-nav-badge{margin-left:auto;min-width:18px;height:18px;padding:0 6px;border-radius:9px;background:rgba(var(--wk-warning-rgb),.2);color:var(--wk-warning);font-size:11px;font-weight:700;line-height:18px;text-align:center}
#wk-side,.wk-nav-item span,.brand>span:last-child{transition:width var(--wk-anim-normal) ease}
body.wk-collapsed #wk-side{width:64px}
body.wk-collapsed .wk-nav-item span:not(.wk-nav-badge),body.wk-collapsed .wk-nav-head,body.wk-collapsed .wk-side-search span,body.wk-collapsed .wk-side-search .wk-kbd,body.wk-collapsed .brand>span:last-child,body.wk-collapsed .wk-side-top .wk-icon-btn,body.wk-collapsed .wk-side-foot span:not(.wk-badge){display:none}
body.wk-collapsed .wk-nav-item{justify-content:center;padding:9px 0}
body.wk-collapsed .wk-nav-badge{position:absolute;top:2px;right:6px;min-width:8px;width:8px;height:8px;padding:0;font-size:0}
body.wk-collapsed .wk-side-search{justify-content:center;padding:8px 0;margin:2px 10px 6px}
body.wk-collapsed .wk-side-top{justify-content:center;padding:0}
body.wk-collapsed .wk-side-top .brand{padding:18px 0 14px;cursor:pointer}
body.wk-collapsed .wk-side-foot{justify-content:center;padding:12px 4px}

.wk-search{position:relative;display:inline-flex;align-items:center}
.wk-search>svg{position:absolute;left:10px;width:14px;height:14px;fill:none;stroke:var(--wk-text-muted);stroke-width:1.6;stroke-linecap:round;pointer-events:none}
.wk-search input{width:230px;padding:6px 28px 6px 32px;border-radius:calc(var(--wk-radius-sm) + 6px)}
.wk-search input::-webkit-search-cancel-button{display:none}
.wk-search .wk-clear{position:absolute;right:5px;width:20px;height:20px;padding:0;border-radius:50%;border-color:transparent;background:rgba(var(--wk-text-primary-rgb),.14);color:var(--wk-text-secondary);font-size:13px;line-height:18px}
.wk-search .wk-clear:hover:not(:disabled){background:rgba(var(--wk-text-primary-rgb),.24);border-color:transparent}

.wk-menu{position:fixed;z-index:9500;min-width:200px;max-width:340px;padding:5px;background:var(--wk-surface-elevated);border:1px solid var(--wk-border);border-radius:calc(var(--wk-radius) + 2px);box-shadow:var(--wk-shadow-md);animation:wk-dlg-in .1s ease-out}
.wk-menu .it{display:block;width:100%;text-align:left;padding:7px 12px;border:0;border-radius:6px;background:transparent;font-weight:400;color:var(--wk-text-primary);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.wk-menu .it:hover:not(:disabled),.wk-menu .it:focus-visible{background:rgba(var(--wk-accent-rgb),.18);border:0}
.wk-menu .it:disabled{color:var(--wk-text-muted);cursor:default}
.wk-menu .it.danger:not(:disabled){color:var(--wk-error)}
.wk-menu .sep{height:1px;margin:5px 4px;background:var(--wk-border)}
[data-copy]{cursor:copy}
[data-copy]:hover{color:var(--wk-accent)}
.wk-pal-ov{align-items:flex-start!important;padding-top:13vh!important}
.wk-pal{width:560px;max-width:100%;background:var(--wk-surface-elevated);border:1px solid var(--wk-border);border-radius:calc(var(--wk-radius) + 4px);box-shadow:var(--wk-shadow-md);overflow:hidden;animation:wk-dlg-in .14s ease-out}
.wk-pal-in{display:flex;align-items:center;gap:10px;padding:14px 16px;border-bottom:1px solid var(--wk-border)}
.wk-pal-in svg{width:17px;height:17px;fill:none;stroke:var(--wk-text-muted);stroke-width:1.6;stroke-linecap:round;flex:none}
.wk-pal-in input{flex:1;border:0!important;background:transparent!important;box-shadow:none!important;padding:0;font-size:calc(var(--wk-font-size-body) + 3px);color:var(--wk-text-primary)}
.wk-pal-list{max-height:min(380px,50vh);overflow:auto;padding:6px}
.wk-pal-item{display:flex;align-items:center;gap:12px;padding:9px 10px;border-radius:var(--wk-radius-sm);cursor:pointer}
.wk-pal-item.on{background:rgba(var(--wk-accent-rgb),.18)}
.wk-pal-item .wk-pal-ic{width:28px;height:28px;border-radius:7px;background:rgba(var(--wk-text-primary-rgb),.07);display:flex;align-items:center;justify-content:center;flex:none;color:var(--wk-text-secondary)}
.wk-pal-item.on .wk-pal-ic{color:var(--wk-accent)}
.wk-pal-ic svg{width:15px;height:15px;fill:none;stroke:currentColor;stroke-width:1.4;stroke-linecap:round;stroke-linejoin:round}
.wk-pal-item b{font-weight:500;flex:1;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.wk-pal-item small{color:var(--wk-text-muted);font-size:var(--wk-font-size-small);white-space:nowrap}
.wk-pal-empty{padding:26px;text-align:center;color:var(--wk-text-muted)}
.wk-pal-foot{display:flex;gap:16px;padding:9px 16px;border-top:1px solid var(--wk-border);color:var(--wk-text-muted);font-size:var(--wk-font-size-small)}
`;
  document.head.append(el('style', { text: css, 'data-ux': '' }));

  const ICON_SEARCH = '<svg viewBox="0 0 16 16" aria-hidden="true"><circle cx="7" cy="7" r="4.6"/><path d="M10.5 10.5 14 14"/></svg>';
  const svgOf = (key) => '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="' + ((W.icons && W.icons[key]) || 'M8,3 V13 M3,8 H13') + '"/></svg>';
  const titleOfPage = (id) => { const n = document.querySelector('.wk-nav-item[data-id="' + id + '"]'); return n ? n.textContent.replace(/\d+$/, '').trim() : id; };

  // ------------------------------------------------------------------------------------------------ search field
  /* WK.ui.search(onInput, placeholder?) -> { el, input, value(), clear(), focus() }.  Esc clears; Ctrl+F focuses the visible one. */
  ui.search = function (onInput, placeholder) {
    const input = el('input', { type: 'search', placeholder: placeholder || 'Search', 'aria-label': placeholder || 'Search', spellcheck: 'false', autocomplete: 'off', 'data-search': '' });
    const clear = el('button', { class: 'wk-clear wk-hide', type: 'button', 'aria-label': 'Clear search', title: 'Clear', text: '×' });
    const box = el('div', { class: 'wk-search', html: ICON_SEARCH }, input, clear);
    const sync = () => { clear.classList.toggle('wk-hide', input.value === ''); onInput && onInput(input.value.trim()); };
    input.addEventListener('input', sync);
    input.addEventListener('keydown', (e) => { if (e.key === 'Escape' && input.value) { e.preventDefault(); e.stopPropagation(); input.value = ''; sync(); } });
    clear.addEventListener('click', () => { input.value = ''; sync(); input.focus(); });
    return { el: box, input, value: () => input.value.trim(), clear() { input.value = ''; sync(); }, focus() { input.focus(); input.select(); } };
  };
  /* true when every word of `query` occurs somewhere in `text` (case-insensitive) */
  ui.matches = (text, query) => { if (!query) return true; const t = String(text == null ? '' : text).toLowerCase(); return query.toLowerCase().split(/\s+/).every(w => t.includes(w)); };

  // ------------------------------------------------------------------------------------------------ quick open
  function destinations() {
    const out = [], add = (title, group, kw, icon, run) => out.push({ title, group, kw: kw || '', icon, run });
    const go = (id, opts) => () => W.nav.go(id, opts || {});
    add('Dashboard', 'Go to', 'home overview status', 'dashboard', go('dashboard'));
    add('System', 'Go to', '', 'system', go('system'));
    [['processes', 'Processes', 'task manager cpu memory end kill running apps'], ['startup', 'Startup apps', 'startup boot login autostart programs'],
      ['services', 'Services', 'windows services start stop'], ['apps', 'Installed applications', 'programs uninstall software'], ['disk', 'Disk usage', 'storage space folders large files what is using my disk drive full size'],
      ['env', 'Environment variables', 'path env variables'], ['hosts', 'Hosts file', 'hosts block dns edit'], ['info', 'System information', 'specs hardware cpu gpu ram windows version uptime']]
      .forEach(([tab, t, kw]) => add(t, 'System', kw, 'system', go('system', { tab })));
    add('Network', 'Go to', '', 'network', go('network'));
    [['ping', 'Ping', 'test connection latency internet'], ['dns', 'DNS lookup', 'resolve domain name'], ['traceroute', 'Traceroute', 'trace route hops path'],
      ['ipconfig', 'IP configuration', 'ip address adapters mac gateway dns servers'], ['wifi', 'Wi-Fi details', 'wifi wireless wlan signal strength channel router nearby networks saved passwords ssid'], ['conntest', 'Connection test', 'diagnose internet slow no connection troubleshoot router dns fix'], ['ports', 'Open ports', 'ports listening netstat connections tcp udp port in use who is using'],
      ['maintenance', 'Network maintenance', 'flush dns renew dhcp winsock tcp/ip reset repair']]
      .forEach(([tab, t, kw]) => add(t, 'Network', kw, 'network', go('network', { tab })));
    add('Cleanup', 'Go to', 'free space disk temp junk', 'cleanup', go('cleanup'));
    add('Diagnostics', 'Go to', 'health check problems issues', 'diagnostics', go('diagnostics'));
    add('Activity', 'Go to', 'history log', 'activity', go('activity'));
    add('Settings', 'Go to', 'preferences options theme appearance', 'settings', go('settings'));
    add('About', 'Go to', 'version updates privacy', 'about', go('about'));
    add('Export health report', 'Action', 'save share send support summary system report help friend diagnose', 'diagnostics', () => W.report.open());
    add('Run diagnostics', 'Action', 'scan check health problems', 'diagnostics', go('diagnostics', { run: true }));
    add('Clean temporary files', 'Action', 'cleanup free space junk', 'cleanup', go('cleanup'));
    add('Flush DNS cache', 'Action', 'dns network fix', 'network', go('network', { tab: 'maintenance' }));
    add('Check for updates', 'Action', 'update upgrade new version', 'about', () => { W.nav.go('about'); W.about && W.about.check(); });
    if (!W.admin.isElevated()) add('Restart as administrator', 'Action', 'admin elevate uac', 'settings', () => W.admin.restart());
    add('Open data folder', 'Action', 'files settings appdata', 'about', () => W.shell.openFolder('@data'));
    add('Open logs folder', 'Action', 'crash log', 'about', () => W.shell.openFolder('@logs'));
    add('Toggle sidebar', 'Action', 'hide show collapse', 'dashboard', () => toggleSidebar());
    add('Keyboard shortcuts', 'Help', 'keys hotkeys', 'about', () => showShortcuts());
    W.themes.names().forEach(n => add('Theme: ' + n, 'Appearance', 'color dark light appearance', 'settings', () => W.themes.select(n)));
    return out;
  }
  function rank(d, q) {
    const t = d.title.toLowerCase(), all = (d.title + ' ' + d.group + ' ' + d.kw).toLowerCase(), words = q.toLowerCase().split(/\s+/).filter(Boolean);
    if (!words.every(w => all.includes(w))) return -1;
    return t === q.toLowerCase() ? 0 : t.startsWith(q.toLowerCase()) ? 1 : words.every(w => t.includes(w)) ? 2 : 3;
  }
  let palOpen = false;
  function openPalette() {
    if (palOpen || W.nav.current() === 'onboarding') return;
    palOpen = true;
    const prev = document.activeElement, all = destinations();
    let results = [], active = 0;
    const input = el('input', { type: 'text', placeholder: 'Jump to a page, tool or action…', 'aria-label': 'Quick open', spellcheck: 'false', autocomplete: 'off' });
    const list = el('div', { class: 'wk-pal-list', role: 'listbox' });
    const box = el('div', { class: 'wk-pal', role: 'dialog', 'aria-label': 'Quick open' },
      el('div', { class: 'wk-pal-in', html: ICON_SEARCH }, input), list,
      el('div', { class: 'wk-pal-foot' }, el('span', { text: '↑↓ to move' }), el('span', { text: 'Enter to open' }), el('span', { text: 'Esc to close' })));
    const ov = el('div', { class: 'wk-overlay wk-pal-ov' }, box);
    function close() { document.removeEventListener('keydown', onKey, true); ov.remove(); palOpen = false; try { prev && prev.focus && prev.focus(); } catch (_) { /* ignore */ } }
    function run(d) { close(); try { d.run(); } catch (e) { console.error(e); } }
    function render() {
      const q = input.value.trim();
      results = (q ? all.map(d => [d, rank(d, q)]).filter(x => x[1] >= 0).sort((a, b) => a[1] - b[1]).map(x => x[0]) : all.filter(d => d.group === 'Go to' || d.title === 'Run diagnostics')).slice(0, 9);
      active = Math.min(active, Math.max(0, results.length - 1));
      list.replaceChildren(...(results.length ? results.map((d, i) => el('div', { class: 'wk-pal-item' + (i === active ? ' on' : ''), role: 'option', onmousemove: () => { if (active !== i) { active = i; paint(); } }, onclick: () => run(d) },
        el('span', { class: 'wk-pal-ic', html: svgOf(d.icon) }), el('b', { text: d.title }), el('small', { text: d.group }))) : [el('div', { class: 'wk-pal-empty', text: 'Nothing matches "' + q + '".' })]));
    }
    function paint() { [...list.children].forEach((c, i) => c.classList.toggle('on', i === active)); const a = list.children[active]; a && a.scrollIntoView({ block: 'nearest' }); }
    function onKey(e) {
      if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); close(); }
      else if (e.key === 'ArrowDown') { e.preventDefault(); active = results.length ? (active + 1) % results.length : 0; paint(); }
      else if (e.key === 'ArrowUp') { e.preventDefault(); active = results.length ? (active - 1 + results.length) % results.length : 0; paint(); }
      else if (e.key === 'Enter') { e.preventDefault(); if (results[active]) run(results[active]); }
      else if (e.key === 'Tab') { e.preventDefault(); active = results.length ? (active + (e.shiftKey ? -1 : 1) + results.length) % results.length : 0; paint(); }       // Tab walks the results like the arrow keys
    }
    input.addEventListener('input', () => { active = 0; render(); });
    ov.addEventListener('mousedown', (e) => { if (e.target === ov) close(); });
    document.addEventListener('keydown', onKey, true);
    document.body.appendChild(ov); render(); input.focus();
  }
  W.quickOpen = openPalette;

  // ------------------------------------------------------------------------------------------------ shortcuts help
  function showShortcuts() {
    return W.alert('Keyboard shortcuts',
      'Ctrl+K     Quick open: jump to any page, tool or action\n' +
      'Ctrl+F     Search the list on this page\n' +
      'Ctrl+R     Refresh this page\n' +
      'Ctrl+B     Show or hide the sidebar\n' +
      'Ctrl+,     Open Settings\n' +
      'Ctrl+1-8   Dashboard, System, Network, Cleanup, Diagnostics, Activity, Settings, About\n' +
      'Esc        Clear a search or close a dialog');
  }
  W.showShortcuts = showShortcuts;

  // ------------------------------------------------------------------------------------------------ sidebar
  function setCollapsed(on, persist) {
    document.body.classList.toggle('wk-collapsed', !!on);
    const t = document.getElementById('wk-side-toggle'); if (t) t.title = on ? 'Show sidebar (Ctrl+B)' : 'Hide sidebar (Ctrl+B)';
    if (persist) { try { W.store.write('ui', { sidebarCollapsed: !!on }); } catch (_) { /* ignore */ } }
  }
  function toggleSidebar() { setCollapsed(!document.body.classList.contains('wk-collapsed'), true); }

  function buildSidebarExtras() {
    const side = document.getElementById('wk-side'), brand = side && side.querySelector('.brand'), nav = document.getElementById('wk-nav');
    if (!side || !brand || !nav) return;
    const top = el('div', { class: 'wk-side-top' });
    brand.replaceWith(top); top.append(brand);
    brand.addEventListener('click', () => { if (document.body.classList.contains('wk-collapsed')) toggleSidebar(); });
    top.append(el('button', { id: 'wk-side-toggle', class: 'wk-icon-btn', type: 'button', title: 'Hide sidebar (Ctrl+B)', 'aria-label': 'Toggle sidebar', onclick: toggleSidebar,
      html: '<svg viewBox="0 0 16 16" aria-hidden="true"><rect x="2" y="3" width="12" height="10" rx="2"/><path d="M6 3v10"/></svg>' }));
    const sb = el('button', { class: 'wk-side-search', type: 'button', title: 'Quick open (Ctrl+K)', onclick: openPalette, html: ICON_SEARCH + '<span>Search</span>' });
    sb.append(el('span', { class: 'wk-kbd', text: 'Ctrl K' }));
    side.insertBefore(sb, nav);
  }

  function setBadge(count) {
    const item = document.querySelector('.wk-nav-item[data-id="diagnostics"]'); if (!item) return;
    let b = item.querySelector('.wk-nav-badge');
    if (!(count > 0)) { if (b) b.remove(); item.title = 'Diagnostics  (Ctrl+5)'; return; }
    if (!b) { b = el('span', { class: 'wk-nav-badge' }); item.append(b); }
    b.textContent = String(count); item.title = 'Diagnostics: ' + count + (count === 1 ? ' issue needs' : ' issues need') + ' attention  (Ctrl+5)';
  }

  // ------------------------------------------------------------------------------------------------ keys
  function visible(n) { return n && n.offsetParent !== null; }

  // ------------------------------------------------------------------------------------------------ copy + context menus
  // WK.ui.copy(text): copy and confirm with a toast.   WK.ui.menu(items, x, y): items = [{label, run, danger?, disabled?, title?} | {sep:true}].
  // WK.ui.contextMenu(node, () => items): right-click (or the menu key / Shift+F10 while the node has focus) opens the menu.
  ui.copy = async (text, what) => { if (text == null || text === '') return; if (await W.shell.copy(String(text))) W.toast('Copied', what || (String(text).length > 60 ? String(text).slice(0, 57) + '...' : String(text)), 'Success'); };
  ui.searchWeb = (q) => W.shell.openUrl('https://www.bing.com/search?q=' + encodeURIComponent(q));
  let menuEl = null;
  function closeMenu() {
    if (!menuEl) return;
    menuEl.remove(); menuEl = null;
    for (const [t, f, c] of menuHooks) document.removeEventListener(t, f, c); menuHooks.length = 0; window.removeEventListener('blur', closeMenu); window.removeEventListener('resize', closeMenu);
  }
  const menuHooks = [];
  ui.menu = function (items, x, y) {
    closeMenu();
    const prev = document.activeElement, m = el('div', { class: 'wk-menu', role: 'menu' });
    items.forEach((it) => m.append(it.sep ? el('div', { class: 'sep', role: 'separator' }) :
      el('button', { class: 'it' + (it.danger ? ' danger' : ''), role: 'menuitem', type: 'button', disabled: !!it.disabled, title: it.title || null, text: it.label, onclick: () => { closeMenu(); try { prev && prev.focus && prev.focus(); } catch (_) { /* ignore */ } if (it.run) it.run(); } })));
    document.body.append(m); menuEl = m;
    const r = m.getBoundingClientRect();
    m.style.left = Math.max(8, Math.min(x, innerWidth - r.width - 8)) + 'px'; m.style.top = Math.max(8, Math.min(y, innerHeight - r.height - 8)) + 'px';
    const live = () => [...m.querySelectorAll('.it:not(:disabled)')];
    const on = (t, f, c) => { document.addEventListener(t, f, c); menuHooks.push([t, f, c]); };
    on('keydown', (e) => {
      const k = e.key, b = live(), i = b.indexOf(document.activeElement);
      if (k === 'Escape' || k === 'Tab') { e.preventDefault(); e.stopPropagation(); closeMenu(); try { prev && prev.focus && prev.focus(); } catch (_) { /* ignore */ } }
      else if (k === 'ArrowDown') { e.preventDefault(); (b[(i + 1) % b.length] || b[0]).focus(); }
      else if (k === 'ArrowUp') { e.preventDefault(); (b[(i - 1 + b.length) % b.length] || b[b.length - 1]).focus(); }
      else if (k === 'Home') { e.preventDefault(); b[0] && b[0].focus(); } else if (k === 'End') { e.preventDefault(); b.length && b[b.length - 1].focus(); }
    }, true);
    on('mousedown', (e) => { if (!m.contains(e.target)) closeMenu(); }, true);
    on('wheel', closeMenu, { capture: true, passive: true }); on('contextmenu', (e) => { if (!m.contains(e.target)) closeMenu(); }, true);
    window.addEventListener('blur', closeMenu); window.addEventListener('resize', closeMenu);
    const first = live()[0]; if (first) first.focus();
    return m;
  };
  ui.contextMenu = function (node, getItems) {
    const open = (x, y, e) => { const items = getItems(e); if (!items || !items.length) return false; ui.menu(items, x, y); return true; };
    node.addEventListener('contextmenu', (e) => { if (open(e.clientX, e.clientY, e)) { e.preventDefault(); e.stopPropagation(); } });
    node.addEventListener('keydown', (e) => { if (e.target !== node || !(e.key === 'ContextMenu' || (e.shiftKey && e.key === 'F10'))) return; const r = node.getBoundingClientRect(); if (open(r.left + 24, r.top + r.height / 2, e)) e.preventDefault(); });
  };

  function installKeys() {
    document.addEventListener('click', (e) => {                         // click a value marked data-copy to copy it (dragging to select text does not)
      const t = e.target.closest && e.target.closest('[data-copy]'); if (!t || String(window.getSelection())) return;
      const text = t.getAttribute('data-copy') || t.textContent.trim(); if (!text || text === '-') return;
      ui.copy(text);
    });
    document.addEventListener('keydown', (e) => {
      if (!e.ctrlKey || e.altKey || e.metaKey) return;
      const busy = !!document.querySelector('.wk-overlay'), key = e.key.toLowerCase();
      if (busy || W.nav.current() === 'onboarding') return;
      if (key === 'k' && !e.shiftKey) { e.preventDefault(); openPalette(); }
      else if (key === 'f' && !e.shiftKey) {
        e.preventDefault();
        const s = [...document.querySelectorAll('.wk-page.on input[data-search]')].find(visible);
        if (s) { s.focus(); s.select(); } else openPalette();
      } else if (key === 'r' && !e.shiftKey) {
        e.preventDefault();
        const b = [...document.querySelectorAll('.wk-page.on button')].find(x => visible(x) && !x.disabled && /^(Refresh|Rescan|Reload|Run Diagnostics)$/.test(x.textContent.trim()));
        if (b) b.click();
      } else if (key === 'b' && !e.shiftKey) { e.preventDefault(); toggleSidebar(); }
      else if (e.key === ',') { e.preventDefault(); W.nav.go('settings'); }
    });
    document.addEventListener('keydown', (e) => {                       // F5 behaves like Ctrl+R
      if (e.key !== 'F5' || document.querySelector('.wk-overlay')) return;
      e.preventDefault(); const b = [...document.querySelectorAll('.wk-page.on button')].find(x => visible(x) && !x.disabled && /^(Refresh|Rescan|Reload|Run Diagnostics)$/.test(x.textContent.trim())); if (b) b.click();
    });
  }

  // ------------------------------------------------------------------------------------------------ window title + boot
  let titleTimer = null;
  function pushTitle(id) {
    clearTimeout(titleTimer);
    titleTimer = setTimeout(() => { try { const p = W.native('set_window_title', { title: id === 'onboarding' ? 'WinKit' : titleOfPage(id) + ' - WinKit' }); p && p.catch && p.catch(() => {}); } catch (_) { /* ignore */ } }, 30);
  }
  const goOrig = W.nav.go;
  W.nav.go = function (id, opts) { const r = goOrig.call(W.nav, id, opts); pushTitle(id); return r; };

  const bootOrig = W.boot;
  W.boot = async function () {
    buildSidebarExtras(); installKeys();
    try { const ui0 = await W.store.read('ui'); if (ui0 && ui0.sidebarCollapsed) setCollapsed(true, false); } catch (_) { /* default: expanded */ }
    const r = await bootOrig();
    try { const s = await W.native('diag_summary'); if (s && s.hasRun) setBadge(s.issueCount); } catch (_) { /* no summary yet */ }
    if (W.on) W.on('diag_summary_changed', (s) => setBadge(s && s.hasRun ? s.issueCount : 0));
    return r;
  };
})();
