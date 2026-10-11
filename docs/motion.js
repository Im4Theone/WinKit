/* ==========================================================================
   WinKit - motion layer
   Lenis smooth scrolling, GSAP + ScrollTrigger reveals, and three React Bits components rendered with React:
   SplitText (hero title), ScrollReveal (statement) and SpotlightCard (cards).

   Everything here only enhances content that is already in the HTML, so search engines, readers without
   JavaScript and people who prefer reduced motion get the full page.

   React Bits components (https://reactbits.dev) are ported from their JSX source to plain React.createElement
   calls so the site needs no build step. Copyright (c) 2026 David Haz. Used under the MIT + Commons Clause
   License Condition v1.0 as part of this website; the full notice is in vendor/REACT-BITS-LICENSE.md.
   ========================================================================== */

(function () {
  'use strict';

  var doc = document.documentElement;
  var reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  var gsap = window.gsap, ScrollTrigger = window.ScrollTrigger, GSAPSplitText = window.SplitText;

  if (!gsap || !ScrollTrigger) { doc.classList.remove('anim'); return; }
  window.__wkMotion = true;
  gsap.registerPlugin(ScrollTrigger);
  if (GSAPSplitText) gsap.registerPlugin(GSAPSplitText);

  /* ---------- Lenis smooth scrolling, driven by GSAP's ticker so ScrollTrigger stays in sync ---------- */

  var lenis = null;
  if (!reduce && window.Lenis) {
    lenis = new window.Lenis({ autoRaf: false, anchors: { offset: -72 }, lerp: 0.12 });
    lenis.on('scroll', ScrollTrigger.update);
    gsap.ticker.add(function (time) { lenis.raf(time * 1000); });
    gsap.ticker.lagSmoothing(0);
    window.__wkLenis = lenis;
  }

  /* ---------- Reveal on scroll (elements with .reveal) ---------- */

  var reveal = function (els) {
    gsap.to(els, { opacity: 1, y: 0, scale: 1, duration: 0.9, ease: 'power3.out', stagger: 0.08, overwrite: true, clearProps: 'transform' });
  };
  if (doc.classList.contains('anim')) {
    var items = gsap.utils.toArray('.reveal');
    var initial = items.filter(function (el) { return el.getBoundingClientRect().top < window.innerHeight * 0.92; });
    reveal(initial);
    ScrollTrigger.batch(items.filter(function (el) { return initial.indexOf(el) < 0; }), {
      start: 'top 90%', once: true, onEnter: reveal, onEnterBack: reveal
    });

    // hero logo and the showcase video frame
    gsap.from('.hero-logo', { opacity: 0, scale: 0.8, rotate: -8, duration: 1.1, ease: 'back.out(1.6)' });
    gsap.fromTo('.showcase-frame', { scale: 0.93 }, {
      scale: 1, ease: 'none',
      scrollTrigger: { trigger: '.shot-section', start: 'top bottom', end: 'top 25%', scrub: true }
    });
  }

  /* ---------- React Bits components ---------- */

  var React = window.React, ReactDOM = window.ReactDOM;
  if (!React || !ReactDOM || !ReactDOM.createRoot) return;
  var h = React.createElement;
  var useRef = React.useRef, useEffect = React.useEffect, useLayoutEffect = React.useLayoutEffect, useState = React.useState;

  /* React Bits: SplitText (GSAP SplitText + ScrollTrigger). useGSAP is replaced by useLayoutEffect + gsap.context. */
  function SplitText(props) {
    var text = props.text, className = props.className || '', delay = props.delay == null ? 50 : props.delay;
    var duration = props.duration == null ? 1.25 : props.duration, ease = props.ease || 'power3.out';
    var splitType = props.splitType || 'chars', from = props.from || { opacity: 0, y: 40 }, to = props.to || { opacity: 1, y: 0 };
    var threshold = props.threshold == null ? 0.1 : props.threshold, rootMargin = props.rootMargin || '-100px';
    var textAlign = props.textAlign || 'center', Tag = props.tag || 'p';
    var ref = useRef(null), done = useRef(false);
    var fonts = useState(document.fonts ? document.fonts.status === 'loaded' : true), fontsLoaded = fonts[0], setFontsLoaded = fonts[1];

    useEffect(function () {
      if (!fontsLoaded && document.fonts) document.fonts.ready.then(function () { setFontsLoaded(true); });
    }, []);

    useLayoutEffect(function () {
      var el = ref.current;
      if (!el || !text || !fontsLoaded || done.current || !GSAPSplitText) return undefined;
      var startPct = (1 - threshold) * 100;
      var m = /^(-?\d+(?:\.\d+)?)(px|em|rem|%)?$/.exec(rootMargin);
      var mv = m ? parseFloat(m[1]) : 0, mu = m ? m[2] || 'px' : 'px';
      var sign = mv === 0 ? '' : mv < 0 ? '-=' + Math.abs(mv) + mu : '+=' + mv + mu;
      var start = 'top ' + startPct + '%' + sign;
      var instance;
      var ctx = gsap.context(function () {
        instance = new GSAPSplitText(el, {
          type: splitType, smartWrap: true, autoSplit: splitType === 'lines', linesClass: 'split-line',
          wordsClass: 'split-word', charsClass: 'split-char', reduceWhiteSpace: false,
          onSplit: function (self) {
            var targets = (splitType.indexOf('chars') >= 0 && self.chars.length && self.chars) ||
              (splitType.indexOf('words') >= 0 && self.words.length && self.words) ||
              (splitType.indexOf('lines') >= 0 && self.lines.length && self.lines) || self.chars || self.words || self.lines;
            return gsap.fromTo(targets, Object.assign({}, from), Object.assign({}, to, {
              duration: duration, ease: ease, stagger: delay / 1000,
              scrollTrigger: { trigger: el, start: start, once: true, fastScrollEnd: true, anticipatePin: 0.4 },
              onComplete: function () { done.current = true; },
              willChange: 'transform, opacity', force3D: true
            }));
          }
        });
      }, ref);
      return function () { ctx.revert(); try { instance && instance.revert(); } catch (e) { /* noop */ } };
    }, [text, fontsLoaded]);

    return h(Tag, {
      ref: ref, className: 'split-parent ' + className,
      style: { textAlign: textAlign, overflow: 'hidden', display: 'inline-block', whiteSpace: 'normal', wordWrap: 'break-word', willChange: 'transform, opacity' }
    }, text);
  }

  /* React Bits: ScrollReveal (words fade and un-blur as the text scrolls through the viewport). */
  function ScrollReveal(props) {
    var text = props.text, enableBlur = props.enableBlur !== false, baseOpacity = props.baseOpacity == null ? 0.1 : props.baseOpacity;
    var baseRotation = props.baseRotation == null ? 3 : props.baseRotation, blurStrength = props.blurStrength == null ? 4 : props.blurStrength;
    var rotationEnd = props.rotationEnd || 'bottom bottom', wordAnimationEnd = props.wordAnimationEnd || 'bottom bottom';
    var ref = useRef(null);

    useEffect(function () {
      var el = ref.current;
      if (!el) return undefined;
      var ctx = gsap.context(function () {
        gsap.fromTo(el, { transformOrigin: '0% 50%', rotate: baseRotation }, {
          ease: 'none', rotate: 0, scrollTrigger: { trigger: el, start: 'top bottom', end: rotationEnd, scrub: true }
        });
        var words = el.querySelectorAll('.word');
        gsap.fromTo(words, { opacity: baseOpacity, willChange: 'opacity' }, {
          ease: 'none', opacity: 1, stagger: 0.05,
          scrollTrigger: { trigger: el, start: 'top bottom-=20%', end: wordAnimationEnd, scrub: true }
        });
        if (enableBlur) {
          gsap.fromTo(words, { filter: 'blur(' + blurStrength + 'px)' }, {
            ease: 'none', filter: 'blur(0px)', stagger: 0.05,
            scrollTrigger: { trigger: el, start: 'top bottom-=20%', end: wordAnimationEnd, scrub: true }
          });
        }
      }, ref);
      return function () { ctx.revert(); };
    }, []);

    var parts = text.split(/(\s+)/).map(function (word, i) {
      return /^\s+$/.test(word) ? word : h('span', { className: 'word', key: i }, word);
    });
    return h('span', { ref: ref, className: 'scroll-reveal' }, h('span', { className: 'scroll-reveal-text' }, parts));
  }

  /* React Bits: SpotlightCard (a soft light that follows the pointer, with a glowing rim). */
  var THEMES = {
    dark: { surface: '#111111', border: 'rgba(255, 255, 255, 0.08)', shadow: '0 24px 48px -24px rgba(0, 0, 0, 0.6)', light: '#ffffff', fill: 1, edge: 1 },
    light: { surface: '#ffffff', border: 'rgba(24, 24, 27, 0.1)', shadow: '0 1px 2px rgba(24, 24, 27, 0.04), 0 18px 40px -20px rgba(24, 24, 27, 0.16)', light: '#18181b', fill: 0.28, edge: 0.7 }
  };
  var clamp = function (v, a, b) { return Math.min(b, Math.max(a, v)); };
  var falloff = function (color, softness) {
    var soft = clamp(softness, 0, 1), core = Math.pow(1 - soft, 1.5) * 0.85;
    var stop = function (alpha, at) { return 'color-mix(in srgb, ' + color + ' ' + (alpha * 100).toFixed(2) + '%, transparent) ' + (at * 100).toFixed(2) + '%'; };
    var list = [stop(1, 0)];
    for (var i = 0; i <= 14; i++) {
      var t = core + ((1 - core) * i) / 14, g = (t - core) / (1 - core);
      var smooth = 1 - g * g * (3 - 2 * g), glow = Math.exp(-4.6 * g * g) * (1 - Math.pow(g, 6));
      list.push(stop(smooth + (glow - smooth) * soft, t));
    }
    return list.join(', ');
  };

  function SpotlightCard(props) {
    var palette = THEMES[props.theme] || THEMES.dark, color = props.spotlightColor || palette.light;
    var s = {
      fill: falloff(color, props.softness == null ? 0.7 : props.softness), edge: falloff(color, 1),
      size: Math.max(20, props.spotlightSize || 240), intensity: clamp(props.intensity == null ? 0.15 : props.intensity, 0, 1) * palette.fill,
      borderGlow: clamp(props.borderGlow == null ? 0.6 : props.borderGlow, 0, 1) * palette.edge,
      proximity: Math.max(0, props.proximity == null ? 80 : props.proximity), smoothing: clamp(props.smoothing == null ? 0.3 : props.smoothing, 0, 1)
    };
    var rootRef = useRef(null), lightRef = useRef(null), edgeRef = useRef(null), sRef = useRef(s);
    sRef.current = s;

    useEffect(function () {
      var root = rootRef.current, light = lightRef.current, edge = edgeRef.current;
      if (!root || !light || !edge) return undefined;
      var st = { x: 0, y: 0, presence: 0 }, pointer = { x: 0, y: 0, active: false }, focus = null, raf = 0, last = 0, alive = true;
      var grad = function (size, x, y, stops) { return 'radial-gradient(circle ' + size.toFixed(1) + 'px at ' + x.toFixed(1) + 'px ' + y.toFixed(1) + 'px, ' + stops + ')'; };
      var tick = function (now) {
        raf = 0; if (!alive) return;
        var c = sRef.current, dt = Math.min(0.05, Math.max(0.001, (now - last) / 1000)); last = now;
        var r = root.getBoundingClientRect(), tx = st.x, ty = st.y, target = 0;
        if (focus && root.contains(focus)) {
          var b = focus.getBoundingClientRect(); tx = b.left + b.width / 2 - r.left; ty = b.top + b.height / 2 - r.top; target = 1;
        } else if (pointer.active) {
          tx = pointer.x - r.left; ty = pointer.y - r.top;
          var gap = Math.hypot(Math.max(0, -tx, tx - r.width), Math.max(0, -ty, ty - r.height));
          target = gap === 0 ? 1 : c.proximity > 0 ? Math.pow(Math.max(0, 1 - gap / c.proximity), 2) : 0;
        }
        if (st.presence < 0.02 && target > 0) { st.x = tx; st.y = ty; }
        var follow = reduce || c.smoothing === 0 ? 1 : 1 - Math.exp(-dt / (0.015 + c.smoothing * 0.22));
        st.x += (tx - st.x) * follow; st.y += (ty - st.y) * follow;
        var fade = reduce ? 1 : 1 - Math.exp(-dt / (target > st.presence ? 0.1 : 0.32));
        st.presence += (target - st.presence) * fade;
        if (Math.abs(target - st.presence) < 0.002) st.presence = target;
        light.style.background = grad(c.size, st.x, st.y, c.fill);
        edge.style.background = grad(c.size * 0.6, st.x, st.y, c.edge);
        light.style.opacity = String(Math.min(1, st.presence * c.intensity));
        edge.style.opacity = String(Math.min(1, st.presence * c.borderGlow));
        var moving = Math.abs(tx - st.x) > 0.1 || Math.abs(ty - st.y) > 0.1;
        if (moving || st.presence !== target) raf = requestAnimationFrame(tick);
      };
      var wake = function () { if (raf || !alive) return; last = performance.now(); raf = requestAnimationFrame(tick); };
      var onMove = function (e) { pointer.x = e.clientX; pointer.y = e.clientY; pointer.active = true; wake(); };
      var onOut = function (e) { if (e.relatedTarget) return; pointer.active = false; wake(); };
      var onFocusIn = function (e) { if (e.target instanceof Element && e.target.matches(':focus-visible')) { focus = e.target; wake(); } };
      var onFocusOut = function (e) { if (!root.contains(e.relatedTarget)) { focus = null; wake(); } };
      window.addEventListener('pointermove', onMove, { passive: true });
      document.addEventListener('pointerout', onOut);
      root.addEventListener('focusin', onFocusIn);
      root.addEventListener('focusout', onFocusOut);
      return function () {
        alive = false; cancelAnimationFrame(raf);
        window.removeEventListener('pointermove', onMove); document.removeEventListener('pointerout', onOut);
        root.removeEventListener('focusin', onFocusIn); root.removeEventListener('focusout', onFocusOut);
      };
    }, []);

    return h('div', {
      ref: rootRef, className: 'spotlight-card',
      style: { '--spotlight-card-surface': palette.surface, '--spotlight-card-border': palette.border, '--spotlight-card-shadow': palette.shadow }
    },
      h('span', { ref: lightRef, className: 'spotlight-card__light', 'aria-hidden': 'true' }),
      h('div', { className: 'spotlight-card__content', dangerouslySetInnerHTML: { __html: props.html } }),
      h('span', { className: 'spotlight-card__border', 'aria-hidden': 'true' }),
      h('span', { ref: edgeRef, className: 'spotlight-card__edge', 'aria-hidden': 'true' }));
  }

  /* ---------- Mount the components on the existing markup ---------- */

  var mount = function (el, element) { el.classList.add('rb-mounted'); ReactDOM.createRoot(el).render(element); };

  if (!reduce) {
    document.querySelectorAll('[data-rb="split-text"]').forEach(function (el) {
      mount(el, h(SplitText, { text: el.textContent.trim(), tag: 'span', splitType: 'chars', delay: 60, duration: 1.1, threshold: 0, rootMargin: '0px', from: { opacity: 0, y: 60 }, to: { opacity: 1, y: 0 }, textAlign: 'left' }));
    });
    document.querySelectorAll('[data-rb="scroll-reveal"]').forEach(function (el) {
      mount(el, h(ScrollReveal, { text: el.textContent.trim(), baseOpacity: 0.12, baseRotation: 2, blurStrength: 5, rotationEnd: 'bottom 70%', wordAnimationEnd: 'bottom 70%' }));
    });
  }
  // the spotlight follows the pointer only, so it stays on with reduced motion (it moves instantly instead of easing)
  if (window.matchMedia('(hover: hover)').matches) {
    document.querySelectorAll('[data-rb="spotlight"]').forEach(function (el) {
      mount(el, h(SpotlightCard, { html: el.innerHTML, theme: 'light', spotlightColor: '#5b6f86', intensity: 0.5, spotlightSize: 260, borderGlow: 0.9 }));
    });
  }

  window.addEventListener('load', function () { ScrollTrigger.refresh(); });
})();
