/* about.js - About page (port of AboutView.xaml / AboutViewModel). */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;
  document.head.append(el('style', { text: `
.ab-wrap{max-width:600px}
.ab-head{display:flex;align-items:center;gap:16px;margin-bottom:24px}
.ab-head h1{margin:0}
.ab-ver{margin-top:2px}
.ab-wrap .wk-card{margin-bottom:16px}
.ab-wrap .wk-title{margin-bottom:8px}
.ab-btns{display:flex;flex-wrap:wrap;gap:10px;margin-top:14px}
.ab-prog{margin-top:12px}
.ab-kv{display:grid;grid-template-columns:140px 1fr;gap:8px 16px;margin:0}
.ab-kv dt{color:var(--wk-text-muted)}
.ab-kv dd{margin:0;word-break:break-all}
` }));

  WK.pages.register('about', {
    title: 'About',
    create(root) {
      let off = null;
      const ver = el('div', { class: 'wk-secondary ab-ver' });
      const status = el('div', { class: 'wk-secondary', role: 'status' });
      const bar = el('i'); const prog = el('div', { class: 'wk-progress ab-prog wk-hide' }, bar);
      const check = el('button', { text: 'Check for Updates', onclick: () => WK.about.check() });
      const upd = el('button', { class: 'primary', text: 'Update', onclick: () => WK.about.install() });
      const skip = el('button', { text: 'Skip this version', onclick: () => WK.about.skipThisVersion() });
      const env = el('dl', { class: 'ab-kv selectable' });
      const folderBtn = (label, target) => el('button', { class: 'sm', text: label, onclick: () => WK.shell.openFolder(target).catch((e) => WK.alert("Couldn't open the folder", e.message)) });
      root.append(el('div', { class: 'ab-wrap' },
        el('div', { class: 'ab-head' }, WK.ui.logo(true), el('div', null, el('h1', { class: 'wk-display', text: 'WinKit' }), ver)),
        el('div', { class: 'wk-card' },
          el('div', { text: 'A lightweight Windows toolkit for system information, diagnostics, cleanup, and networking.' }),
          el('div', { class: 'wk-secondary', style: 'margin-top:10px', text: 'Every action WinKit takes is a real, explainable operation — no fake optimizations, no invented performance claims.' })),
        el('div', { class: 'wk-card' },
          el('h2', { class: 'wk-title', text: 'Privacy' }),
          el('div', { class: 'wk-secondary', text: 'No account required. No telemetry. Crash reports are only ever sent when you explicitly click "Send to Developers" — nothing is sent automatically. No permanent background process.' })),
        el('div', { class: 'wk-card' },
          el('h2', { class: 'wk-title', text: 'Updates' }),
          status, prog,
          el('div', { class: 'ab-btns' }, check, upd, skip)),
        el('div', { class: 'wk-card' },
          el('h2', { class: 'wk-title', text: 'This installation' }),
          env,
          el('div', { class: 'ab-btns' }, folderBtn('Open data folder', '@data'), folderBtn('Open logs folder', '@logs')))));

      function renderEnv(info) {
        if (!info) return;
        const rows = [['Windows', info.windowsVersion], ['Architecture', info.architecture], ['Install type', info.isInstalledLocation ? 'Installed' : 'Portable'],
          ['Running as admin', info.isAdmin ? 'Yes' : 'No'], ['Data folder', info.dataDir]];
        env.replaceChildren(...rows.flatMap(([k, val]) => [el('dt', { text: k }), el('dd', { text: val == null ? '' : String(val) })]));
      }
      function render() {
        const s = WK.about.getState();
        ver.textContent = 'Version ' + WK.app.version;
        status.textContent = s.statusText;
        check.disabled = !!s.isChecking || !!s.isInstalling;
        upd.classList.toggle('wk-hide', !s.updateAvailable);
        skip.classList.toggle('wk-hide', !s.updateAvailable);
        upd.disabled = skip.disabled = !!s.isInstalling;
        prog.classList.toggle('wk-hide', !s.isInstalling);
        const p = s.progress && Number(s.progress.percent);
        prog.classList.toggle('indet', !(p >= 0));
        bar.style.width = p >= 0 ? Math.min(100, p) + '%' : '';
      }
      return {
        show() {
          if (off) off(); off = WK.on('about:changed', render); render();
          WK.app.info().then(renderEnv).catch(() => {});
        },
        hide() { if (off) off(); off = null; },
      };
    },
  });
})();
