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
// a screen clicked: shown big (pixels kept sharp), its caption under it; a click or Esc closes it
(() => {
  const open = (img) => {
    const lb = document.createElement('div'), big = new Image(), cap = img.closest('figure')?.querySelector('figcaption');
    lb.className = 'lb';
    big.src = img.currentSrc || img.src;
    big.alt = img.alt;
    if (img.naturalWidth) lb.style.setProperty('--ar', img.naturalWidth / img.naturalHeight);
    lb.append(big);
    if (cap || img.alt) { const p = document.createElement('p'); p.textContent = cap ? cap.textContent : img.alt; lb.append(p); }
    const close = () => { lb.remove(); removeEventListener('keydown', key); };
    const key = (e) => { if (e.key === 'Escape') close(); };
    lb.addEventListener('click', close);
    addEventListener('keydown', key);
    document.body.append(lb);
  };
  for (const img of document.querySelectorAll('.shots img, img.wide, img.shot-m, .split img.screen'))
    img.addEventListener('click', () => open(img));
})();
