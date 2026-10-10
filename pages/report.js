/* pages/report.js - "Export health report": gathers a plain-text summary of this PC (system, drives, health checks, network, startup, busiest programs)
 * that you can save or copy, for example to send to whoever is helping you. Opened from Diagnostics and Quick Open: WK.report.open().
 * Computer name, network addresses and Wi-Fi names are left out unless you tick the box. Saved Wi-Fi networks and passwords are never included. */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;
  document.head.appendChild(el('style', { 'data-for': 'rp', text: '.rp-opt{display:flex;gap:10px;align-items:flex-start;margin:14px 0 4px;cursor:pointer}.rp-opt input{margin-top:3px}.rp-opt small{display:block;color:var(--wk-text-muted);margin-top:2px}' }));

  const NL = '\r\n';
  const safe = async (cmd, args) => { try { return await WK.native(cmd, args); } catch (_) { return null; } };
  const gb = (n) => (Math.round((Number(n) || 0) * 10) / 10) + ' GB';
  const pad = (s, n) => String(s).padEnd(n);
  const errText = (e) => (e && e.message) ? e.message : String(e);
  const head = (t) => NL + t.toUpperCase() + NL + '-'.repeat(t.length);
  const kv = (k, v) => pad(k, 16) + v;
  const KIND = { Fixed: 'local disk', Removable: 'removable drive', Network: 'network drive' };

  async function build(personal, say) {
    const scrub = (s) => personal ? String(s == null ? '' : s) : String(s == null ? '' : s).replace(/([A-Za-z]:\\Users\\)[^\\]+/gi, '$1<user>');
    const hide = (v, shown) => personal ? shown : '(left out)';
    say('Reading system details...');
    const [info, sys, drives, adapters, startup, procs, apps, wifi, hw] = await Promise.all([safe('app_info'), safe('sysinfo_get'), safe('disk_drives'), safe('ipcfg_get'), safe('startup_list'), safe('proc_list'), safe('apps_list'), safe('wifi_info'), safe('hw_health')]);
    say('Running the health checks (about 10 seconds)...');
    const diag = await safe('diag_run_all');
    const L = [];
    L.push('WinKit health report', '====================', kv('Created', new Date().toLocaleString()), kv('WinKit', (info && info.version) || ''),
      kv('Personal info', personal ? 'Included (computer name, network addresses, Wi-Fi name)' : 'Left out (computer name, network addresses, Wi-Fi name)'));

    L.push(head('System'));
    if (sys) {
      L.push(kv('Computer', hide(0, sys.machineName)), kv('Windows', sys.windowsVersion + ', ' + sys.architecture), kv('Processor', (sys.cpu && sys.cpu.name || 'Unknown') + (sys.cpu && sys.cpu.coreCount ? ' (' + sys.cpu.coreCount + ' logical cores)' : '')),
        kv('Memory', sys.memory ? gb(sys.memory.totalGb) + ', ' + Math.round(sys.memory.usagePercent) + '% in use' : 'Unknown'), kv('Graphics', (sys.gpu && sys.gpu.name) || 'Unknown'),
        kv('Uptime', sys.uptimeText || ''), kv('Administrator', sys.isAdmin ? 'Yes' : 'No (checks that need administrator rights were reported as such)'));
    } else L.push('Could not read system details.');

    if (hw && hw.battery && hw.battery.present) {
      const b = hw.battery;
      L.push(head('Battery'), kv('Charge', (b.percent == null ? 'Unknown' : b.percent + '%') + (b.charging ? ', charging' : b.onAc ? ', plugged in' : ', on battery') + (b.remainingMinutes != null && !b.onAc ? ', about ' + Math.floor(b.remainingMinutes / 60) + ' h ' + (b.remainingMinutes % 60) + ' min left' : '')));
      if (b.wearPercent != null) L.push(kv('Health', b.health + ': holds ' + Math.round(100 - b.wearPercent) + '% of its original capacity (' + b.fullMwh + ' of ' + b.designMwh + ' mWh)' + (b.cycleCount ? ', ' + b.cycleCount + ' charge cycles' : '')));
    }

    L.push(head('Drives'));
    if (Array.isArray(drives) && drives.length) drives.forEach((d) => { const used = Math.round((d.totalBytes - d.freeBytes) * 100 / d.totalBytes);
      L.push(kv(d.letter, (d.label ? d.label + ', ' : '') + d.fileSystem + ', ' + (KIND[d.type] || String(d.type).toLowerCase()) + ': ' + WK.fmt.bytes(d.freeBytes) + ' free of ' + WK.fmt.bytes(d.totalBytes) + ' (' + used + '% used)')); });
    else L.push('No drives found.');

    L.push(head('Health checks'));
    if (diag && Array.isArray(diag.results) && diag.results.length) {
      const n = (s) => diag.results.filter((r) => r.status === s).length, issues = n('Warning') + n('Failed');
      L.push((issues ? issues + (issues === 1 ? ' issue' : ' issues') + ' found' : 'No issues found') + ': ' + n('Passed') + ' passed, ' + n('Warning') + ' warnings, ' + n('Failed') + ' failed', '');
      diag.results.forEach((r) => {
        L.push(pad('[' + (r.statusLabel || r.status) + ']', 11) + r.name + ': ' + scrub(r.summary));
        if (r.isIssue && r.recommendedAction) L.push(pad('', 11) + 'Advice: ' + scrub(r.recommendedAction));
      });
    } else L.push('The health checks could not run (a scan may already have been in progress).');

    L.push(head('Network'));
    if (Array.isArray(adapters) && adapters.length) adapters.forEach((a) => {
      const ip4 = (a.ipv4Addresses || [])[0], gw = (a.gateways || [])[0];
      L.push(a.name + ' (' + (a.description || '') + '): ' + (a.isUp ? 'connected' : 'not connected'));
      if (a.isUp) L.push('    IPv4 ' + hide(0, ip4 || 'none') + ', gateway ' + (gw ? hide(0, gw) : 'none') + ', DNS ' + ((a.dnsServers || []).length ? hide(0, a.dnsServers.join(', ')) : 'none') + ', DHCP ' + (a.dhcpEnabled ? 'on' : 'off'));
    }); else L.push('No network adapters found.');
    const wc = wifi && wifi.available && (wifi.adapters || []).map((x) => x.connection).find(Boolean);
    if (wc) L.push('Wi-Fi: ' + (personal ? '"' + wc.ssid + '"' : 'connected (name left out)') + ', signal ' + wc.signal + '%' + (wc.rssiDbm != null ? ' (' + wc.rssiDbm + ' dBm)' : '') + (wc.band ? ', ' + wc.band + ' channel ' + wc.channel : '') + ', ' + wc.rxMbps + '/' + wc.txMbps + ' Mbps, ' + wc.security + (wc.othersOnChannel ? ', ' + wc.othersOnChannel + ' other access points on the channel' : ''));

    L.push(head('Starts with Windows'));
    if (Array.isArray(startup)) {
      const on = startup.filter((s) => s.isEnabled).map((s) => s.name);
      L.push(on.length + ' enabled, ' + (startup.length - on.length) + ' disabled.', on.length ? 'Enabled: ' + scrub(on.join(', ')) : '');
    } else L.push('Could not read the startup list.');

    L.push(head('Busiest programs (by memory)'));
    if (Array.isArray(procs) && procs.length) {
      const by = new Map(); procs.forEach((p) => { const e = by.get(p.name) || { n: 0, mem: 0 }; e.n++; e.mem += Number(p.workingSetBytes) || 0; by.set(p.name, e); });
      [...by.entries()].sort((a, b) => b[1].mem - a[1].mem).slice(0, 8).forEach(([n, e]) => L.push(pad(n + (e.n > 1 ? ' (x' + e.n + ')' : ''), 32) + WK.fmt.bytes(e.mem)));
      L.push(procs.length + ' processes running.');
    } else L.push('Could not read the process list.');
    if (Array.isArray(apps)) L.push('', kv('Installed apps', apps.length + ' programs'));
    L.push('', 'Created by WinKit. Saved Wi-Fi networks, passwords and files are never included.');
    return L.join(NL) + NL;
  }

  async function showResult(text) {
    await WK.dialog({
      title: 'Health report ready', message: 'Look it over, then save it or copy it to send to whoever is helping you.',
      details: { header: 'Report', text, copyLabel: 'Copy report', open: true }, cancel: { label: 'Close' }, confirm: { label: 'Save as file...', kind: 'primary' },
      onConfirm: async (ctx) => {
        ctx.bOk.disabled = true;
        try {
          const stamp = new Date().toISOString().slice(0, 10);
          const path = await WK.native('export_file_dialog', { title: 'Save health report', filter: 'Text file (*.txt)|*.txt', defaultName: 'WinKit health report ' + stamp + '.txt', defaultExt: 'txt', text });
          if (path) { WK.activity.record('Health report saved', 'Success'); WK.toast('Health report saved', path, 'Success'); ctx.close(true); return; }
        } catch (e) { ctx.status("Couldn't save the report: " + errText(e), 'error'); }
        ctx.bOk.disabled = false;
      },
    });
  }

  let busy = false;
  WK.report = {
    async open() {
      if (busy) return; busy = true;
      try {
        const box = el('label', { class: 'rp-opt' }, el('input', { type: 'checkbox' }),
          el('span', null, 'Include my computer name, network addresses and Wi-Fi name', el('small', { text: 'Leave this off if you plan to post the report somewhere public.' })));
        let text = null;
        await WK.dialog({
          title: 'Export health report', message: 'Creates a plain-text summary of this PC: system details, drives, health checks, network and the busiest programs. It can take about 15 seconds.',
          extra: box, cancel: { label: 'Cancel' }, confirm: { label: 'Create report', kind: 'primary' },
          onConfirm: async (ctx) => {
            ctx.bOk.disabled = ctx.bCancel.disabled = true; box.querySelector('input').disabled = true;
            try { text = await build(box.querySelector('input').checked, (m) => ctx.status(m, '')); }
            catch (e) { ctx.status("Couldn't create the report: " + errText(e), 'error'); ctx.bOk.disabled = ctx.bCancel.disabled = false; box.querySelector('input').disabled = false; return; }
            ctx.close(true);
          },
        });
        if (text) await showResult(text);
      } finally { busy = false; }
    },
  };
})();
