/* pages/dashboard.js - Dashboard page (port of DashboardView / DashboardViewModel).
 * Commands: sysinfo_get (every 5 s while visible), sysinfo_uptime, diag_summary (never triggers a scan), disk_drives + hw_health (drives and battery, at most every 30 s).
 * Events: diag_summary_changed, activity:changed, profile:changed. Timers run only while the page is visible. */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;

  const css = `
.db-wrap{max-width:1040px}
.db-status{margin:10px 0 30px}
.db-h{margin:0 0 12px}
.db-h.sp{margin-top:30px}
.db-cards{display:grid;grid-template-columns:repeat(auto-fill,minmax(224px,1fr));gap:14px}
.db-ov{min-width:0;display:flex;flex-direction:column;gap:2px}
.db-lbl{color:var(--wk-text-muted);font-size:var(--wk-font-size-small);font-weight:600}
.db-ov .db-big{font-size:calc(var(--wk-font-size-heading) + 7px);font-weight:600;letter-spacing:-.02em;line-height:1.25;margin-top:6px;font-variant-numeric:tabular-nums}
.db-ov .db-t{font-size:var(--wk-font-size-title);font-weight:600;margin-top:6px;line-height:1.35}
.db-ov .db-sec{color:var(--wk-text-secondary);font-size:var(--wk-font-size-small)}
.db-meter{margin:8px 0 6px}
.db-hw{grid-template-columns:repeat(auto-fill,minmax(300px,1fr));margin-top:14px}
.db-dr{display:grid;grid-template-columns:34px minmax(0,1fr) 92px;column-gap:10px;align-items:center;margin-top:9px}
.db-dr .wk-progress{height:5px;margin:0}
.db-dr .db-dl{font-weight:600}
.db-dr .wk-muted{text-align:right;white-space:nowrap}
.db-ov.link{cursor:pointer}.db-ov.link:hover{border-color:var(--wk-text-muted)}
.db-sec.warn{color:var(--wk-warning)}
.db-bhealth{margin-top:6px}
.db-ell{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.db-wrapt{word-break:break-word}
.db-qa{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}
@media (max-width:760px){.db-qa{grid-template-columns:minmax(0,1fr)}}
.db-qa .wk-card{outline:none;display:flex;align-items:flex-start;gap:14px}
.db-qa .wk-card:focus-visible{box-shadow:0 0 0 2px var(--wk-background),0 0 0 4px var(--wk-accent)}
.db-ic{width:38px;height:38px;border-radius:calc(var(--wk-radius) + 1px);background:rgba(var(--wk-accent-rgb),.14);display:flex;align-items:center;justify-content:center;flex:none}
.db-ic svg{width:18px;height:18px;fill:none;stroke:var(--wk-accent);stroke-width:1.4;stroke-linecap:round;stroke-linejoin:round;display:block}
.db-qa .db-qt{font-size:var(--wk-font-size-title);font-weight:600;line-height:1.3}
.db-qa .db-qd{color:var(--wk-text-secondary);margin-top:2px}
.db-act{padding:0;overflow:hidden}
.db-row{display:flex;align-items:center;gap:12px;padding:11px 18px;border-bottom:1px solid rgba(var(--wk-border-rgb),.7)}
.db-row:last-child{border-bottom:0}
.db-row i{width:7px;height:7px;border-radius:50%;background:var(--wk-text-muted);flex:none}
.db-row i.Success{background:var(--wk-success)}.db-row i.Warning{background:var(--wk-warning)}.db-row i.Error{background:var(--wk-error)}
.db-row .db-title{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.db-none{padding:18px}
.db-more{border-top:1px solid var(--wk-border);padding:10px 18px}
`;
  document.head.append(el('style', { text: css, 'data-db': '' }));

  const REFRESH_MS = 5000, TICK_MS = 1000, RECENT = 5;
  const pct = (n) => (typeof n === 'number' && isFinite(n)) ? Math.round(n) + '%' : '—';
  const g1 = (n) => { const s = Number(n).toFixed(1); return s.endsWith('.0') ? s.slice(0, -2) : s; };
  const timeOf = (e) => { const d = new Date(e.timestamp); if (isNaN(d.getTime())) return ''; try { return d.toLocaleTimeString([], { timeStyle: 'short' }); } catch (_) { return d.toLocaleTimeString(); } };

  WK.pages.register('dashboard', {
    title: 'Dashboard',
    create(root) {
      let overview = null, inFlight = false, disposed = false;
      let uptimeBase = null, uptimeAt = 0;
      let refreshTimer = null, tickTimer = null;
      const unsubs = [];

      // ---- greeting / status ----
      const greet = el('h1', { class: 'wk-display' });
      const stxt = el('span', { text: "Diagnostics haven't been run yet." });
      const status = el('div', { class: 'wk-pill' }, el('i'), stxt);

      function updateGreeting() {
        let name = ''; try { name = (WK.profile.current() || {}).name || ''; } catch (_) { /* ignore */ }
        const h = new Date().getHours();
        const period = h >= 5 && h < 12 ? 'Good morning' : h >= 12 && h < 18 ? 'Good afternoon' : h >= 18 && h < 23 ? 'Good evening' : 'Hello, night owl';
        greet.textContent = name.trim() === '' ? period + '!' : period + ', ' + name;
      }
      function applySummary(s) {
        // hasRun:false -> neutral. Mapping identical to DashboardViewModel.UpdateDiagnosticsStatus.
        let healthy = null, msg = "Diagnostics haven't been run yet.";
        if (s && s.hasRun && typeof s.issueCount === 'number') {
          healthy = s.issueCount === 0;
          msg = s.issueCount === 0 ? 'Your system is running normally.' : s.issueCount === 1 ? '1 issue needs your attention.' : s.issueCount + ' issues need your attention.';
        }
        status.className = 'wk-pill' + (healthy === true ? ' ok' : healthy === false ? ' warn' : '');
        stxt.textContent = msg;
      }
      async function loadSummary() { try { applySummary(await WK.native('diag_summary')); } catch (_) { applySummary(null); } }

      // ---- overview cards ----
      const v = {};
      const mk = (cls, key) => (v[key] = el('div', { class: cls }));
      const meter = (key) => { const bar = el('i'); v[key] = el('div', { class: 'wk-progress db-meter', role: 'presentation' }, bar); v[key].bar = bar; return v[key]; };
      const card = (label, ...body) => el('div', { class: 'wk-card db-ov' }, el('div', { class: 'db-lbl', text: label }), ...body);
      const hwHead = el('h2', { class: 'wk-eyebrow db-h sp wk-hide', text: 'Hardware health' });
      const cards = el('div', { class: 'db-cards' },
        card('CPU', mk('db-big', 'cpuPct'), meter('cpuBar'), mk('db-sec db-ell', 'cpuName')),
        card('Memory', mk('db-big', 'memPct'), meter('memBar'), mk('db-sec', 'memTxt')),
        card('Storage', mk('db-big', 'stPct'), meter('stBar'), mk('db-sec', 'stFree')),
        card('GPU', mk('db-t db-wrapt', 'gpu')),
        card('Network', mk('db-t', 'net'), mk('db-sec db-ell', 'adapter')),
        card('Windows', mk('db-t db-wrapt', 'win'), mk('db-sec', 'uptime')));

      // ---- hardware health: drives and (on laptops) the battery ----
      const hwCards = el('div', { class: 'db-cards db-hw' });
      let hwAt = 0, hwBusy = false;
      const fmtMin = (m) => Math.floor(m / 60) + ' h ' + (m % 60) + ' min';
      function drivesCard(list) {
        const rows = list.slice().sort((a, b) => (a.type === 'Fixed' ? 0 : 1) - (b.type === 'Fixed' ? 0 : 1)).slice(0, 5).map((d) => {
          const used = d.totalBytes ? Math.round((d.totalBytes - d.freeBytes) * 100 / d.totalBytes) : 0;
          return el('div', { class: 'db-dr', title: (d.label ? d.label + ' \u00b7 ' : '') + d.fileSystem + ' \u00b7 ' + WK.fmt.bytes(d.totalBytes) + ' in total' },
            el('span', { class: 'db-dl', text: d.letter }), el('div', { class: 'wk-progress' + (used >= 95 ? ' bad' : used >= 85 ? ' warn' : '') }, el('i', { style: 'width:' + used + '%' })), el('span', { class: 'wk-muted', text: WK.fmt.bytes(d.freeBytes) + ' free' }));
        });
        const full = list.filter((d) => d.totalBytes && (d.totalBytes - d.freeBytes) * 100 / d.totalBytes >= 90);
        return el('div', { class: 'wk-card db-ov link', tabindex: 0, role: 'button', title: 'See what is using the space', onclick: () => WK.nav.go('system', { tab: 'disk' }),
          onkeydown: (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); WK.nav.go('system', { tab: 'disk' }); } } },
          el('div', { class: 'db-lbl', text: 'Drives' }), ...rows,
          full.length ? el('div', { class: 'db-sec warn', style: 'margin-top:8px', text: full.map((d) => d.letter).join(', ') + (full.length === 1 ? ' is' : ' are') + ' almost full. Click to see what is using the space.' }) : null);
      }
      function batteryCard(b) {
        const p = b.percent, low = p != null && !b.onAc && p <= 20, crit = p != null && !b.onAc && p <= 10;
        const state = b.charging ? 'Charging' : b.onAc ? 'Plugged in' : 'On battery' + (b.remainingMinutes != null ? ' \u00b7 about ' + fmtMin(b.remainingMinutes) + ' left' : '');
        const bar = el('i', { style: 'width:' + (p == null ? 0 : p) + '%' });
        let hl = null;
        if (b.wearPercent != null) { const keep = Math.round(100 - b.wearPercent); hl = el('div', { class: 'db-sec db-bhealth' + (b.health === 'Worn' ? ' warn' : ''), title: b.fullMwh + ' of ' + b.designMwh + ' mWh' + (b.cycleCount ? ' \u00b7 ' + b.cycleCount + ' charge cycles' : ''), text: 'Health: ' + b.health + ' \u00b7 holds ' + keep + '% of its original charge' }); }
        return el('div', { class: 'wk-card db-ov' }, el('div', { class: 'db-lbl', text: 'Battery' }), el('div', { class: 'db-big', text: p == null ? '\u2014' : p + '%' }),
          el('div', { class: 'wk-progress db-meter' + (crit ? ' bad' : low ? ' warn' : ''), role: 'presentation' }, bar), el('div', { class: 'db-sec', text: state }), hl);
      }
      async function loadHardware() {
        if (hwBusy) return; hwBusy = true; hwAt = Date.now();
        try {
          const [drives, hw] = await Promise.all([WK.native('disk_drives').catch(() => null), WK.native('hw_health').catch(() => null)]);
          if (disposed) return;
          const cards = [];
          if (Array.isArray(drives) && drives.length) cards.push(drivesCard(drives));
          if (hw && hw.battery && hw.battery.present) cards.push(batteryCard(hw.battery));
          hwCards.replaceChildren(...cards); hwHead.classList.toggle('wk-hide', !cards.length); hwCards.classList.toggle('wk-hide', !cards.length);
        } finally { hwBusy = false; }
      }

      function setMeter(key, value) {
        const m = v[key], ok = typeof value === 'number' && isFinite(value);
        m.bar.style.width = ok ? Math.max(0, Math.min(100, value)) + '%' : '0%';
        m.classList.toggle('warn', ok && value >= 75 && value < 90);
        m.classList.toggle('bad', ok && value >= 90);
      }
      function renderOverview() {
        const o = overview;
        const st = o && Array.isArray(o.storage) && o.storage.length ? o.storage[0] : null;
        v.cpuPct.textContent = o && o.cpu ? pct(o.cpu.usagePercent) : '—';
        v.cpuName.textContent = o && o.cpu ? (o.cpu.name || '') : ''; v.cpuName.title = v.cpuName.textContent;
        v.memPct.textContent = o && o.memory ? pct(o.memory.usagePercent) : '—';
        v.memTxt.textContent = o && o.memory ? g1(o.memory.usedGb) + ' of ' + g1(o.memory.totalGb) + ' GB in use' : '';
        v.gpu.textContent = o && o.gpu ? (o.gpu.name || '') : '—'; v.gpu.title = v.gpu.textContent;
        v.stPct.textContent = st ? pct(st.usagePercent) : '—';
        v.stFree.textContent = st ? Math.round(st.freeGb) + ' GB free on ' + (st.name || 'C:') : '';
        v.net.textContent = o && o.network ? (o.network.isConnected ? 'Connected' : 'Not connected') : '—';
        v.adapter.textContent = o && o.network ? (o.network.adapterName || '') + (o.network.ipAddress ? ' · ' + o.network.ipAddress : '') : ''; v.adapter.title = v.adapter.textContent;
        v.win.textContent = o ? (o.windowsVersion || '') : '—';
        setMeter('cpuBar', o && o.cpu ? o.cpu.usagePercent : null);
        setMeter('memBar', o && o.memory ? o.memory.usagePercent : null);
        setMeter('stBar', st ? st.usagePercent : null);
        renderUptime();
      }
      function renderUptime() {
        const up = uptimeBase == null ? null : uptimeBase + (performance.now() - uptimeAt);
        v.uptime.textContent = up == null ? '' : 'Up for ' + WK.fmt.uptimeMs(up) + (up > 14 * 86400000 ? ' \u00b7 a restart is due' : '');
        v.uptime.classList.toggle('warn', up != null && up > 14 * 86400000);
      }
      function setUptime(ms) { if (typeof ms === 'number' && isFinite(ms)) { uptimeBase = ms; uptimeAt = performance.now(); } }

      async function refreshOverview() {
        if (inFlight) return; inFlight = true;
        try {
          const o = await WK.native('sysinfo_get');
          if (o && typeof o === 'object') { overview = o; setUptime(o.uptimeMs); }
        } catch (e) { console.warn('sysinfo_get failed', e); }
        finally { inFlight = false; if (!disposed) renderOverview(); }
        if (!disposed && Date.now() - hwAt > 30000) loadHardware();
      }

      // ---- quick actions ----
      const icon = (k) => el('span', { class: 'db-ic', html: '<svg viewBox="0 0 16 16" aria-hidden="true"><path d="' + (WK.icons && WK.icons[k] || '') + '"/></svg>' });
      const quick = (ic, title, desc, go) => el('div', { class: 'wk-card interactive', tabindex: 0, role: 'button', onclick: go,
        onkeydown: (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); go(); } } },
        icon(ic), el('div', null, el('div', { class: 'db-qt', text: title }), el('div', { class: 'db-qd', text: desc })));
      const qa = el('div', { class: 'db-qa' },
        quick('diagnostics', 'Run Diagnostics', 'Check connectivity, firewall, and updates', () => WK.nav.go('diagnostics', { run: true })),
        quick('cleanup', 'Clean Temporary Files', 'Free up space from temp and cache files', () => WK.nav.go('cleanup')),
        quick('network', 'Network Test', 'Ping, DNS lookup, and traceroute tools', () => WK.nav.go('network')),
        quick('system', 'System Information', 'Processes, startup apps, and services', () => WK.nav.go('system')));

      // ---- recent activity ----
      const actBody = el('div'), actMore = el('div', { class: 'db-more wk-hide' }, el('button', { class: 'link', text: 'View all activity', onclick: () => WK.nav.go('activity') }));
      const actCard = el('div', { class: 'wk-card db-act' }, actBody, actMore);
      function renderActivity() {
        const all = WK.activity.entries();
        actBody.replaceChildren(...(all.length
          ? all.slice(0, RECENT).map((e) => el('div', { class: 'db-row' }, el('i', { class: e.kind }), el('span', { class: 'db-title', text: e.title, title: e.title }), el('span', { class: 'wk-muted', text: timeOf(e) })))
          : [el('div', { class: 'wk-muted db-none', text: 'No activity yet. Actions you take in WinKit will show up here.' })]));
        actMore.classList.toggle('wk-hide', all.length <= RECENT);
      }

      root.append(el('div', { class: 'db-wrap' },
        greet, el('div', { class: 'db-status' }, status),
        el('h2', { class: 'wk-eyebrow db-h', text: 'System overview' }), cards,
        hwHead, hwCards,
        el('h2', { class: 'wk-eyebrow db-h sp', text: 'Quick actions' }), qa,
        el('h2', { class: 'wk-eyebrow db-h sp', text: 'Recent activity' }), actCard));

      updateGreeting(); renderOverview(); renderActivity();

      function stop() {
        clearInterval(refreshTimer); clearInterval(tickTimer); refreshTimer = tickTimer = null;
        while (unsubs.length) { try { unsubs.pop()(); } catch (_) { /* ignore */ } }
      }
      return {
        show() {
          stop();
          updateGreeting(); renderActivity(); renderUptime();
          hwAt = 0;                                                  // re-read drives and battery whenever the page is opened
          unsubs.push(
            WK.on('diag_summary_changed', applySummary),
            WK.on('activity:changed', renderActivity),
            WK.on('profile:changed', updateGreeting));
          refreshTimer = setInterval(() => { if (document.hidden) return; updateGreeting(); refreshOverview(); }, REFRESH_MS);      // nothing to refresh while minimised
          tickTimer = setInterval(renderUptime, TICK_MS);
          loadSummary();
          if (uptimeBase == null) WK.native('sysinfo_uptime').then((u) => { if (uptimeBase == null && u) { setUptime(u.uptimeMs); renderUptime(); } }).catch(() => {});
          return refreshOverview();
        },
        hide() { stop(); }
      };
    }
  });
})();
