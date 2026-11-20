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
