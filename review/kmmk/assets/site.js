/* KMMK website template — shared behaviour. Each block only runs on pages
   that contain the elements it needs. */
(function () {
  var reduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  var $ = function (s, r) { return (r || document).querySelector(s); };
  var $$ = function (s, r) { return [].slice.call((r || document).querySelectorAll(s)); };

  // ---------- Navigation: mobile menu, current page, dark over black sections ----------
  var nav = $('#nav');
  if (nav) {
    var btn = $('.nav-toggle', nav);
    btn.addEventListener('click', function () {
      var open = nav.classList.toggle('open');
      btn.setAttribute('aria-expanded', String(open));
    });
    var here = location.pathname.split('/').pop() || 'index.html';
    $$('.nav-links a', nav).forEach(function (a) {
      if (a.getAttribute('href') === here) a.setAttribute('aria-current', 'page');
    });
  }
  var darks = $$('.black, footer');
  var corner = $('.corner-label');
  function overDark(y) {
    return darks.some(function (s) { var r = s.getBoundingClientRect(); return r.top <= y && r.bottom >= y; });
  }
  function updateTone() {
    if (nav) nav.classList.toggle('dark', overDark(nav.offsetHeight / 2));
    if (corner) corner.classList.toggle('on-dark', overDark(corner.getBoundingClientRect().top + 8));
  }
  window.addEventListener('scroll', updateTone, { passive: true });
  window.addEventListener('resize', updateTone);
  updateTone();

  // ---------- Reveal on scroll ----------
  var reveals = $$('.reveal');
  if ('IntersectionObserver' in window) {
    var rio = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) { if (e.isIntersecting) { e.target.classList.add('in'); rio.unobserve(e.target); } });
    }, { rootMargin: '0px 0px -10% 0px' });
    reveals.forEach(function (e) { rio.observe(e); });
  } else reveals.forEach(function (e) { e.classList.add('in'); });

  // ---------- Frame sequences ----------
  // Loads frames START..END of a sequence made by review/add-sequence.sh.
  // Phones (or small canvases) get the lighter m/ set.
  // data-crop="x1,y1,x2,y2" (fractions of the frame) draws only the area the
  // product ever occupies, so the canvas carries no empty margin.
  function cropOf(canvas) {
    return (canvas.dataset.crop || '0,0,1,1').split(',').map(Number);
  }
  function sequence(canvas, onLoad) {
    var seq = canvas.dataset.seq, start = +canvas.dataset.start || 0, end = +canvas.dataset.end;
    var full = +canvas.dataset.full, lite = +canvas.dataset.lite, ratio = +canvas.dataset.ratio || 1;
    var crop = cropOf(canvas), cw = crop[2] - crop[0], ch = crop[3] - crop[1];
    var cssW = canvas.getBoundingClientRect().width || 600;
    var useLite = lite && cssW * Math.min(window.devicePixelRatio || 1, 2) / cw <= lite * 1.15;
    var w = useLite ? lite : full, h = Math.round(w * ratio);
    var sx = crop[0] * w, sy = crop[1] * h, sw = cw * w, sh = ch * h;
    canvas.width = Math.round(sw); canvas.height = Math.round(sh);
    var base = seq + (useLite ? 'm/' : '');
    var n = end - start + 1, frames = new Array(n), loaded = new Array(n).fill(false), count = 0;
    var order = [0], seen = { 0: 1 };
    for (var step = 16; step >= 1; step >>= 1)
      for (var k = 0; k < n; k += step) if (!seen[k]) { seen[k] = 1; order.push(k); }
    var next = 0, inFlight = 0;
    (function pump() {
      while (inFlight < 6 && next < order.length) {
        (function (i) {
          inFlight++;
          var img = new Image();
          img.onload = function () { frames[i] = img; loaded[i] = true; count++; inFlight--; onLoad(i, count === n); pump(); };
          img.onerror = function () { count++; inFlight--; onLoad(i, count === n); pump(); };
          img.src = base + 'frame-' + String(start + i).padStart(4, '0') + '.webp';
        })(order[next++]);
      }
    })();
    var ctx = canvas.getContext('2d'), shown = -1;
    return {
      n: n,
      draw: function (i, force) {
        var f = i;
        if (!loaded[f]) {
          f = -1;
          for (var d = 1; d < n; d++) {
            if (i - d >= 0 && loaded[i - d]) { f = i - d; break; }
            if (i + d < n && loaded[i + d]) { f = i + d; break; }
          }
        }
        if (f < 0 || (f === shown && !force)) return;
        shown = f;
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        ctx.drawImage(frames[f], sx, sy, sw, sh, 0, 0, canvas.width, canvas.height);
      }
    };
  }

  // ---------- Home: the main animation plays on its own, looping ----------
  var homeCanvas = $('.home-canvas');
  if (homeCanvas) {
    var fps = +homeCanvas.dataset.fps || 30, hold = 1200;
    var ready = false, frame = 0, last = 0, holdUntil = 0, visible = true, raf = 0;
    var player = sequence(homeCanvas, function (i, all) {
      if (i === 0 || frame === 0) player.draw(0, true);
      if (all) { ready = true; if (reduced) player.draw(Math.round(player.n * 0.45), true); else play(); }
    });
    function loop(t) {
      raf = 0;
      if (!visible) return;
      if (t >= holdUntil && t - last >= 1000 / fps) {
        last = t;
        frame++;
        if (frame >= player.n) { frame = 0; holdUntil = t + hold; }
        else if (frame === player.n - 1) holdUntil = t + hold;
        player.draw(frame);
      }
      raf = requestAnimationFrame(loop);
    }
    function play() { if (ready && !reduced && !raf) raf = requestAnimationFrame(loop); }
    if ('IntersectionObserver' in window) {
      new IntersectionObserver(function (e) { visible = e[0].isIntersecting; play(); }).observe(homeCanvas);
    }
  }

  // ---------- SP Cables: scroll animation + typewriter text ----------
  var cableTrack = $('.cable-hero-track');
  if (cableTrack) {
    var cc = $('.cable-canvas'), box = $('.cable-canvas-box'), stage = $('.cable-hero-stage');
    // Fit the canvas into the space left under the text
    function fitCanvas() {
      var r = +cc.dataset.ratio || 1, bw = box.clientWidth, bh = box.clientHeight;
      var c = cropOf(cc), cr = (c[3] - c[1]) * r / (c[2] - c[0]);
      // Old size: the whole frame fitted in the box. Now the product is drawn
      // at twice that size, limited by the space available.
      var old = Math.min(bw, bh / r, 1100) * (c[2] - c[0]);
      var w = Math.min(old * (+cc.dataset.scale || 1), bw, bh / cr);
      cc.style.width = w + 'px'; cc.style.height = w * cr + 'px';
    }
    fitCanvas();
    // Split typed text into per-character spans
    var chars = [];
    $$('.typed [data-type]').forEach(function (el) {
      var text = el.textContent; el.textContent = '';
      el.setAttribute('aria-label', text);
      text.split(/(\s+)/).forEach(function (word) {
        if (!word) return;
        if (/^\s+$/.test(word)) { el.appendChild(document.createTextNode(word)); return; }
        var w = document.createElement('span');
        w.style.whiteSpace = 'nowrap';
        w.setAttribute('aria-hidden', 'true');
        for (var c = 0; c < word.length; c++) {
          var s = document.createElement('span'); s.className = 'ch'; s.textContent = word[c];
          w.appendChild(s); chars.push(s);
        }
        el.appendChild(w);
      });
    });
    var typedCount = 0;
    function typeTo(n) {
      if (n > typedCount) for (var i = typedCount; i < n; i++) chars[i].classList.add('on');
      else for (var j = n; j < typedCount; j++) chars[j].classList.remove('on');
      typedCount = n;
    }
    var cablePos = 0, cableRaf = 0;
    var cable = sequence(cc, function () { cable.draw(Math.round(cablePos)); });
    function tick() {
      cableRaf = 0;
      var p = Math.min(1, Math.max(0, -cableTrack.getBoundingClientRect().top / (cableTrack.offsetHeight - stage.offsetHeight)));
      typeTo(Math.round(chars.length * Math.min(1, p / 0.7)));
      var target = p * (cable.n - 1);
      cablePos += (target - cablePos) * 0.22;
      if (Math.abs(target - cablePos) < 0.05) cablePos = target;
      cable.draw(Math.round(cablePos));
      if (cablePos !== target) cableRaf = requestAnimationFrame(tick);
    }
    function kick() { if (!cableRaf) cableRaf = requestAnimationFrame(tick); }
    if (reduced) typeTo(chars.length);
    else { window.addEventListener('scroll', kick, { passive: true }); kick(); }
    window.addEventListener('resize', function () { fitCanvas(); cable.draw(Math.round(cablePos), true); kick(); });
  }

  // ---------- Vertical series dots (SP Cables) ----------
  var seriesNav = $('.series-dots');
  if (seriesNav) {
    var series = $$('[data-series]');
    series.forEach(function (s) {
      var a = document.createElement('a');
      a.href = '#' + s.id;
      a.textContent = s.dataset.series;
      a.setAttribute('aria-label', s.dataset.series + ' Series');
      seriesNav.appendChild(a);
    });
    var links = $$('a', seriesNav);
    function updateSeries() {
      var mid = window.innerHeight / 2, active = -1;
      var first = series[0].getBoundingClientRect(), lastR = series[series.length - 1].getBoundingClientRect();
      seriesNav.classList.toggle('show', first.top < window.innerHeight * 0.8 && lastR.bottom > window.innerHeight * 0.2);
      series.forEach(function (s, i) { if (s.getBoundingClientRect().top <= mid) active = i; });
      if (active < 0) active = 0;
      links.forEach(function (a, i) { a.setAttribute('aria-current', String(i === active)); });
    }
    window.addEventListener('scroll', updateSeries, { passive: true });
    window.addEventListener('resize', updateSeries);
    updateSeries();
  }

  // ---------- Cable configuration carousels ----------
  var arrowSvg = function (path) {
    return '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="' + path + '"/></svg>';
  };
  var PREV = 'M10 3 5 8l5 5', NEXT = 'M6 3l5 5-5 5';
  $$('.cable-row').forEach(function (row) {
    var stage = $('.config-stage', row);
    var imgs = $$('img', stage);
    var dots = $('.dots', row);
    var names = $$('.config-name', row);
    var texts = $$('.config-texts > p', row);
    var i = 0, timer = 0, touched = false;

    imgs.forEach(function (img, n) {
      var b = document.createElement('button');
      b.type = 'button';
      b.setAttribute('aria-label', img.dataset.name);
      b.setAttribute('aria-pressed', String(n === 0));
      b.addEventListener('click', function () { user(n); });
      dots.appendChild(b);
    });
    var dotBtns = $$('button', dots);

    function makeArrow(dir) {
      var a = document.createElement('button');
      a.type = 'button';
      a.className = 'arrow';
      a.setAttribute('aria-label', dir < 0 ? 'Previous configuration' : 'Next configuration');
      a.innerHTML = arrowSvg(dir < 0 ? PREV : NEXT);
      a.addEventListener('click', function () { user((i + dir + imgs.length) % imgs.length); });
      return a;
    }
    // Picture controls: ‹ dots ›
    var navWrap = document.createElement('div');
    navWrap.className = 'config-nav';
    dots.parentNode.insertBefore(navWrap, dots);
    navWrap.appendChild(makeArrow(-1));
    navWrap.appendChild(dots);
    navWrap.appendChild(makeArrow(1));
    // Text controls on the same line: ‹ 01A ›
    var head = $('.config-panel .head', row);
    if (head) {
      head.insertBefore(makeArrow(-1), head.firstChild);
      head.appendChild(makeArrow(1));
    }

    function show(n) {
      imgs[i].classList.remove('on'); dotBtns[i].setAttribute('aria-pressed', 'false');
      if (texts[i]) texts[i].classList.remove('on');
      i = n;
      imgs[i].classList.add('on'); dotBtns[i].setAttribute('aria-pressed', 'true');
      if (texts[i]) texts[i].classList.add('on');
      names.forEach(function (el) { el.textContent = imgs[i].dataset.name; });
    }
    function user(n) { touched = true; clearInterval(timer); timer = 0; show(n); }
    show(0);
    if (reduced || !('IntersectionObserver' in window)) return;
    new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        if (e.isIntersecting && !timer && !touched) timer = setInterval(function () { show((i + 1) % imgs.length); }, 3200);
        else if (!e.isIntersecting) { clearInterval(timer); timer = 0; }
      });
    }, { threshold: 0.5 }).observe(stage);
  });

  // ---------- Exploded-view gallery arrows ----------
  var gallery = $('#gallery');
  if (gallery) {
    $$('.gallery-nav button').forEach(function (b) {
      b.addEventListener('click', function () {
        gallery.scrollBy({ left: (gallery.querySelector('figure').offsetWidth + 24) * +b.dataset.dir, behavior: 'smooth' });
      });
    });
  }

  // ---------- Contact form (template: no handler connected yet) ----------
  var form = $('.form');
  if (form) {
    form.addEventListener('submit', function (e) {
      e.preventDefault();
      $('.form-note', form).textContent = 'Preview only — the form will be connected on the live site.';
    });
  }
})();
