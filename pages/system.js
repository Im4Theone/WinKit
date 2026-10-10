/* pages/system.js - System tool (port of SystemToolsView + SystemTools/*View + view models).
 * Tabs (same order/names as SystemToolsViewModel, plus Disk usage from system_disk.js): Process Manager, Startup Manager, Services, Installed Applications,
 * Disk usage, Environment Variables, Hosts File, System Information.
 * Deep link: WK.nav.go('system', { tab: 'processes'|'startup'|'services'|'apps'|'disk'|'env'|'hosts'|'info' }).
 * Semantics copied from the C#: SystemToolsViewModel is transient, so every visit of the page starts on the first tab
 * (or the deep-linked one), every tab (re)loads the first time it is selected in a visit, the 1 s process refresh timer
 * only runs while the page is visible (and not frozen), the uptime text ticks once per second.
 * Backend: API_system.md.  Everything is self-contained here; sub-tab factories are also published on WK._systemTabs.
 */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;

  // ---- scoped CSS (prefix .sys-) ----
  const css = `
.sys-bar{display:flex;align-items:center;gap:10px;margin:0 0 12px}
.sys-bar .sys-grow{flex:1;min-width:0}
.sys-bar button{padding:7px 14px}
.sys-bar .wk-search input{width:250px}
.sys-t td button.sm,.sys-row button.sm{background:transparent;border-color:transparent;color:var(--wk-text-secondary)}
.sys-t tbody tr:hover td button.sm:not(:disabled),.sys-t tr.sel td button.sm:not(:disabled){border-color:var(--wk-border);color:var(--wk-text-primary)}
.sys-t tbody tr:hover td button.sm.destructive:not(:disabled),.sys-t tr.sel td button.sm.destructive:not(:disabled){border-color:rgba(var(--wk-error-rgb),.5);color:var(--wk-error)}
.sys-t tbody tr:hover td button.sm.destructive:hover:not(:disabled){background:rgba(var(--wk-error-rgb),.12)}
.sys-scroll{overflow:auto;max-height:max(200px,calc(100vh - 270px))}
.sys-scroll.sys-env{max-height:max(200px,calc(100vh - 340px))}
table.sys-t{table-layout:fixed;width:100%}
table.sys-t td,table.sys-t th{padding-left:10px;padding-right:10px}
table.sys-t td{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
table.sys-t th{user-select:none}
table.sys-t tbody tr{cursor:default}
.sys-t td .sm,.sys-row .sm{padding:4px 10px}
.sys-t td .sm+.sm{margin-left:6px}
.sys-lock button[data-act]{opacity:.45;pointer-events:none}
.sys-st-run{color:var(--wk-success)}
.sys-st-stop{color:var(--wk-text-muted)}
.sys-st-busy{color:var(--wk-warning)}
.sys-pulse{display:inline-block;width:26px;height:4px;border-radius:2px;background:var(--wk-accent);margin-left:8px;vertical-align:middle;animation:sys-pulse .6s ease-in-out infinite alternate}
@keyframes sys-pulse{from{opacity:.35}to{opacity:1}}
.sys-row{display:flex;align-items:center;gap:12px;padding:12px 16px;border-bottom:1px solid var(--wk-border)}
.sys-row:last-child{border-bottom:0}
.sys-row .sys-main{flex:1;min-width:0}
.sys-row .sys-main>div{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sys-env-form{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.sys-env-form input[type=text]{padding:6px 8px}
.sys-env-form select{padding:6px 8px;width:100px}
.sys-hosts{display:block;width:100%;height:max(280px,calc(100vh - 300px));border:0;border-radius:0;padding:14px;resize:none;background:transparent;white-space:pre;overflow:auto;font-family:var(--wk-mono);font-size:13px;user-select:text}
.sys-link{cursor:pointer}
.sys-link:hover{color:var(--wk-accent);text-decoration:underline}
.sys-info{max-width:640px}
.sys-info .sys-ir{display:flex;margin-bottom:12px}
.sys-info .sys-ir:last-child{margin-bottom:0}
.sys-info .sys-il{width:180px;flex:none;color:var(--wk-text-muted);font-size:var(--wk-font-size-small);padding-top:1px}
.sys-info .sys-iv{flex:1;min-width:0;word-break:break-word}
`;
  const st = document.createElement('style'); st.textContent = css; document.head.appendChild(st);

  // ---- helpers ----
  const cmp = (a, b) => a < b ? -1 : a > b ? 1 : 0;
  const strCmp = (a, b) => String(a == null ? '' : a).localeCompare(String(b == null ? '' : b), undefined, { sensitivity: 'base' });
  const numCmp = (a, b) => cmp(a == null ? -Infinity : a, b == null ? -Infinity : b);
  const fix1 = n => (Math.round(Number(n) * 10) / 10).toFixed(1);
  const trim0 = n => { const s = fix1(n); return s.endsWith('.0') ? s.slice(0, -2) : s; };
  const errMsg = e => (e && e.message) ? e.message : String(e);

  /* Shows the standard error dialog for a failed OpResult (C# dialogService.ShowError). Returns true on success. */
  async function opOk(r, title) {
    if (r && r.success) return true;
    await WK.alert(title, (r && r.message) || 'Unknown error.', (r && r.detail) || undefined);
    return false;
  }
  async function call(cmd, args, failTitle) {  // bridge-level failures (error:<msg>) -> error dialog, returns undefined
    try { return await WK.native(cmd, args); }
    catch (e) { await WK.alert(failTitle, errMsg(e)); return undefined; }
  }

  /* Keyed, sortable table. cols: [{key,title,width?,val?(row)->sortable,cmp?,text?(row)->string,node?(row,get)->Node,sig?(row)->string,cls?,sortable?:false}]
     Rows are reused by key so a 1 s refresh does not destroy buttons the user is clicking. Header click: asc, then toggles (arrow ↑/↓ like GridViewSort). */
  function makeTable(cols, keyOf, opts) {
    opts = opts || {};
    const table = el('table', { class: 'wk-table sys-t' });
    const colgroup = el('colgroup', null, cols.map(c => el('col', c.width ? { style: 'width:' + c.width + 'px' } : null)));
    const minW = cols.reduce((s, c) => s + (c.width || 100), 0);
    table.style.minWidth = minW + 'px';
    const thead = el('thead'), tbody = el('tbody');
    table.append(colgroup, thead, tbody);
    let rows = [], sortKey = opts.sortKey || null, dir = opts.sortDir || 1, selKey = null, flt = null, fltEmpty = ''; const trs = new Map(); let emptyTr = null, emptyText = opts.empty || '';
    function head() {
      thead.replaceChildren(el('tr', null, cols.map(c => {
        const sortable = c.sortable !== false && c.title !== '';
        return el('th', { 'data-sort': sortable ? c.key : null, onclick: sortable ? () => { if (sortKey === c.key) dir = -dir; else { sortKey = c.key; dir = 1; } head(); paint(); } : null },
          c.title, sortKey === c.key ? el('span', { class: 'arr', text: dir > 0 ? '↑' : '↓' }) : null);
      })));
    }
    function sorted() {
      const out = (flt ? rows.filter(flt) : rows).slice(), c = cols.find(x => x.key === sortKey);
      if (c) { const f = c.cmp || ((a, b) => cmp(a, b)); out.sort((a, b) => dir * f(c.val ? c.val(a) : a[c.key], c.val ? c.val(b) : b[c.key])); }
      return out;
    }
    function fillCell(td, c, row, tr) {
      const s = c.sig ? c.sig(row) : String(c.text(row));
      if (td._s === s) return; td._s = s;
      if (c.node) { td.replaceChildren(c.node(row, () => tr._row)); }
      else { const t = c.text(row); td.textContent = t; td.title = t; }
      if (c.tcls) td.className = ((c.cls || '') + ' ' + (c.tcls(row) || '')).trim();       // optional per-row cell class (e.g. status colour)
    }
    function paint() {
      const list = sorted(), seen = new Set();
      list.forEach(row => {
        const k = keyOf(row); seen.add(k); let tr = trs.get(k);
        if (!tr) {
          const pick = () => { selKey = tr._k; [...tbody.children].forEach(x => x.classList.toggle('sel', x === tr)); };
          tr = el('tr', { onclick: pick });
          if (opts.menu) WK.ui.contextMenu(tr, () => { pick(); return opts.menu(tr._row); });
          tr._k = k; tr._cells = cols.map(c => { const td = el('td', { class: (c.cls || '') }); tr.append(td); return td; }); trs.set(k, tr);
        }
        tr._row = row; tr.classList.toggle('sel', k === selKey);
        cols.forEach((c, i) => fillCell(tr._cells[i], c, row, tr));
      });
      for (const [k, tr] of trs) if (!seen.has(k)) { tr.remove(); trs.delete(k); }
      const want = list.map(r => trs.get(keyOf(r)));
      if (emptyTr) { emptyTr.remove(); emptyTr = null; }
      let i = 0; for (const tr of want) { if (tbody.children[i] !== tr) tbody.insertBefore(tr, tbody.children[i] || null); i++; }
      const msg = flt ? fltEmpty : emptyText;
      if (!list.length && msg) { emptyTr = el('tr', null, el('td', { colspan: cols.length, class: 'wk-empty', text: msg })); emptyTr.firstChild.style.whiteSpace = 'normal'; tbody.append(emptyTr); }
    }
    head();
    return { el: table, set(r) { rows = r; paint(); }, setEmpty(t) { emptyText = t; paint(); },
      setFilter(fn, emptyMsg) { flt = fn || null; fltEmpty = emptyMsg || ''; paint(); }, shown: () => (flt ? rows.filter(flt).length : rows.length), repaint() { trs.forEach(tr => tr._cells.forEach(td => { td._s = null; })); paint(); }, rows: () => rows };
  }
  const card = (inner, extra) => el('div', { class: 'wk-card flat ' + (extra || '') }, inner);
  const scroller = (table, extra) => el('div', { class: 'sys-scroll ' + (extra || '') }, table.el || table);
  const refreshBtn = (fn) => { const b = el('button', { text: 'Refresh' }); b.addEventListener('click', () => WK.ui.busy(b, fn)); return b; };
  const bar = (left, right) => el('div', { class: 'sys-bar' }, el('div', { class: 'sys-grow wk-secondary' }, left), right);
  const toolbar = (search, left, ...right) => el('div', { class: 'sys-bar' }, search.el, el('div', { class: 'sys-grow wk-secondary' }, left), ...right);
  const plural = (n, one, many) => n + ' ' + (n === 1 ? one : many);

  const factories = {};
  WK._systemTabs = factories;

  // =====================================================================================================
  // Process Manager
  // =====================================================================================================
  factories.processes = function () {
    let list = [], timer = null, refreshing = false, frozen = false, visible = false, tabOn = false, locked = false;
    const count = el('span', { text: '0 processes' });
    let query = '';
    const search = WK.ui.search(q => { query = q; applyFilter(); }, 'Search processes');
    const freezeBtn = el('button', { text: 'Freeze', onclick: toggleFreeze });
    const wrap = el('div');
    const tbl = makeTable([
      { key: 'name', title: 'Name', width: 130, text: r => r.name, cmp: strCmp },
      { key: 'id', title: 'PID', width: 70, text: r => r.id, cmp: numCmp },
      { key: 'cpuPercent', title: 'CPU', width: 66, text: r => Number(r.cpuPercent).toFixed(1) + '%', val: r => r.cpuPercent, cmp: numCmp },
      { key: 'workingSetBytes', title: 'Memory', width: 88, text: r => WK.fmt.bytes(r.workingSetBytes), val: r => r.workingSetBytes, cmp: numCmp },
      { key: 'filePath', title: 'Path', text: r => r.filePath || '', cmp: strCmp, sig: r => r.filePath || '',
        node: r => r.filePath ? el('span', { class: 'sys-link', title: 'Show in File Explorer', onclick: () => WK.shell.reveal(r.filePath).catch(e => WK.alert("Couldn't open the folder", errMsg(e))), text: r.filePath }) : document.createTextNode('') },
      { key: 'end', title: '', width: 70, sortable: false, sig: r => 'e' + !!r.canBeEnded + '|' + r.name,
        node: (r, get) => el('button', { class: 'sm destructive', 'data-act': 1, text: 'End', disabled: !r.canBeEnded, title: r.canBeEnded ? null : "This process can't be ended", onclick: () => endProcess(get()) }) },
    ], r => r.id, { empty: 'Loading…', sortKey: 'workingSetBytes', sortDir: -1, menu: r => [
      { label: 'Copy name', run: () => WK.ui.copy(r.name) }, { label: 'Copy PID', run: () => WK.ui.copy(r.id) },
      { label: 'Copy path', disabled: !r.filePath, run: () => WK.ui.copy(r.filePath) }, { sep: true },
      { label: 'Show in File Explorer', disabled: !r.filePath, run: () => WK.shell.reveal(r.filePath).catch(e => WK.alert("Couldn't open the folder", errMsg(e))) },
      { label: 'Search online for "' + r.name + '"', run: () => WK.ui.searchWeb(r.name + ' process') }, { sep: true },
      { label: 'End process', danger: true, disabled: !r.canBeEnded, title: r.canBeEnded ? null : "This process can't be ended", run: () => endProcess(r) }] });           // biggest memory users first, like Task Manager
    wrap.append(card(scroller(tbl)));
    const root = el('div', null, toolbar(search, count, freezeBtn), wrap);
    const countText = () => (query ? tbl.shown() + ' of ' + list.length : list.length) + ' processes';
    function applyFilter() {
      tbl.setFilter(query ? r => WK.ui.matches(r.name + ' ' + r.id + ' ' + (r.filePath || ''), query) : null, 'No processes match "' + query + '".');
      count.textContent = countText();
    }

    async function refresh() {
      if (refreshing) return; refreshing = true;
      try {
        list = await WK.native('proc_list');
        if (!Array.isArray(list)) list = [];
        tbl.setEmpty('No processes.'); tbl.set(list); count.textContent = countText();
      } catch (e) { console.error('proc_list failed', e); }
      finally { refreshing = false; }
    }
    function updateTimer() {
      if (timer) { clearInterval(timer); timer = null; }
      if (visible && tabOn && !frozen) timer = setInterval(() => { if (!document.hidden) refresh(); }, 1000);      // only poll while this tab is actually on screen
    }
    async function toggleFreeze() {
      frozen = !frozen; freezeBtn.textContent = frozen ? 'Unfreeze' : 'Freeze';
      updateTimer(); if (!frozen) await refresh();
    }
    async function endProcess(p) {
      if (locked) return;
      const ok = await WK.confirm({ title: 'End process', message: 'End "' + p.name + '" (PID ' + p.id + ')? Unsaved work in this process will be lost.', confirmText: 'End process', isDestructive: true });
      if (!ok) return;
      locked = true; wrap.classList.add('sys-lock');
      try {
        let r; try { r = await WK.native('proc_end', { pid: p.id }); } catch (e) { r = { success: false, message: errMsg(e) }; }
        await opOk(r, "Couldn't end process");
        if (!frozen) await refresh();
      } finally { locked = false; wrap.classList.remove('sys-lock'); }
    }
    return {
      el: root, load: refresh,
      pageShow() { visible = true; frozen = false; freezeBtn.textContent = 'Freeze'; updateTimer(); },   // new VM per visit
      pageHide() { visible = false; updateTimer(); },
      tabShow() { tabOn = true; updateTimer(); },
      tabHide() { tabOn = false; updateTimer(); },
    };
  };

  // =====================================================================================================
  // Startup Manager
  // =====================================================================================================
  factories.startup = function () {
    const list = el('div'); const holder = card(el('div', { class: 'sys-scroll' }, list));
    let busy = false, data = [], query = '';
    const search = WK.ui.search(q => { query = q; renderList(); }, 'Search startup apps');
    const root = el('div', null, toolbar(search, 'Apps that launch automatically when you sign in', refreshBtn(load)), holder);
    list.append(el('div', { class: 'wk-empty', text: 'Loading…' }));
    async function load() {
      let r; try { r = await WK.native('startup_list'); } catch (e) { await WK.alert("Couldn't load startup items", errMsg(e)); return; }
      data = Array.isArray(r) ? r : []; renderList();
    }
    function renderList() {
      const shown = data.filter(e => WK.ui.matches(e.name + ' ' + e.command, query));
      list.replaceChildren(...(shown.length ? shown.map(row) : [el('div', { class: 'wk-empty', text: data.length ? 'No startup apps match "' + query + '".' : 'No startup items.' })]));
    }
    const exeOf = (c) => { c = String(c || '').trim(); const m = c.match(/^"([^"]+)"/) || c.match(/^(.+?\.(?:exe|bat|cmd|com|lnk))(?:\s|$)/i); return m ? m[1] : c; };
    function row(e) {
      const sw = el('input', { type: 'checkbox', class: 'wk-switch', checked: e.isEnabled, 'aria-label': (e.isEnabled ? 'Disable ' : 'Enable ') + e.name });
      sw.addEventListener('change', async () => {
        if (busy) { sw.checked = e.isEnabled; return; }
        busy = true; sw.disabled = true;
        try {
          let res; try { res = await WK.native('startup_set', { scope: e.scope, name: e.name, command: e.command, enabled: !e.isEnabled }); } catch (x) { res = { success: false, message: errMsg(x) }; }
          await opOk(res, "Couldn't change startup item");
        } finally { busy = false; }
        await load();
      });
      const rowEl = el('div', { class: 'sys-row', tabindex: 0 },
        el('div', { class: 'sys-main' }, el('div', { text: e.name }), el('div', { class: 'wk-muted sys-link', text: e.command, title: 'Show in File Explorer', onclick: () => WK.shell.reveal(exeOf(e.command)).catch(x => WK.alert("Couldn't open the folder", errMsg(x))) })),
        e.requiresElevationToChange ? el('span', { class: 'wk-badge muted', title: 'Changing this item needs administrator rights', text: 'Admin' }) : null, sw);
      WK.ui.contextMenu(rowEl, () => [
        { label: 'Copy name', run: () => WK.ui.copy(e.name) }, { label: 'Copy command', run: () => WK.ui.copy(e.command) }, { sep: true },
        { label: 'Show in File Explorer', run: () => WK.shell.reveal(exeOf(e.command)).catch(x => WK.alert("Couldn't open the folder", errMsg(x))) },
        { label: 'Search online for "' + e.name + '"', run: () => WK.ui.searchWeb(e.name + ' startup') }, { sep: true },
        { label: e.isEnabled ? 'Turn off at startup' : 'Turn on at startup', disabled: sw.disabled, run: () => sw.click() }]);
      return rowEl;
    }
    return { el: root, load };
  };

  // =====================================================================================================
  // Services
  // =====================================================================================================
  const STATUS_ORDER = ['Stopped', 'StartPending', 'StopPending', 'Running', 'ContinuePending', 'PausePending', 'Paused'];
  const STARTUP_ORDER = ['Boot', 'System', 'Automatic', 'Manual', 'Disabled', 'Unknown'];
  const STATUS_TEXT = { StartPending: 'Starting', StopPending: 'Stopping', ContinuePending: 'Resuming', PausePending: 'Pausing' };
  const statusText = s => STATUS_TEXT[s] || s;
  factories.services = function () {
    let list = [], locked = false, query = ''; const count = el('span', { text: '0 services' }), wrap = el('div');
    const search = WK.ui.search(q => { query = q; applyFilter(); }, 'Search services');
    const tbl = makeTable([
      { key: 'displayName', title: 'Name', text: r => r.displayName, cmp: strCmp },
      { key: 'status', title: 'Status', width: 90, text: r => statusText(r.status), tcls: r => r.status === 'Running' ? 'sys-st-run' : r.status === 'Stopped' ? 'sys-st-stop' : 'sys-st-busy', val: r => STATUS_ORDER.indexOf(r.status), cmp: numCmp },
      { key: 'startupType', title: 'Startup type', width: 100, text: r => r.startupType, val: r => STARTUP_ORDER.indexOf(r.startupType), cmp: numCmp },
      { key: 'act', title: '', width: 128, sortable: false, sig: r => 'a' + !!r.canStart + !!r.canStop + r.displayName,
        node: (r, get) => el('span', null,
          el('button', { class: 'sm', 'data-act': 1, text: 'Start', disabled: !r.canStart, onclick: () => start(get()) }),
          el('button', { class: 'sm destructive', 'data-act': 1, text: 'Stop', disabled: !r.canStop, onclick: () => stop(get()) })) },
    ], r => r.name, { empty: 'Loading…', menu: r => [
      { label: 'Copy name', run: () => WK.ui.copy(r.displayName) }, { label: 'Copy service name', run: () => WK.ui.copy(r.name) }, { sep: true },
      { label: 'Start', disabled: !r.canStart, run: () => start(r) }, { label: 'Stop', danger: true, disabled: !r.canStop, run: () => stop(r) }] });
    wrap.append(card(scroller(tbl)));
    async function load() {
      let r; try { r = await WK.native('svc_list'); } catch (e) { await WK.alert("Couldn't load services", errMsg(e)); return; }
      list = Array.isArray(r) ? r : []; tbl.setEmpty('No services.'); tbl.set(list); count.textContent = countText();
    }
    const countText = () => (query ? tbl.shown() + ' of ' + list.length : list.length) + ' services';
    function applyFilter() {
      tbl.setFilter(query ? r => WK.ui.matches(r.displayName + ' ' + r.name + ' ' + statusText(r.status) + ' ' + r.startupType, query) : null, 'No services match "' + query + '".');
      count.textContent = countText();
    }
    async function run(fn) { if (locked) return; locked = true; wrap.classList.add('sys-lock'); try { await fn(); await load(); } finally { locked = false; wrap.classList.remove('sys-lock'); } }
    const start = s => run(async () => {
      let r; try { r = await WK.native('svc_start', { name: s.name }); } catch (e) { r = { success: false, message: errMsg(e) }; }
      await opOk(r, "Couldn't start service");
    });
    const stop = async s => {
      if (locked) return;
      const ok = await WK.confirm({ title: 'Stop service', message: 'Stop "' + s.displayName + '"? Some features of Windows or other apps may stop working until it\'s started again.', confirmText: 'Stop service', isDestructive: true });
      if (!ok) return;
      await run(async () => {
        let r; try { r = await WK.native('svc_stop', { name: s.name }); } catch (e) { r = { success: false, message: errMsg(e) }; }
        await opOk(r, "Couldn't stop service");
      });
    };
    return { el: el('div', null, toolbar(search, count, refreshBtn(load)), wrap), load };
  };

  // =====================================================================================================
  // Installed applications
  // =====================================================================================================
  factories.apps = function () {
    let list = [], locked = false, query = ''; const uninstalling = new Set(); const count = el('span', { text: '0 applications' }), wrap = el('div');
    const search = WK.ui.search(q => { query = q; applyFilter(); }, 'Search applications');
    const tbl = makeTable([
      { key: 'name', title: 'Name', text: r => r.name, cmp: strCmp },
      { key: 'publisher', title: 'Publisher', width: 128, text: r => r.publisher || '', cmp: strCmp },
      { key: 'version', title: 'Version', width: 112, text: r => r.version || '', cmp: strCmp },
      { key: 'estimatedSizeMb', title: 'Size', width: 84, text: r => WK.fmt.megabytes(r.estimatedSizeMb), val: r => r.estimatedSizeMb, cmp: numCmp },
      { key: 'act', title: '', width: 100, sortable: false, sig: r => 'u' + uninstalling.has(r.name) + r.name,
        node: (r, get) => { const b = uninstalling.has(r.name);
          return el('span', null, el('button', { class: 'sm destructive', 'data-act': 1, text: 'Uninstall', disabled: b, onclick: () => uninstall(get()) }), b ? el('span', { class: 'sys-pulse' }) : null); } },
    ], r => r.name, { empty: 'Loading…', menu: r => [
      { label: 'Copy name', run: () => WK.ui.copy(r.name) }, { label: 'Search online for "' + r.name + '"', run: () => WK.ui.searchWeb(r.name) }, { sep: true },
      { label: 'Uninstall', danger: true, disabled: uninstalling.has(r.name), run: () => uninstall(r) }] });
    wrap.append(card(scroller(tbl)));
    async function load() {
      let r; try { r = await WK.native('apps_list'); } catch (e) { await WK.alert("Couldn't load installed applications", errMsg(e)); return; }
      list = Array.isArray(r) ? r : []; tbl.setEmpty('No applications found.'); tbl.set(list); count.textContent = countText();
    }
    const countText = () => (query ? tbl.shown() + ' of ' + list.length : list.length) + ' applications';
    function applyFilter() {
      tbl.setFilter(query ? r => WK.ui.matches(r.name + ' ' + (r.publisher || '') + ' ' + (r.version || ''), query) : null, 'No applications match "' + query + '".');
      count.textContent = countText();
    }
    async function uninstall(a) {
      if (locked || uninstalling.has(a.name)) return;
      const ok = await WK.confirm({ title: 'Uninstall application', message: 'This opens the uninstaller for "' + a.name + '". WinKit doesn\'t remove anything itself \u2014 you\'ll finish the uninstall in the app\'s own wizard.', confirmText: 'Continue', isDestructive: true });
      if (!ok) return;
      uninstalling.add(a.name); tbl.repaint();
      try {
        let r; try { r = await WK.native('apps_uninstall', { name: a.name }); } catch (e) { r = { success: false, message: errMsg(e) }; }
        if (!(await opOk(r, "Couldn't start uninstaller"))) return;
        await new Promise(res => setTimeout(res, 2000));   // C# holds the busy pulse for 2 s after a successful launch
      } finally { uninstalling.delete(a.name); tbl.repaint(); }
    }
    return { el: el('div', null, toolbar(search, count, refreshBtn(load)), wrap), load };
  };

  // =====================================================================================================
  // Environment variables
  // =====================================================================================================
  factories.env = function () {
    let list = [], locked = false, query = ''; const wrap = el('div'), count = el('span', { text: '0 variables' });
    const search = WK.ui.search(q => { query = q; applyFilter(); }, 'Search variables');
    const name = el('input', { type: 'text', placeholder: 'Name', title: 'Variable name', style: 'width:180px', 'aria-label': 'Variable name' });
    const value = el('input', { type: 'text', placeholder: 'Value', title: 'Value', style: 'width:260px', 'aria-label': 'Value' });
    const scope = el('select', { 'aria-label': 'Scope' }, el('option', { value: 'User', text: 'User' }), el('option', { value: 'Machine', text: 'Machine' }));
    const add = el('button', { class: 'primary', text: 'Add', onclick: addVar });
    const form = el('div', { class: 'wk-card sys-env-form', style: 'margin-bottom:12px' }, name, value, scope, add);
    const tbl = makeTable([
      { key: 'scope', title: 'Scope', width: 80, text: r => r.scope, val: r => r.scope === 'User' ? 0 : 1, cmp: numCmp },
      { key: 'name', title: 'Name', width: 170, text: r => r.name, cmp: strCmp },
      { key: 'value', title: 'Value', text: r => r.value, cmp: strCmp },
      { key: 'act', title: '', width: 84, sortable: false, sig: r => 'd' + r.name,
        node: (r, get) => el('button', { class: 'sm destructive', 'data-act': 1, text: 'Delete', onclick: () => del(get()) }) },
    ], r => r.scope + '\u0000' + r.name, { empty: 'Loading…', menu: r => [
      { label: 'Copy name', run: () => WK.ui.copy(r.name) }, { label: 'Copy value', run: () => WK.ui.copy(r.value) }, { sep: true },
      { label: 'Delete', danger: true, run: () => del(r) }] });
    wrap.append(card(scroller(tbl, 'sys-env')));
    async function load() {
      let r; try { r = await WK.native('env_list'); } catch (e) { await WK.alert("Couldn't load environment variables", errMsg(e)); return; }
      list = Array.isArray(r) ? r : []; tbl.setEmpty('No environment variables.'); tbl.set(list); count.textContent = countText();
    }
    const countText = () => (query ? tbl.shown() + ' of ' + list.length : list.length) + ' variables';
    function applyFilter() {
      tbl.setFilter(query ? r => WK.ui.matches(r.scope + ' ' + r.name + ' ' + r.value, query) : null, 'No variables match "' + query + '".');
      count.textContent = countText();
    }
    async function addVar() {
      const nm = name.value.trim();
      if (!nm) return;                                     // C#: silently ignores a blank name
      if (!value.value) { name.focus(); await WK.alert('Enter a value', 'Windows does not keep environment variables with an empty value. Type a value for "' + nm + '", or use Delete on the list to remove a variable.'); return; }
      if (locked) return; locked = true; wrap.classList.add('sys-lock');
      try {
        let r; try { r = await WK.native('env_set', { scope: scope.value, name: nm, value: value.value }); } catch (e) { r = { success: false, message: errMsg(e) }; }
        if (!(await opOk(r, "Couldn't set variable"))) return;
        name.value = ''; value.value = ''; await load();
      } finally { locked = false; wrap.classList.remove('sys-lock'); }
    }
    async function del(v) {
      if (locked) return;
      const ok = await WK.confirm({ title: 'Delete environment variable', message: 'Delete "' + v.name + '"? Applications that rely on it may stop working correctly.', confirmText: 'Delete', isDestructive: true });
      if (!ok) return;
      locked = true; wrap.classList.add('sys-lock');
      try {
        let r; try { r = await WK.native('env_delete', { scope: v.scope, name: v.name }); } catch (e) { r = { success: false, message: errMsg(e) }; }
        await opOk(r, "Couldn't delete variable"); await load();
      } finally { locked = false; wrap.classList.remove('sys-lock'); }
    }
    return { el: el('div', null, form, toolbar(search, count), wrap), load };
  };

  // =====================================================================================================
  // Hosts file
  // =====================================================================================================
  factories.hosts = function () {
    const path = el('div', { class: 'wk-secondary selectable', text: 'C:\\Windows\\System32\\drivers\\etc\\hosts' });
    const ta = el('textarea', { class: 'sys-hosts', spellcheck: 'false', wrap: 'off', 'aria-label': 'Hosts file contents' });
    ta.addEventListener('keydown', e => {   // AcceptsTab="True"
      if (e.key === 'Tab' && !e.ctrlKey && !e.altKey && !e.shiftKey) { e.preventDefault(); document.execCommand('insertText', false, '\t'); }
    });
    const reload = el('button', { text: 'Reload', style: 'padding:7px 14px;margin-right:8px' });
    reload.addEventListener('click', () => WK.ui.busy(reload, load));
    const save = el('button', { class: 'primary', text: 'Save', style: 'padding:7px 14px' });
    save.addEventListener('click', () => WK.ui.busy(save, doSave));
    const root = el('div', null,
      el('div', { class: 'sys-bar' },
        el('div', { class: 'sys-grow' }, path, el('div', { class: 'wk-muted', text: 'Editing requires administrator access. Changes take effect immediately.' })),
        el('div', null, reload, save)),
      card(ta));
    async function load() {
      let r; try { r = await WK.native('hosts_get'); } catch (e) { await WK.alert("Couldn't load hosts file", errMsg(e)); return; }
      if (r && r.path) path.textContent = r.path;
      ta.value = (r && r.content) || '';
    }
    async function doSave() {
      const ok = await WK.confirm({ title: 'Save hosts file', message: "This changes how your computer resolves domain names to IP addresses system-wide. Only continue if you're sure about these changes.", confirmText: 'Save', isDestructive: true });
      if (!ok) return;
      const content = ta.value.replace(/\r?\n/g, '\r\n');   // WPF TextBox yields CRLF
      let r; try { r = await WK.native('hosts_save', { content }); } catch (e) { r = { success: false, message: errMsg(e) }; }
      if (!(await opOk(r, "Couldn't save hosts file"))) return;
      WK.toast('Hosts file saved', null, 'Success');
    }
    return { el: root, load };
  };

  // =====================================================================================================
  // System information
  // =====================================================================================================
  factories.info = function () {
    const rows = [['Machine name', 'machine'], ['Architecture', 'arch'], ['Windows version', 'win'], ['Processor', 'cpu'], ['Logical cores', 'cores'],
      ['Memory', 'mem'], ['GPU', 'gpu'], ['Network adapter', 'nic'], ['IP address', 'ip'], ['System uptime', 'up']];
    const vals = {};
    const box = el('div', { class: 'wk-card sys-info' }, rows.map(([l, k]) => el('div', { class: 'sys-ir' }, el('div', { class: 'sys-il', text: l }), vals[k] = el('div', { class: 'sys-iv selectable', 'data-copy': '', title: 'Click to copy' }))));
    let timer = null, baseMs = null, baseAt = 0, visible = false;
    function tick() { if (baseMs != null) vals.up.textContent = WK.fmt.uptimeMs(baseMs + (performance.now() - baseAt)); }
    function setUptime(ms) { baseMs = Number(ms) || 0; baseAt = performance.now(); tick(); }
    function updateTimer() { if (timer) { clearInterval(timer); timer = null; } if (visible) timer = setInterval(tick, 1000); }
    async function load() {
      let r; try { r = await WK.native('sysinfo_get'); } catch (e) { await WK.alert("Couldn't load system information", errMsg(e)); return; }
      if (!r) return;
      const s = (k, v) => { vals[k].textContent = v == null ? '' : String(v); };
      s('machine', r.machineName); s('arch', r.architecture); s('win', r.windowsVersion);
      s('cpu', r.cpu && r.cpu.name); s('cores', r.cpu && r.cpu.coreCount);
      s('mem', r.memory && trim0(r.memory.totalGb) + ' GB'); s('gpu', r.gpu && r.gpu.name);
      s('nic', r.network && r.network.adapterName); s('ip', r.network && r.network.ipAddress);
      setUptime(r.uptimeMs);
    }
    const refresh = el('button', { text: 'Refresh', style: 'padding:7px 14px' }); refresh.addEventListener('click', () => WK.ui.busy(refresh, load));
    const copyAll = el('button', { text: 'Copy all', style: 'padding:7px 14px;margin-right:8px', title: 'Copy these details, for example to paste into a support request' });
    copyAll.addEventListener('click', async () => { if (await WK.shell.copy(rows.map(([l, k]) => l + ': ' + vals[k].textContent).join('\n'))) WK.toast('Copied', 'System details are on your clipboard.', 'Success'); });
    return {
      el: el('div', null, el('div', { style: 'display:flex;justify-content:flex-end;margin-bottom:12px' }, copyAll, refresh), box), load,
      async pageShow() { visible = true; updateTimer(); try { const u = await WK.native('sysinfo_uptime'); if (u) setUptime(u.uptimeMs); } catch (_) { /* ticker starts after sysinfo_get */ } },
      pageHide() { visible = false; updateTimer(); },
    };
  };

  // =====================================================================================================
  // Page
  // =====================================================================================================
  const TABS = [
    { id: 'processes', title: 'Processes' },
    { id: 'startup', title: 'Startup' },
    { id: 'services', title: 'Services' },
    { id: 'apps', title: 'Applications' },
    { id: 'disk', title: 'Disk usage' },
    { id: 'env', title: 'Environment' },
    { id: 'hosts', title: 'Hosts file' },
    { id: 'info', title: 'System info' },
  ];
  const ALIAS = { process: 'processes', proc: 'processes', 'process-manager': 'processes', startup_manager: 'startup', 'startup-manager': 'startup', service: 'services',
    installed: 'apps', applications: 'apps', 'installed-apps': 'apps', environment: 'env', environment_variables: 'env', envvars: 'env', variables: 'env',
    disk_usage: 'disk', 'disk-usage': 'disk', diskusage: 'disk', storage: 'disk', space: 'disk', hostsfile: 'hosts', 'hosts-file': 'hosts', sysinfo: 'info', information: 'info', 'system-information': 'info', system_information: 'info' };

  WK.pages.register('system', {
    title: 'System',
    create(root) {
      const ctl = {}, loaded = new Set(); let tabs = null, visible = false;
      // C# builds all view models up front; the process timer runs for the whole visit, so create that controller eagerly.
      ctl.processes = factories.processes();
      const defs = TABS.map(t => ({
        id: t.id, title: t.title,
        create(panel) {
          const c = ctl[t.id] || (ctl[t.id] = factories[t.id]());
          panel.append(c.el);
          if (visible && c.pageShow) c.pageShow();
          return {
            show() {
              c.tabShow && c.tabShow();
              if (!loaded.has(t.id)) { loaded.add(t.id); const p = c.load && c.load(); if (p && p.catch) p.catch(e => console.error(e)); }
            },
            hide() { c.tabHide && c.tabHide(); },
          };
        },
      }));
      tabs = WK.ui.tabsPage(root, 'System', defs, 'Processes, startup items, services, installed apps and system settings.');
      return {
        show(opts) {
          visible = true; loaded.clear();
          Object.values(ctl).forEach(c => c.pageShow && c.pageShow());
          let id = opts && opts.tab; id = ALIAS[id] || id;
          tabs.select(TABS.some(t => t.id === id) ? id : TABS[0].id);
        },
        hide() { visible = false; tabs.hide(); Object.values(ctl).forEach(c => c.pageHide && c.pageHide()); },
      };
    },
  });
})();
