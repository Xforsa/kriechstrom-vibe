// SPDX-License-Identifier: GPL-3.0-only
// KRIECHSTROM for VIBE FM, the site (the hero is a picture: tools/make_hero.py, its strike in style.css).
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
