/* shell.js - sidebar, page registry, router, helpers shared by all pages.  Loads AFTER core.js, BEFORE pages/*.js.
 *
 * PAGE CONTRACT (each pages/*.js file is an IIFE that calls this once per page):
 *   WK.pages.register('system', {
 *     title: 'System',                    // used for aria + nothing else (pages draw their own h1)
 *     create(root) { ... build DOM into `root` (a div.wk-page) ...; return { show(), hide() } },  // create() runs on first visit
 *   });
 *   show() is called every time the page becomes visible (do refresh/load there; may be async), hide() when it leaves. Both optional.
 * ROUTER:   WK.nav.go('diagnostics', { tab: 'x' })  -> shows the page, passes opts to its show(opts). WK.nav.current() -> id.
 * HELPERS (window.WK.ui):
 *   el(tag, attrs?, ...children)                    tiny DOM builder; attrs: class, text, html(use sparingly), on* handlers, any attribute; children: nodes|strings|arrays
 *   esc(s)                                          HTML-escape for innerHTML templates
 *   tabsPage(root, title, tabs, subtitle?)          System/Network style page: big title (+ optional subtitle) + tab strip. tabs = [{id,title,create(panel)->{show?,hide?}}] ; returns {select(id), current()}; a tab's create() runs on its first selection, its show() on every selection.
 *   busy(button, promiseFn)                         disables button while promiseFn() runs
 *   logo(large?)                                    the WinKit logo mark (span.wk-logo) for headers / empty states
 */
(function () {
  'use strict';
  const W = window.WK = window.WK || {};
  const ui = W.ui = {};

  ui.esc = s => String(s == null ? '' : s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  ui.el = function (tag, attrs, ...kids) {
    const n = document.createElement(tag);
    if (attrs) for (const k in attrs) {
      const v = attrs[k]; if (v == null || v === false) continue;
      if (k === 'class') n.className = v; else if (k === 'text') n.textContent = v; else if (k === 'html') n.innerHTML = v;
      else if (k.startsWith('on') && typeof v === 'function') n.addEventListener(k.slice(2), v);
      else if (k === 'value') n.value = v; else if (k === 'checked') n.checked = !!v; else if (k === 'disabled') n.disabled = !!v;
      else n.setAttribute(k, v === true ? '' : v);
    }
    const add = k => { if (k == null || k === false) return; if (Array.isArray(k)) k.forEach(add); else n.append(k.nodeType ? k : document.createTextNode(String(k))); };
    kids.forEach(add); return n;
  };
  ui.busy = async function (btn, fn) { btn.disabled = true; try { return await fn(); } finally { btn.disabled = false; } };
  ui.logo = function (large) { return ui.el('span', { class: 'wk-logo' + (large ? ' lg' : ''), html: '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M2 4.5 L4.8 12 L8 6.5 L11.2 12 L14 4.5"/></svg>' }); };

  ui.tabsPage = function (root, title, tabs, subtitle) {
    const strip = ui.el('div', { class: 'wk-tabs', role: 'tablist', 'aria-label': title });
    const body = ui.el('div');
    root.append(ui.el('div', { class: 'wk-pagehead' }, ui.el('h1', { class: 'wk-display', text: title }), subtitle ? ui.el('div', { class: 'wk-sub', text: subtitle }) : null), strip, body);
    const st = {}; let cur = null;
    tabs.forEach((t, i) => {
      t.tabEl = ui.el('div', { class: 'wk-tab', role: 'tab', 'aria-selected': 'false', text: t.title, tabindex: 0, onclick: () => select(t.id), onkeydown: e => {
        if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); select(t.id); }
        else if (e.key === 'ArrowRight' || e.key === 'ArrowLeft') {                       // arrow keys move between tabs
          e.preventDefault(); const n = tabs[(i + (e.key === 'ArrowRight' ? 1 : tabs.length - 1)) % tabs.length]; select(n.id); n.tabEl.focus();
        }
      } });
      t.panel = ui.el('div', { class: 'wk-hide', role: 'tabpanel' }); strip.append(t.tabEl); body.append(t.panel); st[t.id] = t;
    });
    function select(id) {
      const t = st[id]; if (!t) return;
      if (cur && cur !== t) { cur.panel.classList.add('wk-hide'); cur.tabEl.classList.remove('on'); cur.tabEl.setAttribute('aria-selected', 'false'); cur.api && cur.api.hide && cur.api.hide(); }
      cur = t; t.tabEl.classList.add('on'); t.tabEl.setAttribute('aria-selected', 'true'); t.panel.classList.remove('wk-hide');
      if (!t.api) t.api = t.create(t.panel) || {};
      t.api.show && t.api.show();
    }
    return { select, current: () => cur && cur.id, hide: () => { cur && cur.api && cur.api.hide && cur.api.hide(); }, showCurrent: () => { cur && cur.api && cur.api.show && cur.api.show(); } };
  };

  // ---- icons (from Resources/Icons.xaml; SVG path data, 16x16 viewBox) ----
  W.icons = {
    dashboard: 'M1,1 H7 V7 H1 Z M9,1 H15 V7 H9 Z M1,9 H7 V15 H1 Z M9,9 H15 V15 H9 Z',
    system: 'M1,2 H15 V11 H1 Z M8,11 V13.5 M5,14 H11',
    network: 'M1,10 H3.5 V14 H1 Z M5,7 H7.5 V14 H5 Z M9,4 H11.5 V14 H9 Z M13,1 H15 V14 H13 Z',
    cleanup: 'M6,0.5 H10 V2 M3,2 H13 V3.5 H3 Z M4,4 L12,4 L11,15 L5,15 Z M7,6 V13 M9,6 V13',
    diagnostics: 'M1,8 H4 L6,3 L8,13 L10,5 L11,8 H15',
    settings: 'M1,3 H15 M4,1.2 V4.8 M1,8 H15 M10,6.2 V9.8 M1,13 H15 M6,11.2 V14.8',
    about: 'M2,2 H14 V14 H2 Z M8,5 L8,5.15 M8,7.3 V11.3',
    activity: 'M14.5,8 A6.5,6.5 0 1 1 14.499,7.9 Z M8,4.3 V8.3 L11,10.3'
  };
  const NAV = [
    { id: 'dashboard', title: 'Dashboard', icon: 'dashboard' },
    { header: 'Tools' },
    { id: 'system', title: 'System', icon: 'system' },
    { id: 'network', title: 'Network', icon: 'network' },
    { id: 'cleanup', title: 'Cleanup', icon: 'cleanup' },
    { id: 'diagnostics', title: 'Diagnostics', icon: 'diagnostics' },
    { header: 'App' },
    { id: 'activity', title: 'Activity', icon: 'activity' },
    { id: 'settings', title: 'Settings', icon: 'settings' },
    { id: 'about', title: 'About', icon: 'about' }
  ];

  NAV.filter(n => n.id).forEach((n, i) => { n.key = i + 1; });

  // ---- page registry + router ----
  const reg = {}, inst = {}; let cur = null;
  W.pages = { register(id, def) { reg[id] = def; } };
  W.nav = {
    current: () => cur,
    go(id, opts) {
      const def = reg[id]; if (!def) { console.warn('no page', id); return; }
      const main = document.getElementById('wk-main');
      if (!inst[id]) {
        const root = ui.el('div', { class: 'wk-page' + (def.flush ? ' flush' : ''), id: 'page-' + id, 'aria-label': def.title || id });
        main.append(root); inst[id] = { root, api: null };
        try { inst[id].api = def.create(root) || {}; } catch (e) { console.error(e); root.textContent = 'This page failed to load: ' + e.message; inst[id].api = {}; if (W.reportError) W.reportError('page:' + id, e); }
      }
      if (cur && cur !== id) { const p = inst[cur]; p.root.classList.remove('on'); p.api.hide && p.api.hide(); }
      cur = id; inst[id].root.classList.add('on');
      document.querySelectorAll('.wk-nav-item').forEach(n => n.classList.toggle('on', n.dataset.id === id));
      document.body.classList.toggle('onboarding', id === 'onboarding');
      try { const r = inst[id].api.show && inst[id].api.show(opts || {}); if (r && r.catch) r.catch(e => console.error(e)); } catch (e) { console.error(e); }
    }
  };

  function buildSidebar() {
    const nav = document.getElementById('wk-nav');
    NAV.forEach(n => {
      if (n.header) { nav.append(ui.el('div', { class: 'wk-nav-head', text: n.header })); return; }
      const svg = `<svg viewBox="0 0 16 16" aria-hidden="true"><path d="${W.icons[n.icon]}"/></svg>`;
      nav.append(ui.el('div', { class: 'wk-nav-item', 'data-id': n.id, tabindex: 0, role: 'button', 'aria-label': n.title, onclick: () => W.nav.go(n.id), title: n.title + '  (Ctrl+' + n.key + ')',
        onkeydown: e => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); W.nav.go(n.id); } }, html: svg + '<span></span>' }));
      nav.lastChild.lastChild.textContent = n.title;
    });
  }

  function buildSidebarFooter() {
    const foot = document.getElementById('wk-side-foot'); if (!foot) return;
    const ver = ui.el('span', { text: 'v' + W.app.version });
    const admin = ui.el('span', { class: 'wk-badge ok', title: 'WinKit is running with administrator rights', text: 'Admin' });
    foot.replaceChildren(...[ver, W.admin && W.admin.isElevated() ? admin : null].filter(Boolean));
  }

  // Ctrl+1 .. Ctrl+8 jump straight to a page (ignored while a dialog is open or during first-run setup)
  function installShortcuts() {
    document.addEventListener('keydown', (e) => {
      if (!e.ctrlKey || e.altKey || e.shiftKey || e.metaKey || !/^[1-9]$/.test(e.key)) return;
      if (document.querySelector('.wk-overlay') || W.nav.current() === 'onboarding') return;
      const target = NAV.filter(n => n.id)[Number(e.key) - 1];
      if (target) { e.preventDefault(); W.nav.go(target.id); }
    });
  }

  W.boot = async function () {
    buildSidebar(); installShortcuts();
    let first = 'dashboard';
    try { const r = await W.ready; if (r && r.onboardingCompleted === false) first = 'onboarding'; } catch (e) { console.error(e); }
    try { buildSidebarFooter(); } catch (e) { console.error(e); }
    if (W.on) W.on('update:review', ({ result }) => { W.nav.go('about'); if (W.about && W.about.showPendingUpdate) W.about.showPendingUpdate(result); });
    W.nav.go(first);
  };
})();
