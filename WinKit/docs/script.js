/* ==========================================================================
   WinKit — product page
   Three small behaviours: nav scroll state, reveal on scroll, footer year.
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

  /* ---------- Footer year ---------- */

  var year = document.getElementById('year');
  if (year) year.textContent = String(new Date().getFullYear());
})();
