/* pages/cleanup.js - Cleanup page (port of CleanupView + CleanupViewModel + CleanupCategoryItemViewModel).
 * Scans on every visit (C# InitializeAsync), keeps the user's checkbox state across rescans, pre-selects everything except RecycleBin. */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;

  const css = `
.cln-wrap{display:flex;flex-direction:column;height:100%;min-height:380px}
.cln-sub{margin:4px 0 18px}
.cln-link{padding:3px 4px;border:0;background:transparent;color:var(--wk-accent);font-weight:400;font-size:inherit}
.cln-link:hover:not(:disabled){text-decoration:underline;background:transparent;border:0}
.cln-list{flex:1;min-height:0;overflow:auto;max-width:700px}
.cln-item{display:grid;grid-template-columns:auto 1fr auto;column-gap:14px;align-items:start;margin-bottom:10px;cursor:pointer}
.cln-item.off .cln-size{color:var(--wk-text-muted)}
.cln-item input{margin-top:3px}
.cln-name{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.cln-desc{margin:2px 0 4px}
.cln-loc{word-break:break-all}
.cln-sizebox{text-align:right;align-self:center;white-space:nowrap}
.cln-size{font-size:var(--wk-font-size-heading);font-weight:600}
.cln-bar{height:4px;width:96px;margin:6px 0 0 auto}
.cln-prog{margin-top:2px;color:var(--wk-accent);font-size:var(--wk-font-size-small)}
.cln-foot{max-width:700px;margin-top:16px;display:flex;align-items:center;gap:12px}
.cln-foot .cln-sel{flex:1;min-width:0}
.cln-foot button{padding:7px 14px}
.cln-foot button.destructive{padding:7px 16px}
.cln-status{max-width:700px;margin:0 0 10px;display:flex;align-items:center;gap:10px;min-height:20px}
.cln-status .wk-progress{flex:1}
`;
  document.head.appendChild(el('style', { 'data-for': 'cln', text: css }));

  const gb = (n) => String(Math.round((Number(n) || 0) * 100) / 100);   // .NET "0.##"
  const errText = (e) => (e && e.message) ? e.message : String(e);
  let opSeq = 0;
  const newOp = (p) => p + '-' + Date.now().toString(36) + '-' + (++opSeq);

  WK.pages.register('cleanup', {
    title: 'Cleanup',
    create(root) {
      let items = [];            // {scan, selected}
      let isScanning = false, isCleaning = false, rescanQueued = false, scanOp = null, elevated = null, lastSummary = '';
      const progress = {};       // category -> text while cleaning

      const list = el('div', { class: 'cln-list' });
      const statusText = el('span', { class: 'wk-secondary' });
      const bar = el('div', { class: 'wk-progress indet', style: 'max-width:200px' }, el('i'));
      const cancelScan = el('button', { class: 'sm', text: 'Cancel', onclick: () => { if (scanOp) WK.native('cleanup_cancel', { opId: scanOp }).catch(() => {}); } });
      const status = el('div', { class: 'cln-status wk-hide' }, el('span', { class: 'wk-spin' }), statusText, bar, cancelScan);
      const selLabel = el('span', { style: 'font-weight:600' });
      const sumLabel = el('div', { class: 'wk-muted', style: 'margin-top:2px' });
      const rescan = el('button', { text: 'Rescan', onclick: () => scan() });
      const clean = el('button', { class: 'destructive', text: 'Clean up', onclick: cleanSelected });
      const foot = el('div', { class: 'wk-card cln-foot' },
        el('div', { class: 'cln-sel' }, el('div', null, 'Selected: ', selLabel), sumLabel), rescan, clean);

      root.append(el('div', { class: 'cln-wrap' },
        el('h1', { class: 'wk-display', text: 'Cleanup' }),
        el('div', { class: 'wk-secondary cln-sub' }, 'Review what each category will remove before cleaning up. ', el('button', { class: 'cln-link', type: 'button', text: 'See what else is using space', onclick: () => WK.nav.go('system', { tab: 'disk' }) })),
        status, list, foot));

      const selected = () => items.filter(i => i.selected);
      function renderStatus(text) {
        const busy = isScanning || isCleaning;
        status.classList.toggle('wk-hide', !busy);
        cancelScan.classList.toggle('wk-hide', !isScanning || isCleaning);
        if (busy) statusText.textContent = text || (isCleaning ? 'Cleaning...' : 'Scanning...');
      }
      function renderFoot() {
        const sel = selected();
        selLabel.textContent = gb(sel.reduce((s, i) => s + (Number(i.scan.sizeGb) || 0), 0)) + ' GB';
        sumLabel.textContent = lastSummary;
        const busy = isScanning || isCleaning;
        rescan.disabled = busy; clean.disabled = busy || sel.length === 0;
      }
      function renderList() {
        if (!items.length) {
          list.replaceChildren(el('div', { class: 'wk-card wk-muted', text: isScanning ? 'Scanning your system...' : 'No cleanup categories found.' }));
          return;
        }
        const maxBytes = Math.max(1, ...items.map(i => Number(i.scan.sizeBytes) || 0));
        list.replaceChildren(...items.map(it => {
          const s = it.scan, cb = el('input', { type: 'checkbox', 'aria-label': s.displayName, checked: it.selected, disabled: isCleaning, onclick: (e) => e.stopPropagation(),
            onchange: () => { it.selected = cb.checked; card.classList.toggle('off', !it.selected); renderFoot(); } });
          const loc = (s.locations && s.locations[0]) || '';
          const badges = [];
          if (s.category === 'RecycleBin') badges.push(el('span', { class: 'wk-badge warn', text: 'Not selected by default' }));
          if (s.needsAdmin && elevated === false) badges.push(el('span', { class: 'wk-badge muted', title: 'Some files may be skipped unless WinKit runs as administrator.', text: 'Admin needed for protected files' }));
          const card = el('div', { class: 'wk-card cln-item' + (it.selected ? '' : ' off'), onclick: (e) => { if (isCleaning) return; cb.checked = !cb.checked; cb.dispatchEvent(new Event('change')); } },
            cb,
            el('div', { style: 'min-width:0' },
              el('div', { class: 'cln-name' }, el('span', { class: 'wk-title', text: s.displayName || s.category }), badges),
              el('div', { class: 'wk-secondary cln-desc', text: s.description || '' }),
              el('div', { class: 'wk-muted cln-loc', text: (s.fileCount == null ? 0 : s.fileCount) + ' items - ' + loc }),
              progress[s.category] ? el('div', { class: 'cln-prog', text: progress[s.category] }) : null),
            el('div', { class: 'cln-sizebox' }, el('div', { class: 'cln-size', text: gb(s.sizeGb) + ' GB' }), el('div', { class: 'wk-muted', text: WK.fmt.bytes(s.sizeBytes) }),
              el('div', { class: 'wk-progress cln-bar', 'aria-hidden': 'true' }, el('i', { style: 'width:' + Math.max(2, Math.round((Number(s.sizeBytes) || 0) / maxBytes * 100)) + '%' }))));
          return card;
        }));
      }
      const renderAll = (t) => { renderList(); renderFoot(); renderStatus(t); };

      async function scan() {
        if (isCleaning) return;
        if (isScanning) { rescanQueued = true; return; }       // a scan is still winding down (e.g. cancelled by leaving the page): run again after it
        isScanning = true; rescanQueued = false; const op = scanOp = newOp('cscan'); let n = 0;
        const off = WK.on('cleanup_scan_category', (d) => { if (d && d.opId === op) renderStatus('Scanning... ' + (++n) + ' categories done'); });
        renderAll();
        try {
          const res = await WK.native('cleanup_scan', { opId: op });
          if (res && !res.cancelled) {
            const prev = new Map(items.map(i => [i.scan.category, i.selected]));
            elevated = !!res.isElevated;
            items = (res.categories || []).map(sc => ({ scan: sc, selected: prev.has(sc.category) ? prev.get(sc.category) : (sc.category !== 'RecycleBin' && sc.defaultSelected !== false) }));
          }
        } catch (e) {
          WK.alert('Scan failed', errText(e));
        } finally { isScanning = false; scanOp = null; off(); renderAll(); }
        if (rescanQueued && !isCleaning) { rescanQueued = false; await scan(); }
      }

      async function cleanSelected() {
        const sel = selected();
        if (!sel.length || isScanning || isCleaning) return;
        const ok = await WK.confirm({
          title: 'Clean selected items',
          message: 'This will remove ' + gb(sel.reduce((s, i) => s + (Number(i.scan.sizeGb) || 0), 0)) + ' GB across ' + sel.length + ' categor' + (sel.length === 1 ? 'y' : 'ies') + ". This can't be undone.",
          confirmText: 'Clean up', isDestructive: true });
        if (!ok) return;
        isCleaning = true; for (const k in progress) delete progress[k];
        const op = newOp('crun');
        const offP = WK.on('cleanup_progress', (d) => { if (d && d.opId === op) { progress[d.category] = d.message; renderAll(d.message); } });
        const offD = WK.on('cleanup_category_done', (d) => { if (d && d.opId === op && d.result) {
          const r = d.result; progress[r.category] = 'Freed ' + WK.fmt.bytes(r.bytesFreed) + ' - ' + r.filesRemoved + ' removed' + (r.filesSkipped ? ', ' + r.filesSkipped + ' skipped' : ''); renderAll('Cleaning...'); } });
        renderAll('Cleaning...');
        try {
          const res = await WK.native('cleanup_run', { categories: sel.map(i => i.scan.category).join(','), opId: op });
          if (res && res.success) {
            const g = gb(res.summary && res.summary.gbFreed);
            WK.activity.record(g + ' GB of files removed', 'Success');
            WK.toast('Cleanup completed', g + ' GB of temporary files removed.', 'Success');
            lastSummary = 'Last cleanup: ' + WK.fmt.bytes(res.summary.bytesFreed) + ' freed, ' + res.summary.filesRemoved + ' removed' + (res.summary.filesSkipped ? ', ' + res.summary.filesSkipped + ' skipped' : '') + '.';
            if (res.userMessage) WK.toast('Some items were skipped', res.userMessage, 'Warning');
          } else {
            WK.alert('Cleanup failed', (res && res.userMessage) || 'Unknown error.', res && res.technicalDetail);
          }
          isCleaning = false; for (const k in progress) delete progress[k];
          await scan();
        } catch (e) {
          WK.alert('Cleanup failed', errText(e));
        } finally { offP(); offD(); isCleaning = false; for (const k in progress) delete progress[k]; renderAll(); }
      }

      renderAll();
      return {
        show() { scan(); },
        hide() { if (scanOp) WK.native('cleanup_cancel', { opId: scanOp }).catch(() => {}); },
      };
    },
  });
})();
