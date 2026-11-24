// SPDX-License-Identifier: GPL-3.0-only
// KRIECHSTROM for VIBE FM, the site (the hero is a picture: tools/make_hero.py, its strike in style.css).
// The sections come in once as they scroll into view; nothing moves on its own after that.
(() => {
  const still = matchMedia('(prefers-reduced-motion: reduce)').matches;
  const io = 'IntersectionObserver' in window && !still ? new IntersectionObserver((es) => {
    for (const e of es) if (e.isIntersecting) { e.target.classList.add('in'); io.unobserve(e.target); }
  }, { threshold: 0.15 }) : null;
  for (const el of document.querySelectorAll('.reveal')) io ? io.observe(el) : el.classList.add('in');
})();
// a screen clicked: shown big (pixels kept sharp), its caption under it; the arrows, the keys or a swipe go to the
// next one of the same group; a click outside, or Esc, closes it
(() => {
  const groups = [...document.querySelectorAll('.shots, .manual-shots')];
  const all = [...document.querySelectorAll('.shots img, img.wide, img.shot-m, .split img.screen')];
  const listOf = (img) => { const g = img.closest('.shots'); return g ? [...g.querySelectorAll('img')] : all; };
  const capOf = (img) => { const c = img.closest('figure')?.querySelector('figcaption'); return c ? c.textContent : img.alt; };
  const open = (img) => {
    const list = listOf(img);
    let i = list.indexOf(img);
    const lb = document.createElement('div'), big = new Image(), p = document.createElement('p');
    const prev = document.createElement('button'), next = document.createElement('button');
    lb.className = 'lb';
    prev.className = 'lb-nav lb-prev', next.className = 'lb-nav lb-next';
    prev.textContent = '\u2039', next.textContent = '\u203A';
    prev.setAttribute('aria-label', 'previous'), next.setAttribute('aria-label', 'next');
    const show = () => {
      const im = list[i];
      big.src = im.currentSrc || im.src; big.alt = im.alt;
      if (im.naturalWidth) lb.style.setProperty('--ar', im.naturalWidth / im.naturalHeight);
      p.textContent = capOf(im) + (list.length > 1 ? '   ' + (i + 1) + ' / ' + list.length : '');
    };
    const go = (d) => { i = (i + d + list.length) % list.length; show(); };
    lb.append(big, p);
    if (list.length > 1) lb.append(prev, next);
    show();
    const close = () => { lb.remove(); removeEventListener('keydown', key); };
    const key = (e) => { if (e.key === 'Escape') close(); else if (e.key === 'ArrowRight') go(1); else if (e.key === 'ArrowLeft') go(-1); };
    prev.addEventListener('click', (e) => { e.stopPropagation(); go(-1); });
    next.addEventListener('click', (e) => { e.stopPropagation(); go(1); });
    big.addEventListener('click', (e) => { e.stopPropagation(); if (list.length > 1) go(1); else close(); });
    lb.addEventListener('click', close);
    let x0 = null;
    lb.addEventListener('touchstart', (e) => { x0 = e.touches[0].clientX; }, { passive: true });
    lb.addEventListener('touchend', (e) => {
      if (x0 === null) return;
      const dx = e.changedTouches[0].clientX - x0; x0 = null;
      if (Math.abs(dx) > 40) go(dx < 0 ? 1 : -1);
    });
    addEventListener('keydown', key);
    document.body.append(lb);
  };
  void groups;
  for (const img of all) img.addEventListener('click', () => open(img));
})();
