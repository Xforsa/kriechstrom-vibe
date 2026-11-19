// SPDX-License-Identifier: GPL-3.0-only
// KRIECHSTROM for VIBE FM, the site: the discharge in the hero, grown as the firmware grows it (kriechstrom/kx_creep.h:
// the same seeded walk), then alive: it flickers, and now and then the picture tears as the VIBE's display does.
(() => {
  const cv = document.getElementById('creep');
  if (!cv) return;
  const still = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const vsin = (t) => Math.sin(t * 2 * Math.PI);
  // a discharge: the firmware's walk (kriechstrom/kx_creep.h), from its own seed, root and heading
  const grow = (seed, rx, ry, head, len, max) => {
    let rng = seed >>> 0;
    const randf = () => { rng ^= rng << 13; rng >>>= 0; rng ^= rng >>> 17; rng ^= rng << 5; rng >>>= 0; return (rng & 0xFFFF) / 65535; };
    const seg = [], queue = [[rx, ry, head, len, 0, 0]];
    while (queue.length && seg.length < max) {
      let [x, y, hd, l, depth, order] = queue.shift(), gone = 0, i = 0;
      while (gone < l && seg.length < max) {
        const step = (depth < 2 ? 7 : 4) + randf() * (depth < 2 ? 9 : 6);
        const a = hd + (randf() - 0.5) * (depth < 1 ? 0.16 : 0.24);
        hd += (randf() - 0.5) * 0.03;
        const nx = x + vsin(a + 0.25) * step, ny = y - vsin(a) * step;
        if (nx < -200 || nx > 1224 || ny < -200 || ny > 1224) break;
        seg.push([x, y, nx, ny, depth, Math.min(order + i, 255)]);
        x = nx; y = ny; gone += step; i++;
        if (depth < 7 && randf() < (depth < 2 ? 0.30 : 0.22) && queue.length < 1024)
          queue.push([x, y, hd + (randf() < 0.5 ? -1 : 1) * (0.05 + randf() * 0.12), (l - gone) * (0.35 + randf() * 0.35), depth + 1, order + i]);
      }
    }
    return seg;
  };
  // three electrodes: the foot (the firmware's figure), the top left corner, the right edge. Coordinates: a field
  // 1024 wide and FH high over the hero, scaled the same both ways
  const FH = 540;
  const figs = [
    { seg: grow(0x4B525354, 660, FH + 4, 0.25, 560, 2400), w: 1.0, delay: 0 },
    { seg: grow(0x5354524F, -6, -6, 0.90, 430, 1300), w: 0.75, delay: 0.5 },
    { seg: grow(0x4D4B5249, 1030, 150, 0.53, 380, 900), w: 0.7, delay: 0.9 },
  ];
  const ctx = cv.getContext('2d');
  const base = document.createElement('canvas'), bctx = base.getContext('2d');
  let W = 0, H = 0, dpr = 1, baked = false;
  const size = () => {
    dpr = Math.min(2, devicePixelRatio || 1); W = cv.clientWidth; H = cv.clientHeight;
    cv.width = base.width = W * dpr; cv.height = base.height = H * dpr; baked = false;
  };
  size();
  const col = (d, hot) => {
    const c = d === 0 ? [255, 240, 230] : d === 1 ? [245, 80, 50] : d < 4 ? [224, 32, 24] : [140, 30, 18];
    return `rgb(${Math.min(255, c[0] + hot * 60) | 0},${Math.min(255, c[1] + hot * 170) | 0},${Math.min(255, c[2] + hot * 160) | 0})`;
  };
  // the figures (each up to its own growth g), and around a pulse's front (fig, front) the hot ones on top
  const paint = (c, t, fig, front, hotOnly) => {
    const s = W * dpr / 1024, oy = H * dpr - FH * s, X = (x) => x * s, Y = (y) => oy + y * s;   /* (the foot at the bottom) */
    if (!hotOnly) { c.fillStyle = '#000'; c.fillRect(0, 0, W * dpr, H * dpr); }
    c.lineCap = 'round';
    figs.forEach((f, fi) => {
      const g = still ? 1 : Math.max(0, Math.min(1, (t - f.delay) / 2.2)), lim = 8 + g * 248;
      for (let pass = 0; pass < 2; pass++) {
        c.globalCompositeOperation = pass ? 'source-over' : 'lighter';
        for (const [x0, y0, x1, y1, d, o] of f.seg) {
          if (o > lim) continue;
          const hot = fi === fig ? Math.max(0, 1 - Math.abs(o - front) / 12) : 0;
          if (hotOnly && hot <= 0) continue;
          if (!pass) {
            if (d > 1 && !hot) continue;
            c.strokeStyle = `rgba(200,30,20,${(0.09 + 0.3 * hot) * f.w})`; c.lineWidth = (14 - 6 * Math.min(d, 1)) * s * (1 + hot);
          } else {
            c.globalAlpha = f.w + (1 - f.w) * hot;
            c.strokeStyle = o > lim - 8 ? 'rgb(90,20,12)' : col(d, hot);
            c.lineWidth = Math.max(0.6 * dpr, (d === 0 ? 4.4 : d === 1 ? 2.6 : d === 2 ? 1.6 : 1.0) * s * 1.6 * (1 + 0.6 * hot));
          }
          c.beginPath(); c.moveTo(X(x0), Y(y0)); c.lineTo(X(x1), Y(y1)); c.stroke();
          c.globalAlpha = 1;
        }
      }
    });
    c.globalCompositeOperation = 'source-over';
  };
  const t0 = performance.now();
  let fig = 0, timer = 0;
  const growFrame = (now) => {                     // the first 3 s: they grow, every frame
    const t = (now - t0) / 1000;
    paint(ctx, t, -1, -100, false);
    if (t < 3.3 && !still) requestAnimationFrame(growFrame);
    else { bake(); schedule(4000); }
  };
  const bake = () => { paint(bctx, 99, -1, -100, false); baked = true; ctx.drawImage(base, 0, 0); };
  const pulse = (which) => {                       // the current runs out from one root to the tips, ~1.1 s
    clearTimeout(timer);
    fig = which;
    const p0 = performance.now();
    const step = (now) => {
      const pt = (now - p0) / 1100;
      if (!baked) bake();
      ctx.drawImage(base, 0, 0);
      if (pt < 1) { paint(ctx, 99, fig, pt * 260, true); requestAnimationFrame(step); }
      else schedule(5500 + Math.random() * 3500);
    };
    requestAnimationFrame(step);
  };
  const schedule = (ms) => {
    clearTimeout(timer);
    if (still) return;
    timer = setTimeout(() => { if (document.visibilityState === 'visible') pulse((fig + 1 + (Math.random() * 2 | 0)) % figs.length); else schedule(3000); }, ms);
  };
  requestAnimationFrame(growFrame);
  addEventListener('resize', () => { size(); bake(); });
  cv.parentElement.addEventListener('pointerenter', () => { if (baked) pulse(0); });
})();

// the sections come in once as they scroll into view; the display's wear plays once when it is reached
(() => {
  const still = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const io = 'IntersectionObserver' in window ? new IntersectionObserver((es) => {
    for (const e of es) if (e.isIntersecting) {
      const el = e.target;
      el.classList.add('in');
      if (el.dataset.src) { el.dataset.play = el.dataset.src; el.src = el.dataset.src; delete el.dataset.src; }
      io.unobserve(el);
    }
  }, { threshold: 0.25 }) : null;
  for (const el of document.querySelectorAll('.reveal, img[data-src]')) {
    if (!io || still) { el.classList.add('in'); if (el.dataset.src) el.src = el.dataset.src; continue; }
    io.observe(el);
  }
})();

// the display's wear plays once; a click plays it again
for (const im of document.querySelectorAll('img.screen')) {
  im.style.cursor = 'pointer';
  im.addEventListener('click', () => { const u = im.dataset.play || im.src; im.src = 'media/wear-still.png'; requestAnimationFrame(() => { im.src = u + '?' + Date.now(); }); });
}
