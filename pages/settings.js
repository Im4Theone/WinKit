/* settings.js - Settings page (port of SettingsView.xaml / SettingsViewModel / ThemeDraftViewModel). */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;

  const css = `
.st-wrap{max-width:1000px}
.st-wrap h1{margin:0 0 20px}
.st-sec{margin:4px 0 10px}
.st-card{max-width:720px;margin:0 0 16px}
.st-row{display:flex;align-items:center;gap:20px;margin-bottom:14px}
.st-row:last-child{margin-bottom:0}
.st-row.nomargin{margin-bottom:0}
.st-grow{flex:1;min-width:0}
.st-btns{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:4px}
.st-btns button{padding:6px 12px}
.st-select{width:180px;flex:none}
.st-prev{display:flex;gap:4px;flex:none}
.st-prev i{display:block;width:18px;height:18px;border-radius:5px;border:1px solid rgba(128,128,128,.4)}
.st-editor{margin-top:14px;padding:16px;background:var(--wk-surface);border:1px solid var(--wk-border);border-radius:var(--wk-radius-sm)}
.st-card .st-editor{background:var(--wk-background)}
.st-edtop{display:flex;gap:10px;margin-bottom:12px;align-items:center}
.st-edtop input{flex:1;min-width:0;font-size:var(--wk-font-size-title)}
.st-edtop .st-edbtns{display:flex;gap:8px;flex:none}
.st-edtop button{padding:6px 12px}
.st-lbl{color:var(--wk-text-muted);font-size:10.5px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;margin:4px 0 10px}
.st-colors{display:grid;grid-template-columns:repeat(auto-fill,minmax(236px,1fr));gap:10px 16px;margin-bottom:6px}
.st-col{display:flex;align-items:center;min-width:0}
.st-sw{width:20px;height:20px;border-radius:4px;border:1px solid var(--wk-border);margin-right:8px;flex:none;
  background-image:linear-gradient(45deg,#8883 25%,transparent 25%,transparent 75%,#8883 75%),linear-gradient(45deg,#8883 25%,transparent 25%,transparent 75%,#8883 75%);background-size:8px 8px;background-position:0 0,4px 4px;position:relative;overflow:hidden}
.st-sw i{position:absolute;inset:0}
.st-col input{width:100px;padding:5px 8px;font-family:var(--wk-mono);font-size:12px}
.st-col input.bad{border-color:var(--wk-error)}
.st-col span{margin-left:8px;color:var(--wk-text-secondary);font-size:var(--wk-font-size-small);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.st-sl{display:grid;grid-template-columns:140px 1fr 50px;align-items:center;margin-bottom:8px}
.st-sl:last-of-type{margin-bottom:14px}
.st-sl input{width:100%;margin:0}
.st-sl output{color:var(--wk-text-secondary);padding-left:8px}
.st-combos{display:flex;flex-wrap:wrap;gap:0 20px}
.st-combos select{width:160px;display:block}
.st-nm{display:flex;align-items:flex-end;gap:10px}
.st-nm input{width:100%;max-width:280px;padding:7px 10px;display:block}
.st-adm b{font-weight:600}
`;
  document.head.append(el('style', { text: css }));

  const COLORS = [['background', 'Background'], ['surface', 'Surface'], ['surfaceElevated', 'Elevated surface'], ['border', 'Border'], ['textPrimary', 'Primary text'], ['textSecondary', 'Secondary text'], ['textMuted', 'Muted text'], ['accent', 'Accent'], ['accentHover', 'Accent hover'], ['onAccent', 'Text on accent'], ['success', 'Success'], ['warning', 'Warning'], ['error', 'Error'], ['update', 'Update']];
  const HEX = /^#([0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$/;
  const opts = (sel, list, val) => { sel.replaceChildren(...list.map(n => el('option', { value: n, text: n }))); sel.value = val; if (sel.value !== val && list.length) sel.selectedIndex = -1; };

  WK.pages.register('settings', {
    title: 'Settings',
    create(root) {
      let unsubs = [], editorEl = null, ed = null, active = false;

      // ---- Appearance ----
      const themeSel = el('select', { class: 'st-select', 'aria-label': 'Theme', onchange: () => { WK.themes.select(themeSel.value); } });
      const delBtn = el('button', { text: 'Delete', class: 'destructive', onclick: () => WK.themes.deleteFlow() });
      const editorHost = el('div');
      const preview = el('div', { class: 'st-prev', 'aria-hidden': 'true', title: 'Colours of the active theme' });
      const appearance = el('div', { class: 'wk-card st-card' },
        el('div', { class: 'st-row' },
          el('div', { class: 'st-grow' }, el('div', { text: 'Theme' }), el('div', { class: 'wk-muted', text: 'Choose a built-in theme, follow Windows, or use a custom one.' })),
          preview, themeSel),
        el('div', { class: 'st-btns' },
          el('button', { text: 'Create theme', onclick: () => WK.themeEditor.openCreate() }),
          el('button', { text: 'Edit current theme', onclick: () => WK.themeEditor.openEditCurrent() }),
          el('button', { text: 'Import', onclick: () => WK.themes.importFlow() }),
          el('button', { text: 'Export', onclick: () => WK.themes.exportFlow() }),
          el('button', { text: 'Reset to default', onclick: () => WK.themes.resetToDefault() }),
          delBtn),
        editorHost);

      function refreshThemes() {
        const names = WK.themes.names(), sel = WK.themes.selected();
        opts(themeSel, names, sel);
        const isCustom = WK.themes.custom().some(t => t.name === sel);
        delBtn.classList.toggle('wk-hide', !isCustom);
        const cur = WK.themes.current();
        preview.replaceChildren(...['background', 'surface', 'surfaceElevated', 'accent', 'textPrimary'].map(k => el('i', { style: 'background:' + (WK.themes.hexToCss(cur.colors[k]) || 'transparent') })));
      }

      // ---- editor ----
      function buildEditor(d) {
        const nameIn = el('input', { type: 'text', 'aria-label': 'Theme name', value: d.name, oninput: () => WK.themeEditor.set('name', nameIn.value) });
        const inputs = {}, sws = {};
        const colorEls = COLORS.map(([k, label]) => {
          const sw = el('div', { class: 'st-sw' }, el('i')); sws[k] = sw.firstChild;
          const inp = el('input', { type: 'text', 'aria-label': label, spellcheck: 'false', oninput: () => WK.themeEditor.setColor(k, inp.value) });
          inputs[k] = inp;
          return el('div', { class: 'st-col' }, sw, inp, el('span', { text: label }));
        });
        const sliders = {};
        const slider = (field, label, min, max, step, fmt) => {
          const out = el('output');
          const inp = el('input', { type: 'range', min, max, step, 'aria-label': label, oninput: () => WK.themeEditor.set(field, Number(inp.value)) });
          sliders[field] = { inp, out, fmt };
          return el('div', { class: 'st-sl' }, el('span', { text: label }), inp, out);
        };
        const dens = el('select', { 'aria-label': 'Density', onchange: () => WK.themeEditor.set('density', dens.value) }, ['Comfortable', 'Compact'].map(v => el('option', { value: v, text: v })));
        const anim = el('select', { 'aria-label': 'Animation intensity', onchange: () => WK.themeEditor.set('animationIntensity', anim.value) }, ['Full', 'Reduced', 'Off'].map(v => el('option', { value: v, text: v })));
        const node = el('div', { class: 'st-editor' },
          el('div', { class: 'st-edtop' }, nameIn, el('div', { class: 'st-edbtns' },
            el('button', { text: 'Cancel', onclick: () => WK.themeEditor.close() }),
            el('button', { class: 'primary', text: 'Save theme', onclick: () => WK.themeEditor.save() }))),
          el('div', { class: 'st-lbl', text: 'Colors' }),
          el('div', { class: 'st-colors' }, colorEls),
          el('div', { class: 'st-lbl', text: 'Layout' }),
          slider('cornerRadius', 'Corner radius', 0, 24, 1, v => v.toFixed(0)),
          slider('sidebarWidth', 'Sidebar width', 180, 320, 1, v => v.toFixed(0)),
          slider('fontScale', 'Font scale', 0.8, 1.4, 0.01, v => v.toFixed(2)),
          el('div', { class: 'st-combos' },
            el('div', null, el('div', { class: 'st-lbl', style: 'margin-bottom:6px', text: 'Density' }), dens),
            el('div', null, el('div', { class: 'st-lbl', style: 'margin-bottom:6px', text: 'Animation intensity' }), anim)));
        ed = { node, nameIn, inputs, sws, sliders, dens, anim };
        editorHost.replaceChildren(node);
      }
      function syncEditor(d) {
        if (!d) { ed = null; editorHost.replaceChildren(); return; }
        if (!ed) buildEditor(d);
        const foc = document.activeElement;
        if (ed.nameIn !== foc && ed.nameIn.value !== d.name) ed.nameIn.value = d.name;
        for (const [k] of COLORS) {
          const v = d.colors[k], inp = ed.inputs[k];
          if (inp !== foc && inp.value !== v) inp.value = v;
          ed.sws[k].style.background = WK.themes.hexToCss(v) || 'transparent';
          inp.classList.toggle('bad', !HEX.test(v));
        }
        for (const f in ed.sliders) { const s = ed.sliders[f], v = Number(d[f]); if (s.inp !== foc && Number(s.inp.value) !== v) s.inp.value = v; s.out.textContent = s.fmt(v); }
        ed.dens.value = d.density; ed.anim.value = d.animationIntensity;
      }

      // ---- Personalization ----
      const nameIn = el('input', { type: 'text', 'aria-label': 'Your name', onkeydown: e => { if (e.key === 'Enter') saveName(); } });
      const saveName = () => WK.settings.saveName(nameIn.value);
      const pers = el('div', { class: 'wk-card st-card' }, el('div', { class: 'st-nm' },
        el('div', { class: 'st-grow' }, el('div', { text: 'Your name', style: 'margin-bottom:8px' }), nameIn),
        el('button', { class: 'primary', text: 'Save', style: 'padding:8px 16px', onclick: saveName })));

      // ---- Behavior ----
      const sw = {};
      const toggle = (key, label, setter, last) => {
        const inp = el('input', { type: 'checkbox', class: 'wk-switch', 'aria-label': label, onchange: async () => { await WK.settings[setter](inp.checked); syncSettings(); } });
        sw[key] = inp;
        return el('label', { class: 'st-row' + (last ? ' nomargin' : '') }, el('span', { class: 'st-grow', text: label }), inp);
      };
      const behavior = el('div', { class: 'wk-card st-card' },
        toggle('startMinimized', 'Start minimized', 'setStartMinimized'),
        toggle('startWithWindows', 'Start with Windows', 'setStartWithWindows'),
        toggle('notificationsEnabled', 'Notifications', 'setNotificationsEnabled'),
        toggle('autoCheckForUpdates', 'Automatically check for updates', 'setAutoCheckForUpdates', true));
      function syncSettings() { const s = WK.settings.get(); for (const k in sw) sw[k].checked = !!s[k]; }

      // ---- Advanced ----
      const adminVal = el('b');
      const adminBtn = el('button', { text: 'Run as Administrator', style: 'padding:7px 14px', onclick: () => WK.admin.restart() });
      const adv = el('div', { class: 'wk-card st-card' }, el('div', { class: 'st-row nomargin' },
        el('div', { class: 'st-grow' },
          el('div', { class: 'wk-secondary st-adm' }, 'Running as administrator: ', adminVal),
          el('div', { class: 'wk-muted', style: 'margin-top:4px', text: 'WinKit only requests administrator access for the specific action that needs it.' })),
        adminBtn));
      function syncAdmin() { const e = WK.admin.isElevated(); adminVal.textContent = e ? 'True' : 'False'; adminBtn.classList.toggle('wk-hide', e); }

      root.append(el('div', { class: 'st-wrap' },
        el('h1', { class: 'wk-display', text: 'Settings' }),
        el('h2', { class: 'wk-title st-sec', text: 'Appearance' }), appearance,
        el('h2', { class: 'wk-title st-sec', text: 'Personalization' }), pers,
        el('h2', { class: 'wk-title st-sec', text: 'Behavior' }), behavior,
        el('h2', { class: 'wk-title st-sec', text: 'Advanced' }), adv));

      function render() {
        refreshThemes(); syncSettings(); syncAdmin();
        syncEditor(WK.themeEditor.isOpen() ? WK.themeEditor.draft() : null);
        if (document.activeElement !== nameIn) nameIn.value = WK.profile.current().name;
      }

      return {
        async show() {
          active = true;
          unsubs.forEach(u => u()); unsubs = [
            WK.on('themeEditor:changed', d => active && syncEditor(d)),
            WK.on('themes:listChanged', () => active && refreshThemes()),
            WK.on('theme:changed', () => active && refreshThemes()),
            WK.on('settings:changed', () => active && (syncSettings(), refreshThemes())),
            WK.on('profile:changed', p => { if (active && document.activeElement !== nameIn) nameIn.value = p.name; }),
          ];
          render();
          try { await WK.settings.refreshFromSystem(); } catch (e) { console.warn(e); }
          if (active) render();
        },
        hide() { active = false; unsubs.forEach(u => u()); unsubs = []; },
      };
    },
  });
})();
