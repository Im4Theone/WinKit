/* pages/system_disk.js - "Disk usage" tab of the System page: pick a drive (or your user folder), see what takes the space and click a folder to look inside.
 * Backend: disk_drives, disk_scan, disk_cancel (API_disk.md). Read-only: nothing on this tab deletes or changes a file.
 * Loaded after system.js, which owns the tab list and publishes its tab factories on WK._systemTabs. */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;

  const css = `
.dk-drives{display:grid;grid-template-columns:repeat(auto-fill,minmax(236px,1fr));gap:12px;margin-bottom:6px}
.dk-drive{display:flex;flex-direction:column;align-items:stretch;gap:9px;padding:14px 16px;text-align:left;font-weight:400;height:auto}
.dk-dh{display:flex;align-items:baseline;justify-content:space-between;gap:8px}
.dk-dh b{font-size:var(--wk-font-size-title);font-weight:600}
.dk-drive .wk-progress{height:5px}
.dk-lead{margin:4px 0 14px}
.dk-crumbs{display:flex;flex-wrap:wrap;align-items:center;gap:1px;min-width:0}
.dk-crumbs button{padding:3px 7px;background:transparent;border-color:transparent;color:var(--wk-text-secondary);font-weight:400}
.dk-crumbs button:hover:not(:disabled){color:var(--wk-text-primary);background:rgba(var(--wk-text-primary-rgb),.06)}
.dk-crumbs .cur{padding:3px 7px;font-weight:600}
.dk-crumbs .sep{color:var(--wk-text-muted)}
.dk-sum{display:flex;align-items:baseline;gap:10px;margin:2px 0 12px}
.dk-sum b{font-size:var(--wk-font-size-heading);font-weight:600}
.dk-list{padding:0;overflow:hidden}
.dk-scroll{max-height:max(200px,calc(100vh - 360px));overflow:auto}
.dk-row{display:grid;grid-template-columns:20px minmax(0,1fr) minmax(70px,150px) 78px 40px 52px;column-gap:12px;align-items:center;padding:8px 14px;border-bottom:1px solid rgba(var(--wk-border-rgb),.7);cursor:pointer}
.dk-row:last-child{border-bottom:0}
.dk-row:hover,.dk-row:focus-visible{background:rgba(var(--wk-text-primary-rgb),.045)}
.dk-row.file{cursor:default}
.dk-row.link{cursor:default;color:var(--wk-text-muted)}
.dk-ic{width:18px;height:18px;color:var(--wk-text-muted)}
.dk-row.dir .dk-ic{color:var(--wk-accent)}
.dk-ic svg{width:18px;height:18px;fill:none;stroke:currentColor;stroke-width:1.3;stroke-linecap:round;stroke-linejoin:round;display:block}
.dk-name{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.dk-bar{height:6px;border-radius:3px;background:rgba(var(--wk-text-primary-rgb),.1);overflow:hidden}
.dk-bar i{display:block;height:100%;background:var(--wk-accent);border-radius:3px}
.dk-size{text-align:right;font-variant-numeric:tabular-nums}
.dk-pct{text-align:right;font-variant-numeric:tabular-nums}
.dk-show{opacity:0;padding:3px 8px}
.dk-row:hover .dk-show,.dk-row:focus-within .dk-show,.dk-row:focus-visible .dk-show{opacity:1}
.dk-note{margin:10px 2px 0}
.dk-scan{display:flex;align-items:center;gap:12px;padding:16px 18px}
.dk-scan .wk-progress{width:160px;flex:none}
.dk-scan .dk-st{flex:1;min-width:0}
.dk-scan .dk-st div:first-child{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
`;
  document.head.appendChild(el('style', { 'data-for': 'dk', text: css }));

  const ICON_D = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M1.8 4.2c0-.7.5-1.2 1.2-1.2h2.9l1.5 1.7H13c.7 0 1.2.5 1.2 1.2v6.1c0 .7-.5 1.2-1.2 1.2H3c-.7 0-1.2-.5-1.2-1.2z"/></svg>';
  const ICON_F = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M4 1.8h5l3 3v8.4c0 .6-.4 1-1 1H4c-.6 0-1-.4-1-1V2.8c0-.6.4-1 1-1z"/><path d="M9 1.8v3h3"/></svg>';
  const ICON_L = '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="M6.5 9.5l3-3M5 7.5L3.6 9a2.1 2.1 0 003 3L8 10.5M11 8.5L12.4 7a2.1 2.1 0 00-3-3L8 5.5"/></svg>';
  const errMsg = (e) => (e && e.message) ? e.message : String(e);
  const num = (n) => (Number(n) || 0).toLocaleString();
  let opSeq = 0;
  const TYPE_TEXT = { Fixed: 'Local disk', Removable: 'Removable drive', Network: 'Network drive' };

  function ago(ms) {
    const s = Math.round((Date.now() - ms) / 1000);
    return s < 20 ? 'just now' : s < 90 ? 'a minute ago' : s < 3600 ? Math.round(s / 60) + ' minutes ago' : Math.round(s / 3600) + ' hours ago';
  }

  WK._systemTabs.disk = function () {
    const root = el('div'), body = el('div');
    root.append(body);
    const cache = new Map();                     // path -> { res, at }  (so going back up is instant; Rescan refreshes)
    let drives = [], cur = null, op = null, busy = false, homeLoaded = false;

    // ---------------------------------------------------------------- home: drives
    function usedClass(p) { return p > 95 ? ' bad' : p > 85 ? ' warn' : ''; }
    function driveCard(d) {
      const used = d.totalBytes - d.freeBytes, pct = d.totalBytes ? Math.round(used * 100 / d.totalBytes) : 0;
      return el('button', { class: 'wk-card dk-drive', type: 'button', title: 'See what is using space on ' + d.letter, onclick: () => open(d.path) },
        el('div', { class: 'dk-dh' }, el('b', { text: d.label ? d.label + ' (' + d.letter + ')' : d.letter }), el('span', { class: 'wk-muted', text: (TYPE_TEXT[d.type] || d.type) + (d.fileSystem ? ' · ' + d.fileSystem : '') })),
        el('div', { class: 'wk-progress' + usedClass(pct) }, el('i', { style: 'width:' + pct + '%' })),
        el('div', { class: 'wk-muted', text: WK.fmt.bytes(d.freeBytes) + ' free of ' + WK.fmt.bytes(d.totalBytes) }));
    }
    function renderHome() {
      cur = null;
      const userCard = el('button', { class: 'wk-card dk-drive', type: 'button', title: 'Scan your user folder', onclick: () => open('@user') },
        el('div', { class: 'dk-dh' }, el('b', { text: 'Your user folder' }), el('span', { class: 'wk-muted', text: 'Quick scan' })),
        el('div', { class: 'wk-muted', text: 'Documents, Downloads, Desktop, AppData and the rest of your files.' }));
      body.replaceChildren(
        el('div', { class: 'wk-secondary dk-lead', text: 'See what is taking up space. Pick a drive or folder, then click a folder to look inside.' }),
        el('div', { class: 'dk-drives' }, userCard, ...drives.map(driveCard)),
        el('div', { class: 'wk-muted dk-note', text: 'Disk usage only reads your files. Nothing is changed or deleted here.' }));
    }
    async function load() {
      try { const r = await WK.native('disk_drives'); drives = Array.isArray(r) ? r : []; }
      catch (e) { drives = []; WK.alert("Couldn't list your drives", errMsg(e)); }
      homeLoaded = true; if (!cur && !busy) renderHome();
    }

    // ---------------------------------------------------------------- scanning
    function renderScanning(path) {
      const st = el('div', null, el('div', { text: 'Scanning ' + (path === '@user' ? 'your user folder' : path) + '...' }), el('div', { class: 'wk-muted', text: 'Starting...' }));
      const cancel = el('button', { class: 'sm', text: 'Cancel', onclick: () => { if (op) WK.native('disk_cancel', { opId: op }).catch(() => {}); } });
      const bar = el('div', { class: 'wk-progress indet' }, el('i'));
      const box = el('div', { class: 'wk-card flat dk-scan' }, el('span', { class: 'wk-spin' }), el('div', { class: 'dk-st' }, st.firstChild, st.lastChild), bar, cancel);
      body.replaceChildren(box);
      return (d) => { const msg = box.querySelector('.dk-st .wk-muted'); if (msg) msg.textContent = num(d.files) + ' files · ' + WK.fmt.bytes(d.bytes) + (d.total ? ' · ' + d.done + ' of ' + d.total + ' folders' : ''); };
    }
    async function open(path, force) {
      if (busy) return;
      const hit = !force && cache.get(path);
      if (hit) { show(hit.res, hit.at); return; }
      busy = true; const opId = op = 'disk-' + Date.now().toString(36) + '-' + (++opSeq);
      const update = renderScanning(path), off = WK.on('disk_progress', (d) => { if (d && d.opId === opId) update(d); });
      let res = null, failed = null;
      try { res = await WK.native('disk_scan', { path, opId }); }
      catch (e) { failed = e; }
      finally { busy = false; op = null; off(); }
      if (res && !res.cancelled && res.path) { const at = Date.now(); cache.set(res.path, { res, at }); cache.set(path, { res, at }); show(res, at); }
      else if (cur) show(cur.res, cur.at); else renderHome();               // cancelled or failed: stay where you were
      if (failed) WK.alert("Couldn't scan this folder", errMsg(failed));
    }

    // ---------------------------------------------------------------- a scanned folder
    function crumbs(path) {
      const parts = path.replace(/\\$/, '').split('\\'), out = [];
      out.push(el('button', { class: 'sm', text: 'This PC', onclick: () => { if (!busy) renderHome(); } }));
      let acc = '';
      parts.forEach((p, i) => {
        acc += (i ? '\\' : '') + p; const last = i === parts.length - 1, target = i === 0 ? acc + '\\' : acc;
        out.push(el('span', { class: 'sep', text: '›' }));
        out.push(last ? el('span', { class: 'cur', text: p }) : el('button', { class: 'sm', text: p, onclick: () => open(target) }));
      });
      return out;
    }
    function show(res, at) {
      cur = { res, at };
      const base = res.path.endsWith('\\') ? res.path : res.path + '\\', items = res.items || [], top = items.length ? Math.max(1, items[0].bytes) : 1;
      const reveal = (p) => WK.shell.reveal(p).catch((e) => WK.alert("Couldn't open that location", errMsg(e)));
      const rows = items.map((it) => {
        const full = base + it.name, kind = it.link ? 'link' : it.isDir ? 'dir' : 'file';
        const act = () => { if (kind === 'dir') open(full); };
        const pct = res.totalBytes ? it.bytes * 100 / res.totalBytes : 0;
        const rowEl = el('div', { class: 'dk-row ' + kind, tabindex: 0, role: kind === 'dir' ? 'button' : 'row', title: kind === 'link' ? 'A shortcut to another folder. It is not counted here.' : kind === 'dir' ? 'Open ' + it.name + ' (' + num(it.files) + ' files)' : it.name,
          onclick: act, onkeydown: (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); act(); } } },
          el('span', { class: 'dk-ic', html: kind === 'dir' ? ICON_D : kind === 'link' ? ICON_L : ICON_F }),
          el('span', { class: 'dk-name', text: it.name }),
          el('span', { class: 'dk-bar' }, el('i', { style: 'width:' + Math.max(it.bytes ? 1 : 0, Math.round(it.bytes * 100 / top)) + '%' })),
          el('span', { class: 'dk-size', text: kind === 'link' ? 'link' : WK.fmt.bytes(it.bytes) }),
          el('span', { class: 'dk-pct wk-muted', text: kind === 'link' ? '' : pct >= 1 ? Math.round(pct) + '%' : it.bytes ? '<1%' : '' }),
          el('button', { class: 'sm dk-show', type: 'button', text: 'Show', title: 'Show in File Explorer', onclick: (e) => { e.stopPropagation(); reveal(full); } }));
        WK.ui.contextMenu(rowEl, () => [{ label: 'Open', disabled: kind !== 'dir', run: act }, { label: 'Show in File Explorer', run: () => reveal(full) }, { sep: true },
          { label: 'Copy path', run: () => WK.ui.copy(full) }, { label: 'Copy name', run: () => WK.ui.copy(it.name) }, { label: 'Copy size', disabled: kind === 'link', run: () => WK.ui.copy(WK.fmt.bytes(it.bytes)) }]);
        return rowEl;
      });
      const notes = [];
      if (res.itemCount > items.length) notes.push('Showing the largest ' + items.length + ' of ' + num(res.itemCount) + ' items.');
      if (res.unreadable) notes.push(num(res.unreadable) + (res.unreadable === 1 ? ' folder' : ' folders') + " couldn't be read and " + (res.unreadable === 1 ? 'is' : 'are') + ' not counted. Run WinKit as administrator to include ' + (res.unreadable === 1 ? 'it' : 'them') + '.');
      notes.push('Sizes are file sizes. Online-only OneDrive files, shortcuts and junctions are not counted.');
      const rescan = el('button', { text: 'Rescan', onclick: () => open(res.path, true) });
      const showBtn = el('button', { text: 'Show in Explorer', onclick: () => reveal(res.path) });
      const up = res.parent ? el('button', { text: 'Up', title: 'Go to ' + res.parent, onclick: () => open(res.parent) }) : null;
      body.replaceChildren(
        el('div', { class: 'sys-bar' }, el('div', { class: 'sys-grow dk-crumbs' }, ...crumbs(res.path)), up, showBtn, rescan),
        el('div', { class: 'dk-sum' }, el('b', { text: WK.fmt.bytes(res.totalBytes) }), el('span', { class: 'wk-secondary', text: 'in ' + num(res.fileCount) + ' files' }), el('span', { class: 'wk-muted', text: 'Scanned ' + ago(at) })),
        el('div', { class: 'wk-card flat dk-list' }, el('div', { class: 'dk-scroll' }, rows.length ? rows : el('div', { class: 'wk-empty', text: 'This folder is empty.' }))),
        ...notes.map((n) => el('div', { class: 'wk-muted dk-note', text: n })));
    }

    renderHome();
    return {
      el: root, load,
      tabHide() { if (op) WK.native('disk_cancel', { opId: op }).catch(() => {}); },
      pageHide() { if (op) WK.native('disk_cancel', { opId: op }).catch(() => {}); },
    };
  };
})();
