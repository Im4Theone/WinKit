/* onboarding.js - first-run name prompt (port of OnboardingView.xaml / OnboardingViewModel). */
(function () {
  'use strict';
  const WK = window.WK, el = WK.ui.el;
  document.head.append(el('style', { text: `
.ob-page{display:flex;align-items:center;justify-content:center;min-height:100%}
.ob-box{width:400px;max-width:100%;padding:32px 32px 28px}
.ob-logo{display:flex;justify-content:center;margin-bottom:18px}
.ob-box h1{text-align:center;margin:0}
.ob-sub{text-align:center;margin:8px 0 28px}
.ob-box input{width:100%;padding:10px 12px;font-size:var(--wk-font-size-title)}
.ob-box button{width:100%;padding:11px 0;margin-top:20px;font-size:var(--wk-font-size-title)}
` }));

  WK.pages.register('onboarding', {
    title: 'Welcome',
    create(root) {
      const input = el('input', { type: 'text', 'aria-label': 'Your name', autocomplete: 'off', spellcheck: 'false', oninput: sync, onkeydown: e => { if (e.key === 'Enter') { e.preventDefault(); go(); } } });
      const btn = el('button', { class: 'primary', text: 'Continue', onclick: go });
      let busy = false;
      function sync() { btn.disabled = busy || !WK.onboarding.canContinue(input.value); }
      async function go() {
        if (busy || !WK.onboarding.canContinue(input.value)) return;
        busy = true; sync();
        try { if (await WK.onboarding.submit(input.value)) WK.nav.go('dashboard'); }
        finally { busy = false; sync(); }
      }
      root.append(el('div', { class: 'ob-page' }, el('div', { class: 'wk-card ob-box' },
        el('div', { class: 'ob-logo' }, WK.ui.logo(true)),
        el('h1', { class: 'wk-display', text: 'WinKit' }),
        el('div', { class: 'wk-secondary ob-sub', text: 'Welcome to WinKit' }),
        el('div', { class: 'wk-title', style: 'margin-bottom:10px', text: 'What should we call you?' }),
        input, btn)));
      return {
        async show() {
          try { await WK.app.info(); } catch (_) { /* ignore */ }
          input.value = WK.onboarding.defaultName(); sync();
          input.focus(); input.select();
        },
      };
    },
  });
})();
