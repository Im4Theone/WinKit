/* ==========================================================================
   WinKit — product page
   Small behaviours: nav scroll state, reveal on scroll, tour tabs, footer year.
   ========================================================================== */

(function () {
  'use strict';

  /* ---------- Nav: hairline + blur once the page has moved ---------- */

  var nav = document.getElementById('nav');

  if (nav) {
    var setNavState = function () {
      nav.classList.toggle('is-scrolled', window.scrollY > 8);
    };

    setNavState();
    window.addEventListener('scroll', setNavState, { passive: true });
  }

  /* ---------- Reveal on scroll ---------- */

  var revealables = document.querySelectorAll('.reveal');
  var reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  if (reducedMotion || !('IntersectionObserver' in window)) {
    revealables.forEach(function (el) {
      el.classList.add('is-visible');
    });
  } else {
    var observer = new IntersectionObserver(
      function (entries) {
        entries.forEach(function (entry) {
          if (!entry.isIntersecting) return;
          entry.target.classList.add('is-visible');
          observer.unobserve(entry.target);
        });
      },
      { rootMargin: '0px 0px -10% 0px', threshold: 0.08 }
    );

    revealables.forEach(function (el) {
      observer.observe(el);
    });
  }


  /* ---------- Tour: accessible tabs (arrow keys, Home, End). Without JavaScript every screenshot is simply listed. ---------- */

  var tabs = Array.prototype.slice.call(document.querySelectorAll('.tour-tabs [role="tab"]'));

  if (tabs.length) {
    document.documentElement.classList.add('js');

    var select = function (tab, focus) {
      tabs.forEach(function (t) {
        var on = t === tab;
        t.setAttribute('aria-selected', on ? 'true' : 'false');
        t.tabIndex = on ? 0 : -1;
        var panel = document.getElementById(t.getAttribute('aria-controls'));
        if (panel) panel.hidden = !on;
      });
      if (focus) tab.focus();
    };

    tabs.forEach(function (tab, i) {
      tab.addEventListener('click', function () { select(tab, false); });
      tab.addEventListener('keydown', function (e) {
        var n = null;
        if (e.key === 'ArrowRight') n = tabs[(i + 1) % tabs.length];
        else if (e.key === 'ArrowLeft') n = tabs[(i - 1 + tabs.length) % tabs.length];
        else if (e.key === 'Home') n = tabs[0];
        else if (e.key === 'End') n = tabs[tabs.length - 1];
        if (n) { e.preventDefault(); select(n, true); }
      });
    });

    select(tabs[0], false);
  }

  /* ---------- Footer year ---------- */

  var year = document.getElementById('year');
  if (year) year.textContent = String(new Date().getFullYear());
})();
