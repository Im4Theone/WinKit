/* pages/network.js - Network tool page (port of NetworkToolsView + Ping/DnsLookup/Traceroute/IpConfiguration/NetworkMaintenance view models).
 * Tabs (same order as NetworkToolsViewModel): Ping, DNS Lookup, Traceroute, IP Configuration, Maintenance.
 * Deep link: WK.nav.go('network', { tab: 'ping' | 'dns' | 'traceroute' | 'ipconfig' | 'ports' | 'wifi' | 'conntest' | 'maintenance' }).  Opening the page without a tab selects Ping
 * (the C# calls SelectFirstTab on every navigation). Running pings/traces are NOT cancelled when the page is left (the C# view models live on). */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;

  // ---- styles (prefix .net-) ----
  const css = `
.net-bar{display:flex;align-items:center;gap:8px;margin-bottom:14px}
.net-bar input[type=text]{width:260px;padding:7px 10px}
.net-bar button{padding:7px 16px}
.net-rows{padding:0}
.net-row{display:grid;align-items:baseline;column-gap:0;padding:10px 16px;border-bottom:1px solid transparent}
.net-ping{grid-template-columns:60px 160px 1fr}
.net-trace{grid-template-columns:40px 160px 1fr 80px}
.net-headrow{padding:9px 16px;border-bottom:1px solid var(--wk-border);color:var(--wk-text-muted);font-size:10.5px;font-weight:600;letter-spacing:.06em;text-transform:uppercase}
.net-row+.net-row{border-top:1px solid rgba(var(--wk-border-rgb),.6)}
.net-row:hover{background:rgba(var(--wk-text-primary-rgb),.03)}
.net-lat{font-variant-numeric:tabular-nums}
.net-row>*{min-width:0}
.net-trunc{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;padding-right:12px}
.net-scroll{max-height:520px;overflow:auto}
.net-hint{padding:18px 16px}
.net-stat{margin-top:8px}
.net-dns-addr{font-family:var(--wk-mono);margin-bottom:4px}
.net-err{color:var(--wk-error)}
.net-adapters{display:flex;flex-direction:column;gap:12px}
.net-adapter .net-head{display:flex;align-items:center;justify-content:space-between;gap:12px}
.net-adapter .net-pill{border-radius:99px;padding:2px 10px;background:rgba(var(--wk-text-primary-rgb),.08);color:var(--wk-text-muted);font-size:var(--wk-font-size-small);font-weight:600;white-space:nowrap}
.net-adapter .net-pill.up{background:rgba(var(--wk-success-rgb),.15);color:var(--wk-success)}
.net-adapter .net-kv{display:grid;grid-template-columns:150px 1fr;row-gap:6px;margin-top:10px}
.net-adapter .net-kv dt{color:var(--wk-text-muted);font-size:var(--wk-font-size-small);margin:0;line-height:1.7}
.net-adapter .net-kv dd{margin:0;word-break:break-word}
.net-maint{display:flex;flex-wrap:wrap;gap:14px}
.net-mcard{width:230px;min-height:150px;display:flex;flex-direction:column;justify-content:space-between;gap:8px}
.net-mcard .net-act{display:flex;justify-content:space-between;align-items:flex-end;gap:8px}
.net-mcard .net-act button{padding:7px 14px;margin-left:auto}
.net-top-right{display:flex;justify-content:flex-end;margin-bottom:12px}
.wf-bars{display:inline-flex;align-items:flex-end;gap:2px;height:16px;vertical-align:middle}
.wf-bars i{width:4px;border-radius:1px;background:rgba(var(--wk-text-primary-rgb),.2)}
.wf-bars i.on{background:var(--wk-success)}
.wf-bars.fair i.on{background:var(--wk-warning)}.wf-bars.weak i.on{background:var(--wk-error)}
.wf-hero{display:flex;align-items:center;gap:12px}
.wf-hero .wf-bars{height:24px}.wf-hero .wf-bars i{width:6px}
.wf-note{margin-top:12px;padding:9px 12px;border-radius:var(--wk-radius-sm);background:rgba(var(--wk-warning-rgb),.12);color:var(--wk-text-primary)}
.wf-note.bad{background:rgba(var(--wk-error-rgb),.12)}
.wf-nets{padding:0;overflow:hidden;margin-top:12px}
.wf-net{display:grid;grid-template-columns:22px minmax(0,1fr) 120px 150px;column-gap:12px;align-items:center;padding:9px 16px;border-bottom:1px solid rgba(var(--wk-border-rgb),.7)}
.wf-net:last-child{border-bottom:0}
.wf-net.me{background:rgba(var(--wk-accent-rgb),.1)}
.wf-net .nm{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.wf-head{padding:10px 16px;border-bottom:1px solid var(--wk-border);color:var(--wk-text-muted);font-size:10.5px;font-weight:600;letter-spacing:.06em;text-transform:uppercase}
.wf-saved{margin-top:12px}.wf-saved summary{cursor:pointer;color:var(--wk-text-secondary)}
.wf-saved ul{margin:8px 0 0;padding-left:20px;color:var(--wk-text-secondary)}
.ct-verdict{margin-bottom:14px}
.ct-verdict .wk-title{margin-bottom:4px}
.ct-verdict.ok{border-left:3px solid var(--wk-success)}.ct-verdict.warn{border-left:3px solid var(--wk-warning)}.ct-verdict.fail{border-left:3px solid var(--wk-error)}
.ct-steps{padding:0;overflow:hidden}
.ct-step{display:grid;grid-template-columns:26px minmax(0,1fr);column-gap:12px;padding:12px 16px;border-bottom:1px solid rgba(var(--wk-border-rgb),.7)}
.ct-step:last-child{border-bottom:0}
.ct-ic{width:22px;height:22px;border-radius:50%;display:flex;align-items:center;justify-content:center;font-size:12px;font-weight:700;background:rgba(var(--wk-text-primary-rgb),.1);color:var(--wk-text-muted)}
.ct-step.ok .ct-ic{background:rgba(var(--wk-success-rgb),.18);color:var(--wk-success)}
.ct-step.warn .ct-ic{background:rgba(var(--wk-warning-rgb),.2);color:var(--wk-warning)}
.ct-step.fail .ct-ic{background:rgba(var(--wk-error-rgb),.2);color:var(--wk-error)}
.ct-step .wk-spin{margin:3px}
.ct-acts{display:flex;gap:8px;margin-top:10px}
.net-pwrap{padding:0;overflow:hidden}
.net-pwrap .net-scroll{max-height:max(200px,calc(100vh - 290px))}
table.net-ports td,table.net-ports th{padding-left:12px;padding-right:12px;white-space:nowrap}
.net-bar .wk-seg button{padding:6px 12px}
.net-pbar .wk-search{flex:1 1 120px;min-width:120px;max-width:260px}
.net-pbar .wk-search input{width:100%}
.net-pbar .wk-seg{flex:none}
.net-pbar .wk-secondary{white-space:nowrap}
.net-top-right button{padding:7px 14px}
`;
  document.head.appendChild(el('style', { 'data-for': 'net', text: css }));

  let opSeq = 0;
  const newOp = (p) => p + '-' + Date.now().toString(36) + '-' + (++opSeq);
  const errText = (e) => (e && e.message) ? e.message : String(e);

  const tone = (ms) => ms == null ? '' : ms < 60 ? 'wk-tone-ok' : ms < 150 ? 'wk-tone-warn' : 'wk-tone-bad';      // green / amber / red latency

  // ---- shared: host bar (textbox + Run/Stop) ----
  function hostBar(def, runLabel, onRun, onStop) {
    const input = el('input', { type: 'text', value: def, spellcheck: 'false', 'aria-label': 'Host',
      onkeydown: (e) => { if (e.key === 'Enter') { e.preventDefault(); onRun(); } } });
    const run = el('button', { class: 'primary', text: runLabel, onclick: onRun });
    const stop = el('button', { text: 'Stop', class: 'wk-hide', onclick: onStop });
    const bar = el('div', { class: 'net-bar' }, input, run, onStop ? stop : null);
    return { bar, input, setRunning(r) { run.classList.toggle('wk-hide', !!r && !!onStop); stop.classList.toggle('wk-hide', !r); run.disabled = !!r; } };
  }

  // ---------------------------------------------------------------- Ping
  function pingTab(panel) {
    let running = false, opId = null, seen = new Set();
    const bar = hostBar('8.8.8.8', 'Ping', run, stop);
    const rows = el('div', { class: 'net-rows' });
    const hint = el('div', { class: 'net-hint wk-muted', text: 'Enter a host name or IP address and press Ping.' });
    const card = el('div', { class: 'wk-card', style: 'padding:0;overflow:hidden' }, hint, rows);
    const stat = el('div', { class: 'net-stat wk-muted' });
    panel.append(bar.bar, card, stat);

    function addRow(r) {
      if (!r || seen.has(r.sequence)) return; seen.add(r.sequence);
      hint.classList.add('wk-hide');
      const ok = r.status === 'Success';
      const result = ok ? 'Reply in ' + (r.rttMs == null ? '?' : r.rttMs) + ' ms'
        : r.status === 'TimedOut' ? 'Request timed out'
        : r.status === 'Unknown' ? 'No reply (host not found or unreachable)'
        : String(r.status || '').replace(/([a-z])([A-Z])/g, '$1 $2');
      rows.append(el('div', { class: 'net-row net-ping' },
        el('span', { class: 'wk-muted', text: '#' + r.sequence }),
        el('span', { class: 'selectable', text: r.address == null ? '' : r.address }),
        el('span', { class: 'net-lat ' + (ok ? tone(r.rttMs) : 'wk-tone-warn'), text: result })));
    }
    async function run() {
      const host = bar.input.value.trim();
      if (!host || running) return;
      rows.replaceChildren(); seen = new Set(); stat.textContent = ''; hint.classList.add('wk-hide');
      running = true; bar.setRunning(true); opId = newOp('ping');
      const my = opId, off = WK.on('ping_reply', (d) => { if (d && d.opId === my) addRow(d.reply); });
      try {
        const res = await WK.native('ping_run', { host, count: 4, opId: my });
        (res && res.replies || []).forEach(addRow);
        if (res && res.received === 0 && !res.cancelled && !seen.size) hint.classList.remove('wk-hide');
        if (res && res.sent) stat.textContent = 'Sent ' + res.sent + ', received ' + res.received + ', lost ' + res.lost +
          (res.received ? ' - min ' + res.minMs + 'ms, avg ' + res.avgMs + 'ms, max ' + res.maxMs + 'ms' : '') + (res.cancelled ? ' (stopped)' : '');
      } catch (e) {
        WK.alert('Ping failed', errText(e));
      } finally {
        off(); running = false; opId = null; bar.setRunning(false);
        if (!seen.size) hint.classList.remove('wk-hide');
      }
    }
    function stop() { if (opId) WK.native('ping_cancel', { opId }).catch(() => {}); }
    return {};
  }

  // ---------------------------------------------------------------- DNS
  function dnsTab(panel) {
    let running = false;
    const bar = hostBar('example.com', 'Lookup', run, null);
    const body = el('div');
    const card = el('div', { class: 'wk-card wk-hide' }, body);
    const hint = el('div', { class: 'wk-card wk-muted', text: 'Enter a host name or IP address and press Lookup.' });
    panel.append(bar.bar, hint, card);
    async function run() {
      const host = bar.input.value.trim();
      if (!host || running) return;
      running = true; bar.setRunning(true); card.classList.add('wk-hide'); hint.classList.add('wk-hide');
      body.replaceChildren(el('span', { class: 'wk-spin' }), ' ', el('span', { class: 'wk-secondary', text: 'Looking up ' + host + '...' }));
      card.classList.remove('wk-hide');
      try {
        const out = await WK.native('dns_lookup', { host });
        if (out && out.success && out.value) {
          body.replaceChildren(el('div', { class: 'wk-title', style: 'margin-bottom:10px', text: out.value.hostName == null ? '' : out.value.hostName }),
            ...(out.value.addresses || []).map(a => el('div', { class: 'net-dns-addr selectable', text: a })));
        } else {
          body.replaceChildren(el('div', { class: 'net-err', text: (out && out.userMessage) || '' }));
        }
      } catch (e) {
        body.replaceChildren(el('div', { class: 'net-err', text: errText(e) }));
      } finally { running = false; bar.setRunning(false); }
    }
    return {};
  }

  // ---------------------------------------------------------------- Traceroute
  function traceTab(panel) {
    let running = false, opId = null, seen = new Set();
    const bar = hostBar('8.8.8.8', 'Trace', run, stop);
    const rows = el('div', { class: 'net-rows' });
    const scroll = el('div', { class: 'net-scroll' }, rows);
    const hint = el('div', { class: 'net-hint wk-muted', text: 'Enter a host name or IP address and press Trace.' });
    const head = el('div', { class: 'net-row net-trace net-headrow wk-hide' }, el('span', { text: 'Hop' }), el('span', { text: 'Address' }), el('span', { text: 'Host name' }), el('span', { text: 'Time' }));
    const card = el('div', { class: 'wk-card', style: 'padding:0;overflow:hidden' }, hint, head, scroll);
    const stat = el('div', { class: 'net-stat wk-muted' });
    panel.append(bar.bar, card, stat);

    function addHop(h, host) {
      if (!h) return; const key = h.hop + '|' + (h.address || '') + '|' + h.timedOut;
      if (seen.has(key)) return; seen.add(key);
      hint.classList.add('wk-hide'); head.classList.remove('wk-hide');
      let name = h.hostName == null ? '' : h.hostName;
      if (h.timedOut) name = h.hop === 0 ? 'Could not resolve "' + host + '".' : 'Request timed out';
      rows.append(el('div', { class: 'net-row net-trace' },
        el('span', { class: 'wk-muted', text: h.hop }),
        el('span', { text: h.address == null ? '' : h.address }),
        el('span', { class: 'wk-secondary net-trunc', title: name, text: name }),
        el('span', { class: 'net-lat ' + tone(h.rttMs), text: h.rttMs == null ? '' : h.rttMs + ' ms' })));
      scroll.scrollTop = scroll.scrollHeight;
    }
    async function run() {
      const host = bar.input.value.trim();
      if (!host || running) return;
      rows.replaceChildren(); seen = new Set(); stat.textContent = ''; head.classList.add('wk-hide');
      running = true; bar.setRunning(true); opId = newOp('trace');
      const my = opId, off = WK.on('trace_hop', (d) => { if (d && d.opId === my) addHop(d.hop, host); });
      try {
        const res = await WK.native('trace_run', { host, maxHops: 30, opId: my });
        (res && res.hops || []).forEach(h => addHop(h, host));
        if (res && res.resolved && res.destination) stat.textContent = (res.reached ? 'Trace complete' : (res.cancelled ? 'Stopped' : 'Destination not reached')) + ' - ' + res.destination;
      } catch (e) {
        WK.alert('Traceroute failed', errText(e));
      } finally {
        off(); running = false; opId = null; bar.setRunning(false);
        if (!seen.size) hint.classList.remove('wk-hide');
      }
    }
    function stop() { if (opId) WK.native('trace_cancel', { opId }).catch(() => {}); }
    return {};
  }

  // ---------------------------------------------------------------- IP configuration
  function ipTab(panel) {
    let loading = false, loaded = false;
    const btn = el('button', { text: 'Refresh', onclick: refresh });
    const list = el('div', { class: 'net-adapters' });
    const state = el('div', { class: 'wk-muted', style: 'margin-bottom:12px' });
    panel.append(el('div', { class: 'net-top-right' }, state, btn), list);
    state.style.marginRight = 'auto'; state.parentNode.style.alignItems = 'center';
    const first = (a) => (a && a.length ? String(a[0]) : '-');
    function card(a) {
      return el('div', { class: 'wk-card net-adapter' },
        el('div', { class: 'net-head' }, el('div', { class: 'wk-title', text: a.name || '' }), el('span', { class: 'net-pill' + (a.isUp ? ' up' : ''), text: a.isUp ? 'Connected' : 'Not connected' })),
        el('div', { class: 'wk-muted', style: 'margin-top:2px', text: a.description || '' }),
        el('dl', { class: 'net-kv' },
          el('dt', { text: 'IPv4' }), el('dd', { class: 'selectable', 'data-copy': '', title: 'Click to copy', text: first(a.ipv4Addresses) }),
          el('dt', { text: 'MAC address' }), el('dd', { class: 'selectable', 'data-copy': '', title: 'Click to copy', text: a.macAddress == null ? '' : a.macAddress }),
          el('dt', { text: 'IPv6' }), el('dd', { class: 'selectable', 'data-copy': '', title: 'Click to copy', text: first(a.ipv6Addresses) }),
          el('dt', { text: 'Gateway' }), el('dd', { class: 'selectable', 'data-copy': '', title: 'Click to copy', text: first(a.gateways) }),
          el('dt', { text: 'DNS servers' }), el('dd', { class: 'selectable', 'data-copy': '', title: 'Click to copy', text: a.dnsServers && a.dnsServers.length ? a.dnsServers.join(', ') : '-' }),
          el('dt', { text: 'DHCP' }), el('dd', { text: a.dhcpEnabled ? 'Enabled' : 'Disabled' })));
    }
    async function refresh() {
      if (loading) return;
      loading = true; btn.disabled = true; loaded = true;
      state.replaceChildren(el('span', { class: 'wk-spin' }), ' Loading adapters...');
      try {
        const adapters = await WK.native('ipcfg_get');
        list.replaceChildren(...(adapters || []).map(card));
        if (!adapters || !adapters.length) list.append(el('div', { class: 'wk-card wk-muted', text: 'No network adapters found.' }));
      } catch (e) {
        WK.alert('Could not read IP configuration', errText(e));
      } finally { loading = false; btn.disabled = false; state.replaceChildren(); }
    }
    return { show() { if (!loaded) refresh(); } };   // C#: loaded once, on first selection of the tab
  }

  // ---------------------------------------------------------------- Maintenance
  function maintTab(panel) {
    let busy = false; const buttons = [];
    const ACTIONS = [
      { cmd: 'netmaint_flushdns', card: 'Flush DNS', desc: 'Clear the local DNS resolver cache.', title: 'Flush DNS', msg: "This clears your computer's cached DNS lookups.", ok: 'Flush DNS', destructive: false, success: 'DNS cache flushed', admin: false },
      { cmd: 'netmaint_dhcprenew', card: 'DHCP Renew', desc: 'Request a new IP address lease.', title: 'Renew DHCP lease', msg: "This requests a new IP address from your network's DHCP server.", ok: 'Renew', destructive: false, success: 'DHCP lease renewed', admin: true },
      { cmd: 'netmaint_dhcprelease', card: 'DHCP Release', desc: 'Release your current IP address lease.', title: 'Release DHCP lease', msg: "This releases your current IP address. You'll briefly lose network connectivity.", ok: 'Release', destructive: true, success: 'DHCP lease released', admin: true },
      { cmd: 'netmaint_winsockreset', card: 'Winsock Reset', desc: 'Reset the Windows network stack.', title: 'Reset Winsock', msg: 'This resets the Windows network stack. Your computer may need to be restarted.', ok: 'Reset Winsock', destructive: true, success: 'Winsock reset', admin: true },
      { cmd: 'netmaint_tcpipreset', card: 'TCP/IP Reset', desc: 'Reset TCP/IP to its default configuration.', title: 'Reset TCP/IP', msg: 'This resets the TCP/IP stack to its default configuration. Your computer may need to be restarted.', ok: 'Reset TCP/IP', destructive: true, success: 'TCP/IP stack reset', admin: true },
    ];
    async function act(a) {
      const ok = await WK.confirm({ title: a.title, message: a.msg, confirmText: a.ok, isDestructive: a.destructive });
      if (!ok || busy) return;
      busy = true; buttons.forEach(b => b.disabled = true);
      try {
        const r = await WK.native(a.cmd);
        if (r && r.success) {
          WK.activity.record(a.success, 'Success');
          WK.toast(a.success, undefined, 'Success');
        } else {
          WK.alert(a.title + ' failed', (r && r.userMessage) || 'Unknown error.', r && r.technicalDetail);
        }
      } catch (e) {
        WK.alert(a.title + ' failed', errText(e));
      } finally { busy = false; buttons.forEach(b => b.disabled = false); }
    }
    const wrap = el('div', { class: 'net-maint' });
    ACTIONS.forEach(a => {
      const b = el('button', { class: a.destructive ? 'destructive' : '', text: 'Run', onclick: () => act(a) });
      buttons.push(b);
      wrap.append(el('div', { class: 'wk-card net-mcard' },
        el('div', null, el('div', { class: 'wk-title', text: a.card }), el('div', { class: 'wk-secondary', style: 'margin-top:4px', text: a.desc }),
          a.admin ? el('div', { class: 'wk-muted', style: 'margin-top:6px', text: 'Asks for administrator approval' }) : null),
        el('div', { class: 'net-act' }, b)));
    });
    panel.append(wrap);
    return {};
  }


  // ---------------------------------------------------------------- Ports
  function portsTab(panel) {
    let all = [], loaded = false, loading = false, query = '', mode = 'listening';
    const MAX = 400, count = el('span', { class: 'wk-secondary' }), tbody = el('tbody');
    const search = WK.ui.search(q => { query = q; paint(); }, 'Search ports');
    const btn = el('button', { text: 'Refresh', onclick: () => refresh() });
    const seg = el('div', { class: 'wk-seg' });
    [['listening', 'Listening'], ['connected', 'Connected'], ['all', 'All']].forEach(([id, label]) => {
      seg.append(el('button', { text: label, class: id === mode ? 'on' : '', onclick: (e) => { mode = id; [...seg.children].forEach(b => b.classList.toggle('on', b === e.currentTarget)); paint(); } }));
    });
    const th = (t) => el('th', { text: t });
    const table = el('table', { class: 'wk-table net-ports' }, el('thead', null, el('tr', null, ['Program', 'PID', 'Protocol', 'Local address', 'Remote address', 'State'].map(th))), tbody);
    const fmt = (a, p) => (a.indexOf(':') >= 0 ? '[' + a + ']' : a) + ':' + p;
    function paint() {
      const rows = all.filter(r => (mode === 'all' || (mode === 'listening') === (r.state === 'Listening')) && (mode !== 'connected' || r.proto === 'TCP') &&
        (!query || WK.ui.matches(r.process + ' ' + r.pid + ' ' + r.proto + ' ' + r.local + ' ' + r.localPort + ' ' + r.remote + ' ' + r.remotePort + ' ' + r.state, query)));
      const nm = (r) => r.process || '￿';                                  // unnamed owners (closing connections) go last
      rows.sort((a, b) => (mode === 'connected' ? nm(a).localeCompare(nm(b)) : a.localPort - b.localPort) || nm(a).localeCompare(nm(b)) || a.localPort - b.localPort);
      count.textContent = rows.length + (rows.length === 1 ? ' entry' : ' entries');
      const shown = rows.slice(0, MAX);
      const menu = (r) => [{ label: 'Copy local address', run: () => WK.ui.copy(fmt(r.local, r.localPort)) }, { label: 'Copy port', run: () => WK.ui.copy(r.localPort) },
        { label: 'Copy remote address', disabled: !r.remote, run: () => WK.ui.copy(fmt(r.remote, r.remotePort)) }, { label: 'Copy PID', disabled: !r.pid, run: () => WK.ui.copy(r.pid) }, { sep: true },
        { label: 'Search online for "' + (r.process || 'this program') + '"', disabled: !r.process, run: () => WK.ui.searchWeb(r.process + ' process') }];
      tbody.replaceChildren(...shown.map(r => { const tr = el('tr', null,
        el('td', { text: r.process || (r.pid ? '' : '-'), title: r.process }), el('td', { text: r.pid || '-' }), el('td', { text: r.proto }),
        el('td', { class: 'selectable', text: fmt(r.local, r.localPort), 'data-copy': String(r.localPort), title: 'Click to copy the port' }),
        el('td', { class: 'selectable', text: r.remote ? fmt(r.remote, r.remotePort) : '-' }),
        el('td', { text: r.state })); WK.ui.contextMenu(tr, () => menu(r)); return tr; }));
      if (!shown.length) tbody.append(el('tr', null, el('td', { colspan: 6, class: 'wk-empty', text: query ? 'Nothing matches "' + query + '".' : 'Nothing to show here.' })));
      else if (rows.length > MAX) tbody.append(el('tr', null, el('td', { colspan: 6, class: 'wk-empty', text: 'Showing the first ' + MAX + '. Use the search box to narrow the list.' })));
    }
    async function refresh() {
      if (loading) return; loading = true; loaded = true; btn.disabled = true;
      try { const r = await WK.native('ports_list'); all = Array.isArray(r) ? r : []; paint(); }
      catch (e) { WK.alert("Couldn't read open ports", errText(e)); }
      finally { loading = false; btn.disabled = false; }
    }
    panel.append(el('div', { class: 'net-bar net-pbar' }, search.el, seg, count, el('span', { style: 'flex:1' }), btn),
      el('div', { class: 'wk-card flat net-pwrap' }, el('div', { class: 'net-scroll' }, table)));
    return { show() { if (!loaded) refresh(); } };
  }


  // ---------------------------------------------------------------- Wi-Fi
  const bars = (pct, big) => {
    const n = pct >= 75 ? 4 : pct >= 50 ? 3 : pct >= 25 ? 2 : pct > 0 ? 1 : 0;
    return el('span', { class: 'wf-bars' + (pct < 30 ? ' weak' : pct < 55 ? ' fair' : '') }, [1, 2, 3, 4].map(i => el('i', { class: i <= n ? 'on' : '', style: 'height:' + (big ? 6 + i * 4 : 4 + i * 3) + 'px' })));
  };
  const quality = (p) => p >= 80 ? 'Excellent' : p >= 60 ? 'Good' : p >= 40 ? 'Fair' : 'Weak';
  function wifiTab(panel) {
    let loaded = false, loading = false;
    const btn = el('button', { text: 'Refresh', onclick: () => refresh(false) });
    const scanBtn = el('button', { text: 'Scan for networks', title: 'Ask the adapter to look for networks again (takes a few seconds)', onclick: () => refresh(true) });
    const body = el('div');
    panel.append(el('div', { class: 'net-top-right' }, scanBtn, el('span', { style: 'width:8px' }), btn), body);
    const kv = (rows) => el('dl', { class: 'net-kv' }, rows.filter(Boolean).flatMap(([k, v, copy]) => [el('dt', { text: k }), el('dd', copy ? { class: 'selectable', 'data-copy': '', title: 'Click to copy', text: v } : { text: v })]));
    function adapterView(ad) {
      const out = [], c = ad.connection;
      if (c) {
        const s = c.signal || 0, notes = [];
        out.push(el('div', { class: 'wk-card net-adapter' },
          el('div', { class: 'net-head' }, el('div', { class: 'wf-hero' }, bars(s, true), el('div', null, el('div', { class: 'wk-title', text: c.ssid }), el('div', { class: 'wk-muted', text: ad.description }))), el('span', { class: 'net-pill up', text: 'Connected' })),
          kv([['Signal', quality(s) + ' - ' + s + '%' + (c.rssiDbm != null ? ' (' + c.rssiDbm + ' dBm)' : '')],
            c.band ? ['Band and channel', c.band + ', channel ' + c.channel + ' (' + c.frequencyMhz + ' MHz)'] : null,
            ['Link speed', 'Download ' + c.rxMbps + ' Mbps, upload ' + c.txMbps + ' Mbps'],
            c.phy ? ['Wi-Fi standard', c.phy] : null, ['Security', c.security], ['Access point', c.bssid, true], c.profile && c.profile !== c.ssid ? ['Saved as', c.profile] : null])));
        if (c.weakSecurity) notes.push(['bad', 'This network uses weak or outdated security (' + c.security + '). Anyone nearby may be able to read your traffic. Switch the router to WPA2 or WPA3 if you can.']);
        if (s < 40) notes.push(['', 'The signal is weak. Moving closer to the router, or away from walls and other electronics, usually helps. A 5 GHz network is faster but reaches less far than 2.4 GHz.']);
        if (c.band === '2.4 GHz' && c.othersOnChannel >= 3) notes.push(['', c.othersOnChannel + ' other access points share your channel, which can slow the connection down. Try channel 1, 6 or 11 on the router, or use a 5 GHz network.']);
        else if (c.othersOnChannel >= 6) notes.push(['', c.othersOnChannel + ' other access points share your channel. Choosing a less busy channel on the router may help.']);
        notes.forEach(([k, t]) => out.push(el('div', { class: 'wf-note' + (k ? ' ' + k : ''), text: t })));
      } else {
        out.push(el('div', { class: 'wk-card net-adapter' }, el('div', { class: 'net-head' }, el('div', null, el('div', { class: 'wk-title', text: ad.description }), el('div', { class: 'wk-muted', text: 'Wi-Fi adapter' })), el('span', { class: 'net-pill', text: ad.state })),
          el('div', { class: 'wk-secondary', style: 'margin-top:10px', text: ad.state === 'Not connected' ? 'This adapter is not connected to a Wi-Fi network right now.' : 'This adapter is ' + ad.state.toLowerCase() + '.' })));
      }
      const nets = ad.networks || [];
      const list = el('div', { class: 'wk-card flat wf-nets' }, el('div', { class: 'wf-head', text: 'Nearby networks' }));
      if (nets.length) nets.forEach(n => list.append(el('div', { class: 'wf-net' + (n.connected ? ' me' : '') }, bars(n.signal),
        el('div', { class: 'nm', title: n.ssid }, n.ssid, n.connected ? el('span', { class: 'wk-muted', text: '  connected' }) : n.saved ? el('span', { class: 'wk-muted', text: '  saved' }) : null),
        el('div', { class: 'wk-secondary', text: n.channel ? n.band + ', ch ' + n.channel : '' }), el('div', { class: 'wk-secondary', text: n.secure ? n.security : 'Open (no password)' }))));
      else list.append(el('div', { class: 'wk-empty', text: 'No networks found. Press "Scan for networks". If the list stays empty, Windows may be hiding nearby networks because Location access is turned off (Settings, Privacy and security, Location).' }));
      out.push(list);
      const prof = ad.profiles || [];
      if (prof.length) out.push(el('details', { class: 'wf-saved' }, el('summary', { text: 'Saved networks (' + prof.length + ')' }), el('ul', null, prof.map(p => el('li', { text: p })))));
      return out;
    }
    async function refresh(rescan) {
      if (loading) return; loading = true; loaded = true; btn.disabled = scanBtn.disabled = true;
      body.replaceChildren(el('div', { class: 'net-hint wk-muted' }, el('span', { class: 'wk-spin' }), rescan ? ' Looking for networks...' : ' Reading Wi-Fi details...'));
      try {
        const r = await WK.native('wifi_info', { rescan: rescan ? 'true' : 'false' });
        if (!r || !r.available) body.replaceChildren(el('div', { class: 'wk-card wk-secondary', text: (r && r.reason) || 'Wi-Fi details are not available.' }));
        else body.replaceChildren(...r.adapters.flatMap(adapterView));
      } catch (e) { body.replaceChildren(); WK.alert("Couldn't read Wi-Fi details", errText(e)); }
      finally { loading = false; btn.disabled = scanBtn.disabled = false; }
    }
    return { show() { if (!loaded) refresh(false); } };
  }

  // ---------------------------------------------------------------- Connection test
  function connTab(panel) {
    let running = false, stop = false, curOp = null;
    const run = el('button', { class: 'primary', text: 'Run test', onclick: go });
    const stopBtn = el('button', { class: 'wk-hide', text: 'Stop', onclick: () => { stop = true; if (curOp) WK.native('ping_cancel', { opId: curOp }).catch(() => {}); } });
    const verdict = el('div', { class: 'wk-card ct-verdict wk-hide' });
    const steps = el('div', { class: 'wk-card flat ct-steps' });
    const hint = el('div', { class: 'net-hint wk-muted', text: 'Checks each step between this PC and the internet (adapter, router, internet, name lookup) and tells you where a problem is. It takes about 15 seconds.' });
    panel.append(el('div', { class: 'net-bar' }, run, stopBtn), hint, verdict, steps);
    const ICONS = { ok: '\u2713', warn: '!', fail: '\u2715', skip: '\u2013' };
    function step(title) {
      const ic = el('span', { class: 'ct-ic' }, el('span', { class: 'wk-spin' })), detail = el('div', { class: 'wk-secondary' }, 'Checking...'), row = el('div', { class: 'ct-step' }, ic, el('div', null, el('div', { style: 'font-weight:600', text: title }), detail));
      steps.append(row);
      return (state, text) => { row.className = 'ct-step ' + state; ic.replaceChildren(document.createTextNode(ICONS[state] || '')); detail.textContent = text; };
    }
    const jitter = (rs) => { const v = rs.filter(r => r.status === 'Success').map(r => r.rttMs); if (v.length < 2) return 0; let s = 0; for (let i = 1; i < v.length; i++) s += Math.abs(v[i] - v[i - 1]); return s / (v.length - 1); };
    async function ping(host) {
      curOp = newOp('ct');
      try { return await WK.native('ping_run', { host, count: 6, opId: curOp, timeoutMs: 1500, intervalMs: 200 }); } finally { curOp = null; }
    }
    const stat = (r) => { const loss = r.sent ? Math.round(100 * r.lost / r.sent) : 100; return { loss, avg: r.avgMs == null ? null : Number(r.avgMs), jit: jitter(r.replies || []) }; };
    const fmtStat = (s) => s.loss === 100 ? 'No replies' : s.avg.toFixed(0) + ' ms average, ' + s.loss + '% lost' + (s.jit >= 1 ? ', ' + s.jit.toFixed(0) + ' ms jitter' : '');
    function showVerdict(kind, title, text, acts) {
      verdict.className = 'wk-card ct-verdict ' + kind;
      verdict.replaceChildren(...[el('div', { class: 'wk-title', text: title }), el('div', { class: 'wk-secondary', text }),
        acts && acts.length ? el('div', { class: 'ct-acts' }, acts.map(([t, f]) => el('button', { text: t, onclick: f }))) : null].filter(Boolean));
    }
    async function go() {
      if (running) return;
      running = true; stop = false; run.disabled = true; stopBtn.classList.remove('wk-hide'); hint.classList.add('wk-hide'); verdict.classList.add('wk-hide'); steps.replaceChildren();
      const toWifi = ['Wi-Fi details', () => WK.nav.go('network', { tab: 'wifi' })], toMaint = ['Network maintenance', () => WK.nav.go('network', { tab: 'maintenance' })];
      const result = { kind: 'ok', title: 'Your connection looks healthy', text: '', acts: [] };
      try {
        // 1. adapter
        const s1 = step('Network adapter'); let adapters = [];
        try { adapters = (await WK.native('ipcfg_get')) || []; } catch (_) { /* handled below */ }
        const ad = adapters.find(a => a.isUp && (a.ipv4Addresses || []).length && (a.gateways || []).some(g => /^\d+\.\d+\.\d+\.\d+$/.test(g))) || adapters.find(a => a.isUp && (a.ipv4Addresses || []).length);
        if (!ad) { s1('fail', 'No connected network adapter was found.'); Object.assign(result, { kind: 'fail', title: 'This PC is not connected to a network', text: 'Plug in a network cable or connect to Wi-Fi, then run the test again.', acts: [toWifi] }); throw 0; }
        const isWifi = /wi-?fi|wireless|wlan|802\.11/i.test(ad.name + ' ' + ad.description);
        s1('ok', ad.name + ' (' + ad.ipv4Addresses[0] + ')');
        const gw = (ad.gateways || []).find(g => /^\d+\.\d+\.\d+\.\d+$/.test(g));
        if (stop) throw 0;
        // 2. router
        const s2 = step(gw ? 'Router (' + gw + ')' : 'Router'); let wifiSignal = null;
        if (isWifi) { try { const w = await WK.native('wifi_info'); const c = w && w.adapters && w.adapters.map(x => x.connection).find(Boolean); if (c) wifiSignal = c.signal; } catch (_) { /* optional */ } }
        let g = null;
        if (!gw) { s2('warn', 'This connection has no router address, so this step was skipped.'); }
        else {
          g = stat(await ping(gw)); if (stop) throw 0;
          if (g.loss === 100) { s2('fail', 'The router did not answer.'); Object.assign(result, { kind: 'fail', title: "Your PC can't reach the router", text: isWifi ? 'Check that the router is on, and that you are in range of it. Weak Wi-Fi is a common cause.' : 'Check the cable between this PC and the router, and that the router is on.', acts: isWifi ? [toWifi] : [] }); throw 0; }
          const bad = g.loss > 0 || g.avg > 30 || g.jit > 15;
          s2(bad ? 'warn' : 'ok', fmtStat(g) + '.' + (wifiSignal != null ? ' Wi-Fi signal ' + wifiSignal + '%.' : ''));
          if (bad) Object.assign(result, { kind: 'warn', title: 'The connection to your router is unstable', text: (g.loss > 0 ? g.loss + '% of test packets to the router were lost. ' : 'Replies from the router are slow or uneven. ') + (isWifi ? 'This points to Wi-Fi interference or a weak signal. Try moving closer to the router, or use a cable.' : 'Check the network cable and the router.'), acts: isWifi ? [toWifi] : [] });
        }
        // 3. internet
        const s3 = step('Internet'); let net = null, tgt = '1.1.1.1';
        net = stat(await ping(tgt)); if (net.loss === 100) { tgt = '8.8.8.8'; net = stat(await ping(tgt)); } if (stop) throw 0;
        if (net.loss === 100) { s3('fail', 'Nothing answered from the internet (tried 1.1.1.1 and 8.8.8.8).'); if (result.kind === 'ok') Object.assign(result, { kind: 'fail', title: gw ? 'The router works, but there is no internet' : 'No internet connection', text: 'The problem is between your router and the internet. Restart the router and modem, wait a minute, and test again. If it stays down, contact your internet provider.', acts: [] }); throw 0; }
        const nbad = net.loss > 0 || net.avg > 150;
        s3(nbad ? 'warn' : 'ok', 'Reached ' + tgt + ': ' + fmtStat(net) + '.');
        if (nbad && result.kind === 'ok') Object.assign(result, { kind: 'warn', title: 'The internet connection is slow or unreliable', text: (net.loss > 0 ? net.loss + '% of test packets were lost. ' : 'Replies are slow (' + net.avg.toFixed(0) + ' ms). ') + (g && g.avg <= 30 && g.loss === 0 ? 'Your router is fine, so the slowness is beyond it. Try restarting the router, or ask your provider.' : ''), acts: [] });
        // 4. DNS
        const s4 = step('Name lookup (DNS)'); const t0 = performance.now(); let d = null;
        try { d = await WK.native('dns_lookup', { host: 'example.com' }); } catch (_) { /* treated as failure */ }
        const ms = Math.round(performance.now() - t0);
        if (!d || !d.success) { s4('fail', 'Could not look up example.com, although the internet itself works.'); Object.assign(result, { kind: 'fail', title: 'Websites will not open because name lookup fails', text: 'The internet works, but your DNS server is not answering. Flushing the DNS cache can fix it. If it does not, set a different DNS server such as 1.1.1.1 or 8.8.8.8 in the adapter settings.', acts: [toMaint] }); }
        else { s4(ms > 800 ? 'warn' : 'ok', 'Looked up example.com in ' + ms + ' ms' + ((ad.dnsServers || []).length ? ' using ' + ad.dnsServers[0] + '.' : '.')); if (ms > 800 && result.kind === 'ok') Object.assign(result, { kind: 'warn', title: 'Name lookup is slow', text: 'Pages may feel slow to start loading. A faster DNS server such as 1.1.1.1 or 8.8.8.8 can help.', acts: [toMaint] }); }
      } catch (e) { if (e && e !== 0) { WK.alert('The test could not finish', errText(e)); result.kind = null; } }
      if (stop && result.kind) { result.kind = null; hint.classList.add('wk-hide'); }
      if (result.kind) {
        if (result.kind === 'ok') result.text = 'Every step answered normally. If a website or app still misbehaves, the problem is with that service.';
        showVerdict(result.kind, result.title, result.text, result.acts); verdict.classList.remove('wk-hide');
        WK.activity.record('Connection test: ' + result.title, result.kind === 'ok' ? 'Success' : result.kind === 'warn' ? 'Warning' : 'Error');
      }
      running = false; run.disabled = false; run.textContent = 'Run again'; stopBtn.classList.add('wk-hide');
    }
    return {};
  }

  // ---------------------------------------------------------------- page
  const TAB_ALIAS = { ping: 'ping', dns: 'dns', dnslookup: 'dns', 'dns-lookup': 'dns', traceroute: 'traceroute', trace: 'traceroute', tracert: 'traceroute',
    ipconfig: 'ipconfig', ip: 'ipconfig', ipconfiguration: 'ipconfig', 'ip-configuration': 'ipconfig', maintenance: 'maintenance', maint: 'maintenance', netmaint: 'maintenance', wifi: 'wifi', 'wi-fi': 'wifi', wireless: 'wifi', wlan: 'wifi', conntest: 'conntest', 'connection-test': 'conntest', connection: 'conntest', diagnose: 'conntest', ports: 'ports', port: 'ports', connections: 'ports', netstat: 'ports' };

  WK.pages.register('network', {
    title: 'Network',
    create(root) {
      const tp = WK.ui.tabsPage(root, 'Network', [
        { id: 'ping', title: 'Ping', create: pingTab },
        { id: 'dns', title: 'DNS Lookup', create: dnsTab },
        { id: 'traceroute', title: 'Traceroute', create: traceTab },
        { id: 'ipconfig', title: 'IP Configuration', create: ipTab },
        { id: 'ports', title: 'Ports', create: portsTab },
        { id: 'wifi', title: 'Wi-Fi', create: wifiTab },
        { id: 'conntest', title: 'Connection test', create: connTab },
        { id: 'maintenance', title: 'Maintenance', create: maintTab },
      ], 'Test connectivity, look up names, inspect adapters and reset the network stack.');
      return {
        show(opts) {
          const want = opts && opts.tab ? TAB_ALIAS[String(opts.tab).toLowerCase()] : null;
          tp.select(want || 'ping');
        },
        hide() { tp.hide(); },
      };
    },
  });
})();
