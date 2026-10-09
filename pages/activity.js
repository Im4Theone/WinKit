/* activity.js - Activity log page (port of ActivityView.xaml / ActivityViewModel). */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;
  document.head.append(el('style', { text: `
.ac-wrap{max-width:720px}
.ac-sub{margin:4px 0 20px}
.ac-filters{display:inline-flex;gap:2px;padding:3px;margin-bottom:16px;background:var(--wk-surface);border:1px solid var(--wk-border);border-radius:calc(var(--wk-radius-sm) + 3px)}
.ac-filters button{padding:5px 14px;border-color:transparent;background:transparent}
.ac-filters button.on{background:var(--wk-accent);color:var(--wk-on-accent);border-color:var(--wk-accent)}
.ac-filters button.on:hover:not(:disabled){background:var(--wk-accent-hover);border-color:var(--wk-accent-hover)}
.ac-row{display:flex;align-items:center;gap:12px;padding:11px 18px;border-bottom:1px solid rgba(var(--wk-border-rgb),.7)}
.ac-row:last-child{border-bottom:0}
.ac-dot{width:7px;height:7px;border-radius:50%;background:var(--wk-text-muted);flex:none}
.ac-dot.Success{background:var(--wk-success)}.ac-dot.Warning{background:var(--wk-warning)}.ac-dot.Error{background:var(--wk-error)}
.ac-main{flex:1;min-width:0;overflow-wrap:anywhere}
.ac-ts{flex:none;white-space:nowrap}
.ac-empty{padding:28px 18px;text-align:center}
` }));
  // "Today 20:29" / "Yesterday 09:12" / "09/10/2026 20:29"
  function when(e) {
    const d = new Date(e.timestamp); if (isNaN(d.getTime())) return '';
    const now = new Date(), day = (x) => new Date(x.getFullYear(), x.getMonth(), x.getDate()).getTime(), diff = Math.round((day(now) - day(d)) / 86400000);
    let t; try { t = d.toLocaleTimeString([], { timeStyle: 'short' }); } catch (_) { t = d.toLocaleTimeString(); }
    return diff === 0 ? 'Today ' + t : diff === 1 ? 'Yesterday ' + t : WK.activity.formatTimestamp(e);
  }
  const FILTERS = [['all', 'All'], ['actions', 'Actions'], ['themesAndSettings', 'Themes & Settings']];

  WK.pages.register('activity', {
    title: 'Activity',
    create(root) {
      let filter = 'all', off = null;
      const list = el('div', { class: 'wk-card flat' });
      const btns = FILTERS.map(([id, label]) => el('button', { text: label, 'data-f': id, onclick: () => { filter = id; render(); } }));
      root.append(el('div', { class: 'ac-wrap' },
        el('h1', { class: 'wk-display', text: 'Activity' }),
        el('div', { class: 'wk-secondary ac-sub', text: 'Everything WinKit has done, most recent first.' }),
        el('div', { class: 'ac-filters' }, btns), list));
      function render() {
        btns.forEach(b => b.classList.toggle('on', b.dataset.f === filter));
        const items = WK.activity.filtered(filter);
        list.replaceChildren(...(items.length ? items.map(e => el('div', { class: 'ac-row' },
          el('span', { class: 'ac-dot ' + e.kind, title: e.kind }),
          el('div', { class: 'ac-main' }, el('div', { text: e.title }), e.detail ? el('div', { class: 'wk-muted', text: e.detail }) : null),
          el('span', { class: 'wk-muted ac-ts', title: WK.activity.formatTimestamp(e), text: when(e) }))) : [el('div', { class: 'wk-muted ac-empty', text: 'No activity in this category yet.' })]));
      }
      return {
        show() { if (off) off(); off = WK.on('activity:changed', render); render(); },
        hide() { if (off) off(); off = null; },
      };
    },
  });
})();
