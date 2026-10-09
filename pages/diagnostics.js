/* pages/diagnostics.js - Diagnostics page (port of DiagnosticsView / DiagnosticsViewModel / DiagnosticCheckItemViewModel).
 * Commands: diag_run_all, diag_fix, diag_cancel.  Events: diag_check_started, diag_progress, diag_fix_progress, diag_fix_output.
 * The page auto-runs a full scan whenever it is opened (InitializeAsync -> RunAsync). */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;

  // ---- scoped CSS (prefix .dg-) ----
  const css = `
.dg-wrap{max-width:760px}
.dg-head{display:flex;align-items:flex-start;gap:16px;margin-bottom:20px;max-width:760px}
.dg-head .dg-titles{flex:1;min-width:0}
.dg-head .dg-sub{margin-top:4px}
.dg-btns{display:flex;gap:8px;flex:none}
.dg-btns button{padding:8px 16px}
.dg-card{margin-bottom:16px}
.dg-run{display:flex;flex-direction:column;gap:8px}
.dg-run-top{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.dg-sum{display:flex;align-items:center;gap:16px;flex-wrap:wrap}
.dg-sum .dg-sumtxt{flex:1;min-width:200px}
.dg-sum .dg-counts{margin-top:4px}
.dg-sumcard{border-left:3px solid var(--wk-border)}
.dg-sumcard.ok{border-left-color:var(--wk-success)}.dg-sumcard.warn{border-left-color:var(--wk-warning)}.dg-sumcard.bad{border-left-color:var(--wk-error)}
.dg-sumtxt>div:first-child{font-size:var(--wk-font-size-title);font-weight:600}
.dg-sum label{display:inline-flex;align-items:center;gap:8px;cursor:pointer;margin-right:14px}
.dg-sumctl{display:flex;align-items:center}
.dg-group{margin:18px 0 8px 2px;font-weight:600;color:var(--wk-text-muted);font-size:10.5px;letter-spacing:.07em;text-transform:uppercase}
.dg-item{display:flex;align-items:flex-start;gap:12px;margin-bottom:10px}
.dg-dot{width:9px;height:9px;border-radius:50%;margin-top:7px;flex:none;background:var(--c,var(--wk-text-muted))}
.dg-main{flex:1;min-width:0}
.dg-titlerow{display:flex;align-items:baseline;gap:10px;flex-wrap:wrap}
.dg-lbl{font-weight:600;font-size:10.5px;letter-spacing:.06em;padding:1px 9px;border-radius:99px;color:var(--c,var(--wk-text-muted));background:color-mix(in srgb,var(--c,var(--wk-text-muted)) 16%,transparent)}
.dg-sumry{margin-top:2px;word-break:break-word}
.dg-rec,.dg-msg{margin-top:6px}
.dg-item details{margin-top:8px;font-size:var(--wk-font-size-small)}
.dg-item summary{cursor:pointer;color:var(--wk-text-secondary);outline:none;width:max-content}
.dg-item summary:hover{color:var(--wk-text-primary)}
.dg-item summary:focus-visible{box-shadow:0 0 0 2px var(--wk-accent);border-radius:3px}
.dg-detail{margin:6px 0 0;font:12px/1.5 var(--wk-mono);color:var(--wk-text-muted);white-space:pre-wrap;word-break:break-word;user-select:text;-webkit-user-select:text}
.dg-act{flex:none;align-self:flex-start;padding:6px 12px}
.dg-log{margin-top:8px;max-height:120px;font-size:11.5px;padding:8px}
.dg-busy{margin-top:8px}
.dg-empty{padding:18px}
`;
  const style = el('style', { text: css, 'data-dg': '' });
  document.head.append(style);

  const COLORS = { success: 'var(--wk-success)', accent: 'var(--wk-accent)', warning: 'var(--wk-warning)', error: 'var(--wk-error)', muted: 'var(--wk-text-muted)' };
  const BY_STATUS = { Passed: 'success', Informational: 'accent', Warning: 'warning', Failed: 'error', NotApplicable: 'muted' };
  const LABEL = { Passed: 'PASSED', Informational: 'INFO', Warning: 'WARNING', Failed: 'FAILED', NotApplicable: 'N/A' };
  const PRIO = { Failed: 0, Warning: 1, Informational: 2, Passed: 3, NotApplicable: 4 };
  const msgOf = (e) => (e && e.message) ? String(e.message).replace(/^error:\s*/i, '') : String(e);

  WK.pages.register('diagnostics', {
    title: 'Diagnostics',
    create(root) {
      // ---- model ----
      let items = [];                 // {result, busy, statusMessage, open, el, parts, log:[]}
      let running = false, fixRunning = false, acting = false, hasRun = false, showOnlyIssues = false;
      let currentCheck = null, total = 11;
      const unsubs = [];

      const isIssue = (it) => it.result.status === 'Warning' || it.result.status === 'Failed';
      const count = (s) => items.filter((i) => i.result.status === s).length;
      const issueCount = () => count('Warning') + count('Failed');
      const prio = (it) => PRIO[it.result.status] != null ? PRIO[it.result.status] : (it.result.sortPriority != null ? it.result.sortPriority : 5);
      const colorOf = (r) => COLORS[r.statusColor] || COLORS[BY_STATUS[r.status]] || COLORS.muted;
      const labelOf = (r) => r.statusLabel || LABEL[r.status] || String(r.status || '');

      // ---- static DOM ----
      const runBtn = el('button', { class: 'primary', text: 'Run Diagnostics', onclick: () => run() });
      const cancelBtn = el('button', { text: 'Cancel', class: 'wk-hide', onclick: () => { WK.native('diag_cancel', { target: 'scan' }).catch(() => {}); } });
      const head = el('div', { class: 'dg-head' },
        el('div', { class: 'dg-titles' },
          el('h1', { class: 'wk-display', text: 'Diagnostics' }),
          el('div', { class: 'wk-secondary dg-sub', text: 'Analyze your system for configuration, integrity, performance, and maintenance issues.' })),
        el('div', { class: 'dg-btns' }, cancelBtn, runBtn));

      const runTitle = el('span', { text: 'Running diagnostics...' });
      const runCount = el('span', { class: 'wk-muted' });
      const runCur = el('div', { class: 'wk-muted' });
      const runBar = el('i');
      const runCard = el('div', { class: 'wk-card dg-card dg-wrap wk-hide' },
        el('div', { class: 'dg-run' },
          el('div', { class: 'dg-run-top' }, runTitle, runCount),
          el('div', { class: 'wk-progress' }, runBar), runCur));

      const sumText = el('div');
      const sumCounts = el('div', { class: 'wk-muted dg-counts' });
      const onlyBox = el('input', { type: 'checkbox', id: 'dg-only', onchange: () => { showOnlyIssues = onlyBox.checked; renderList(); } });
      const reviewBtn = el('button', { text: 'Review Issues', onclick: () => { showOnlyIssues = true; onlyBox.checked = true; renderList(); } });
      const sumCard = el('div', { class: 'wk-card dg-card dg-wrap dg-sumcard wk-hide' },
        el('div', { class: 'dg-sum' },
          el('div', { class: 'dg-sumtxt' }, sumText, sumCounts),
          el('div', { class: 'dg-sumctl' }, el('label', { for: 'dg-only' }, onlyBox, 'Show only issues'), reviewBtn)));

      const list = el('div', { class: 'dg-wrap' });
      root.append(head, runCard, sumCard, list);

      // ---- item cards ----
      function buildItem(result) {
        const it = { result, busy: false, statusMessage: null, open: false, log: [] };
        const dot = el('span', { class: 'dg-dot' });
        const lbl = el('span', { class: 'dg-lbl' });
        const sumry = el('div', { class: 'wk-secondary dg-sumry', text: result.summary || '' });
        const rec = el('div', { class: 'wk-muted dg-rec' });
        const msg = el('div', { class: 'wk-muted dg-msg wk-hide' });
        const busyBar = el('div', { class: 'wk-progress indet dg-busy wk-hide' }, el('i'));
        const logPre = el('pre', { class: 'wk-out dg-log wk-hide' });
        const detPre = el('pre', { class: 'dg-detail' });
        const det = el('details', { ontoggle: () => { it.open = det.open; } }, el('summary', { text: 'Technical details' }), detPre);
        const btn = el('button', { class: 'dg-act', onclick: () => onAct(it) });
        const card = el('div', { class: 'wk-card dg-item' }, dot,
          el('div', { class: 'dg-main' }, el('div', { class: 'dg-titlerow' }, el('span', { class: 'wk-title', text: result.name || result.checkId }), lbl), sumry, rec, msg, busyBar, logPre, det),
          btn);
        const c = colorOf(result);
        card.style.setProperty('--c', c); lbl.textContent = labelOf(result);
        rec.textContent = result.recommendedAction || ''; rec.classList.toggle('wk-hide', !result.recommendedAction);
        const td = result.technicalDetail ? String(result.technicalDetail).replace(/\r\n/g, '\n') : '';
        detPre.textContent = td; det.classList.toggle('wk-hide', !td);
        btn.classList.toggle('wk-hide', !result.fixAction);
        it.el = card; it.parts = { btn, msg, busyBar, logPre };
        updateItem(it);
        return it;
      }
      function updateItem(it) {
        const p = it.parts, a = it.result.fixAction;
        if (a) {
          p.btn.textContent = it.busy ? 'Cancel' : a.label;
          p.btn.disabled = it.busy ? false : (running || fixRunning);
        }
        p.msg.textContent = it.statusMessage || ''; p.msg.classList.toggle('wk-hide', !it.statusMessage);
        p.busyBar.classList.toggle('wk-hide', !it.busy);
        const showLog = it.busy && it.log.length > 0;
        p.logPre.classList.toggle('wk-hide', !showLog);
        if (showLog) { p.logPre.textContent = it.log.join('\n'); p.logPre.scrollTop = p.logPre.scrollHeight; }
      }
      const updateAllItems = () => items.forEach(updateItem);

      // ---- list / summary rendering ----
      function renderList() {
        const vis = items.filter((i) => !showOnlyIssues || isIssue(i));
        const sorted = vis.map((it, idx) => ({ it, idx })).sort((a, b) => prio(a.it) - prio(b.it) || a.idx - b.idx).map((x) => x.it);
        const groups = new Map();
        sorted.forEach((it) => { const g = it.result.categoryLabel || it.result.category || ''; if (!groups.has(g)) groups.set(g, []); groups.get(g).push(it); });
        const kids = [];
        groups.forEach((arr, g) => { kids.push(el('div', { class: 'dg-group', text: g })); arr.forEach((it) => kids.push(it.el)); });
        if (!kids.length && hasRun && !running && showOnlyIssues) kids.push(el('div', { class: 'wk-empty', text: 'No issues to show.' }));
        list.replaceChildren(...kids);
        renderChrome();
      }
      function renderChrome() {
        const showSummary = hasRun && !running;
        const issues = issueCount(), healthy = showSummary && issues === 0;
        sumCard.classList.toggle('wk-hide', !showSummary);
        sumCard.classList.toggle('ok', healthy); sumCard.classList.toggle('bad', issues > 0 && count('Failed') > 0); sumCard.classList.toggle('warn', issues > 0 && count('Failed') === 0);
        sumText.textContent = healthy ? 'No issues found. Your system is running normally.' : issues + (issues === 1 ? ' issue found' : ' issues found');
        sumCounts.textContent = count('Passed') + ' passed · ' + count('Warning') + ' warnings · ' + count('Failed') + ' failed';
        reviewBtn.classList.toggle('wk-hide', healthy);
        onlyBox.checked = showOnlyIssues;
        runCard.classList.toggle('wk-hide', !running);
        cancelBtn.classList.toggle('wk-hide', !running);
        runBtn.disabled = fixRunning;
        runCount.textContent = '(' + items.length + ' checks completed)';
        runBar.style.width = Math.min(100, Math.round(items.length / (total || 11) * 100)) + '%';
        runCur.textContent = currentCheck ? 'Checking ' + currentCheck + '...' : '';
        runCur.classList.toggle('wk-hide', !currentCheck);
      }

      // ---- scan ----
      function setResults(results) {
        const old = new Map(items.map((i) => [i.result.checkId, i]));
        items = (results || []).map((r) => { const o = old.get(r.checkId); return o && o.result === r ? o : buildItem(r); });
      }
      async function run() {
        if (running || fixRunning) return;
        running = true; items = []; currentCheck = null; updateAllItems(); renderList();
        let res = null, err = null;
        try { res = await WK.native('diag_run_all'); } catch (e) { err = e; }
        running = false; currentCheck = null;
        if (err) {
          renderList();
          WK.alert('Diagnostics couldn\'t run', msgOf(err));
          return;
        }
        if (res && Array.isArray(res.results)) setResults(res.results);
        const cancelled = !!(res && res.cancelled);
        if (cancelled) {
          WK.activity.record('Diagnostics scan cancelled', 'Info');
        } else {
          hasRun = true;
          const n = issueCount();
          if (n === 0) WK.activity.record('Diagnostics completed - no issues found', 'Success');
          else WK.activity.record('Diagnostics completed - ' + n + ' issue(s) found', 'Warning');
        }
        updateAllItems(); renderList();
      }

      // ---- actions ----
      async function onAct(it) {
        const action = it.result.fixAction;
        if (!action) return;
        if (it.busy) { WK.native('diag_cancel', { target: 'fix' }).catch(() => {}); return; }
        if (running || fixRunning || acting) return;
        acting = true;
        let ok;
        try {
          ok = await WK.confirm({
            title: action.label + ' - ' + it.result.name,
            message: (action.confirmationMessage || '') + (action.requiresElevation ? ' Windows may prompt for administrator permission.' : ''),
            confirmText: action.label, isDestructive: !!action.isDestructive
          });
        } finally { acting = false; }
        if (!ok) return;
        if (action.kind === 'Review') { navigateForReview(it.result); return; }

        fixRunning = true; it.busy = true; it.statusMessage = null; it.log = [];
        updateAllItems(); renderChrome();
        try {
          const r = await WK.native('diag_fix', { checkId: it.result.checkId });
          if (r && r.cancelled) {
            WK.activity.record(it.result.name + ': fix cancelled', 'Info');
          } else if (r && r.success) {
            const summary = r.message || 'Completed.';
            WK.activity.record(it.result.name + ': ' + summary, 'Success');
            WK.toast(it.result.name, summary, 'Success');
            if (r.note) WK.toast('Note', r.note, 'Warning');
          } else {
            WK.activity.record(it.result.name + ': fix failed', 'Error');
            await WK.alert(action.label + ' failed', (r && r.message) || 'Unknown error.', r && r.technicalDetail);
          }
        } catch (e) {
          WK.activity.record(it.result.name + ': fix failed', 'Error');
          await WK.alert(action.label + ' failed', msgOf(e));
        } finally {
          it.busy = false; it.statusMessage = null; it.log = []; fixRunning = false;
          updateAllItems(); renderChrome();
        }
        await run();
      }
      function navigateForReview(r) {
        const t = r.fixAction && r.fixAction.reviewTarget;
        const id = r.checkId;
        if (t === 'startup-manager' || id === 'startup') WK.nav.go('system', { tab: 'startup', tabTitle: 'Startup Manager' });
        else if (t === 'services-manager' || id === 'services') WK.nav.go('system', { tab: 'services', tabTitle: 'Services' });
        else if (t === 'cleanup' || id === 'disk-space') WK.nav.go('cleanup');
      }

      // ---- events ----
      const byId = (id) => items.find((i) => i.result.checkId === id);
      function subscribe() {
        unsubscribe();
        unsubs.push(
          WK.on('diag_check_started', (d) => { if (!running || !d) return; currentCheck = d.name; if (d.total) total = d.total; renderChrome(); }),
          WK.on('diag_progress', (d) => {
            if (!running || !d || !d.result) return;
            if (d.total) total = d.total;
            if (!byId(d.result.checkId)) items.push(buildItem(d.result));
            renderList();
          }),
          WK.on('diag_fix_progress', (d) => { const it = d && byId(d.checkId); if (it) { it.statusMessage = d.message || null; updateItem(it); } }),
          WK.on('diag_fix_output', (d) => {
            const it = d && byId(d.checkId); if (!it || !it.busy) return;
            it.log.push(String(d.line == null ? '' : d.line).replace(/[\r\n]+$/, ''));
            if (it.log.length > 200) it.log.splice(0, it.log.length - 200);
            updateItem(it);
          }),
          WK.on('diag_fix_finished', (d) => { const it = d && byId(d.checkId); if (it) { it.statusMessage = null; updateItem(it); } })
        );
      }
      function unsubscribe() { while (unsubs.length) { try { unsubs.pop()(); } catch (_) { /* ignore */ } } }

      renderList();
      return {
        show() {
          subscribe();
          if (!running && !fixRunning) return run();
          // scan/fix still in flight from an earlier visit: just redraw
          renderList();
        },
        hide() { unsubscribe(); }
      };
    }
  });
})();
