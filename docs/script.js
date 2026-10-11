/* ==========================================================================
   WinKit — product page
   Small behaviours: nav scroll state, tour tabs, showcase video, footer year. Motion is in motion.js.
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

  /* Reveal on scroll lives in motion.js (GSAP ScrollTrigger). */

  var reducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  /* ---------- Tour: accessible tabs (arrow keys, Home, End). Without JavaScript every screenshot is simply listed. ---------- */

  var tabs = Array.prototype.slice.call(document.querySelectorAll('.tour-tabs [role="tab"]'));

  if (tabs.length) {
    document.documentElement.classList.add('js');

    var select = function (tab, focus, animate) {
      tabs.forEach(function (t) {
        var on = t === tab;
        t.setAttribute('aria-selected', on ? 'true' : 'false');
        t.tabIndex = on ? 0 : -1;
        var panel = document.getElementById(t.getAttribute('aria-controls'));
        if (panel) panel.hidden = !on;
        if (on && panel && animate && window.gsap && !reducedMotion) {
          window.gsap.fromTo(panel.querySelectorAll('.tour-media, .tour-copy > *'), { opacity: 0, y: 18 },
            { opacity: 1, y: 0, duration: 0.55, stagger: 0.06, ease: 'power3.out', overwrite: true, clearProps: 'transform' });
        }
      });
      if (window.ScrollTrigger) window.ScrollTrigger.refresh();
      if (focus) tab.focus();
    };

    tabs.forEach(function (tab, i) {
      tab.addEventListener('click', function () { select(tab, false, true); });
      tab.addEventListener('keydown', function (e) {
        var n = null;
        if (e.key === 'ArrowRight') n = tabs[(i + 1) % tabs.length];
        else if (e.key === 'ArrowLeft') n = tabs[(i - 1 + tabs.length) % tabs.length];
        else if (e.key === 'Home') n = tabs[0];
        else if (e.key === 'End') n = tabs[tabs.length - 1];
        if (n) { e.preventDefault(); select(n, true, true); }
      });
    });

    select(tabs[0], false);
  }

  /* ---------- Showcase video ----------
     Plays muted while at least a third of it is on screen and pauses when scrolled away.
     The button pauses it for good (WCAG 2.2.2). With reduced motion it waits for a click. */

  var video = document.getElementById('showcase');
  var toggle = document.getElementById('showcase-toggle');

  if (video && toggle) {
    var figure = video.closest('.showcase');
    var userPaused = reducedMotion;
    var started = false;

    video.removeAttribute('controls');
    toggle.hidden = false;

    var sync = function () {
      var state = video.paused ? (started ? 'paused' : 'idle') : 'playing';
      figure.setAttribute('data-state', state);
      toggle.setAttribute('aria-label', video.paused ? 'Play video' : 'Pause video');
    };

    var play = function () {
      var p = video.play();
      if (p && p.catch) p.catch(sync);
    };

    video.addEventListener('play', function () { started = true; sync(); });
    video.addEventListener('pause', sync);

    toggle.addEventListener('click', function () {
      if (video.paused) { userPaused = false; play(); }
      else { userPaused = true; video.pause(); }
    });
    video.addEventListener('click', function () { toggle.click(); });

    /* Full screen: the burned-in captions are small on phones. Native controls appear while full screen. */
    var fsButton = document.getElementById('showcase-fs');
    var enterFs = video.requestFullscreen || video.webkitRequestFullscreen || video.webkitEnterFullscreen;
    if (fsButton && enterFs) {
      fsButton.hidden = false;
      fsButton.addEventListener('click', function () {
        userPaused = false;
        play();
        enterFs.call(video);
      });
      document.addEventListener('fullscreenchange', function () {
        video.controls = document.fullscreenElement === video;
      });
    }

    if ('IntersectionObserver' in window) {
      new IntersectionObserver(function (entries) {
        entries.forEach(function (entry) {
          if (entry.isIntersecting) { if (!userPaused) play(); }
          else if (!video.paused) video.pause();
        });
      }, { threshold: 0.35 }).observe(video);
    } else if (!userPaused) {
      play();
    }

    sync();
  }

  /* ---------- Footer year ---------- */

  var year = document.getElementById('year');
  if (year) year.textContent = String(new Date().getFullYear());
})();
