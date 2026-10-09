/* pages/network.js - Network tool page (port of NetworkToolsView + Ping/DnsLookup/Traceroute/IpConfiguration/NetworkMaintenance view models).
 * Tabs (same order as NetworkToolsViewModel): Ping, DNS Lookup, Traceroute, IP Configuration, Maintenance.
 * Deep link: WK.nav.go('network', { tab: 'ping' | 'dns' | 'traceroute' | 'ipconfig' | 'maintenance' }).  Opening the page without a tab selects Ping
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
          el('dt', { text: 'IPv4' }), el('dd', { class: 'selectable', text: first(a.ipv4Addresses) }),
          el('dt', { text: 'MAC address' }), el('dd', { class: 'selectable', text: a.macAddress == null ? '' : a.macAddress }),
          el('dt', { text: 'IPv6' }), el('dd', { class: 'selectable', text: first(a.ipv6Addresses) }),
          el('dt', { text: 'Gateway' }), el('dd', { class: 'selectable', text: first(a.gateways) }),
          el('dt', { text: 'DNS servers' }), el('dd', { class: 'selectable', text: a.dnsServers && a.dnsServers.length ? a.dnsServers.join(', ') : '-' }),
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

  // ---------------------------------------------------------------- page
  const TAB_ALIAS = { ping: 'ping', dns: 'dns', dnslookup: 'dns', 'dns-lookup': 'dns', traceroute: 'traceroute', trace: 'traceroute', tracert: 'traceroute',
    ipconfig: 'ipconfig', ip: 'ipconfig', ipconfiguration: 'ipconfig', 'ip-configuration': 'ipconfig', maintenance: 'maintenance', maint: 'maintenance', netmaint: 'maintenance' };

  WK.pages.register('network', {
    title: 'Network',
    create(root) {
      const tp = WK.ui.tabsPage(root, 'Network', [
        { id: 'ping', title: 'Ping', create: pingTab },
        { id: 'dns', title: 'DNS Lookup', create: dnsTab },
        { id: 'traceroute', title: 'Traceroute', create: traceTab },
        { id: 'ipconfig', title: 'IP Configuration', create: ipTab },
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
