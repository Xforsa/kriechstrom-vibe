// SPDX-License-Identifier: GPL-3.0-only
// KRIECHSTROM for VIBE FM, the site: the discharge in the hero, grown as the firmware grows it (kriechstrom/kx_creep.h:
// the same seeded walk), then alive: it flickers, and now and then the picture tears as the VIBE's display does.
(() => {
  const cv = document.getElementById('creep');
  if (!cv) return;
  const still = matchMedia('(prefers-reduced-motion: reduce)').matches;
  let rng = 0x4B525354;
  const rand = () => { rng ^= rng << 13; rng >>>= 0; rng ^= rng >>> 17; rng ^= rng << 5; rng >>>= 0; return rng; };
  const randf = () => (rand() & 0xFFFF) / 65535;
  const vsin = (t) => Math.sin(t * 2 * Math.PI);
  const seg = [], queue = [];
  const fork = (x, y, h, l, d, o) => { if (queue.length < 1024 && l > 6) queue.push([x, y, h, l, d, Math.min(o, 255)]); };
  const branch = (x, y, head, len, depth, order) => {
    let gone = 0, i = 0;
    while (gone < len && seg.length < 3000) {
      const step = (depth < 2 ? 7 : 4) + randf() * (depth < 2 ? 9 : 6);
      const a = head + (randf() - 0.5) * (depth < 1 ? 0.16 : 0.24);
      head += (randf() - 0.5) * 0.03;
      const nx = x + vsin(a + 0.25) * step, ny = y - vsin(a) * step;
      if (nx < 2 || nx > 1022 || ny < 2 || ny > 1022) return;
      seg.push([x, y, nx, ny, depth, Math.min(order + i, 255)]);
      x = nx; y = ny; gone += step; i++;
      if (depth < 7 && randf() < (depth < 2 ? 0.30 : 0.22))
        fork(x, y, head + (randf() < 0.5 ? -1 : 1) * (0.05 + randf() * 0.12), (len - gone) * (0.35 + randf() * 0.35), depth + 1, order + i);
    }
  };
  fork(512, 1020, 0.25, 900, 0, 0); fork(500, 1020, 0.34, 420, 1, 10); fork(524, 1020, 0.16, 420, 1, 10);
  while (queue.length && seg.length < 3000) branch(...queue.shift());

  const ctx = cv.getContext('2d');
  let W = 0, H = 0, dpr = 1;
  const size = () => { dpr = Math.min(2, devicePixelRatio || 1); W = cv.clientWidth; H = cv.clientHeight; cv.width = W * dpr; cv.height = H * dpr; };
  size(); addEventListener('resize', size);
  const col = (d, heat) => d === 0 ? `rgb(255,${200 + 40 * heat | 0},${190 + 50 * heat | 0})` : d === 1 ? 'rgb(245,80,50)' : d < 4 ? 'rgb(224,32,24)' : 'rgb(140,30,18)';
  const t0 = performance.now();
  let tearUntil = 0, tear = null;
  const frame = (now) => {
    const t = (now - t0) / 1000, grow = still ? 1 : Math.min(1, t / 2.2), lim = 8 + grow * 248;
    const heat = still ? 0.6 : 0.55 + 0.45 * Math.abs(Math.sin(t * 7.3) * Math.sin(t * 2.1));
    const s = Math.min(W, H * 1.1) / 1024 * dpr * 1.05, ox = (W * dpr - 1024 * s) * 0.62, oy = H * dpr - 1024 * s;
    ctx.fillStyle = '#000'; ctx.fillRect(0, 0, cv.width, cv.height);
    ctx.lineCap = 'round';
    ctx.globalCompositeOperation = 'lighter';
    for (const [x0, y0, x1, y1, d, o] of seg) {       // the glow under the two strongest
      if (o > lim || d > 1) continue;
      ctx.strokeStyle = `rgba(200,30,20,${0.10 + 0.12 * heat})`; ctx.lineWidth = (14 - 6 * d) * s;
      ctx.beginPath(); ctx.moveTo(ox + x0 * s, oy + y0 * s); ctx.lineTo(ox + x1 * s, oy + y1 * s); ctx.stroke();
    }
    ctx.globalCompositeOperation = 'source-over';
    for (const [x0, y0, x1, y1, d, o] of seg) {
      if (o > lim) continue;
      ctx.strokeStyle = o > lim - 8 ? 'rgb(90,20,12)' : col(d, heat);
      ctx.lineWidth = Math.max(0.6 * dpr, (d === 0 ? 4.4 : d === 1 ? 2.6 : d === 2 ? 1.6 : 1.0) * s * 1.6);
      ctx.beginPath(); ctx.moveTo(ox + x0 * s, oy + y0 * s); ctx.lineTo(ox + x1 * s, oy + y1 * s); ctx.stroke();
    }
    if (!still && grow >= 1) {                         // the tears: a band of the picture, shifted, for a moment
      if (now > tearUntil && Math.random() < 0.02) {
        tear = []; for (let k = 0; k < 2 + (Math.random() * 3 | 0); k++)
          tear.push([Math.random() * cv.height, (6 + Math.random() * 30) * dpr, (Math.random() * 60 - 30) * dpr]);
        tearUntil = now + 90 + Math.random() * 120;
      }
      if (now < tearUntil && tear) for (const [y, h, dx] of tear) ctx.drawImage(cv, 0, y, cv.width, h, dx, y, cv.width, h);
    }
    ctx.fillStyle = 'rgba(0,0,0,0.18)';               // the display's scanlines
    for (let y = 0; y < cv.height; y += 3 * dpr) ctx.fillRect(0, y, cv.width, dpr);
    if (!still) requestAnimationFrame(frame);
  };
  requestAnimationFrame(frame);
})();
