(function () {
  var btn = document.createElement('button');
  btn.className = 'to-top';
  btn.type = 'button';
  btn.setAttribute('aria-label', 'Back to top');
  btn.innerHTML = '↑';
  document.body.appendChild(btn);
  var ticking = false;
  function update() {
    if (window.scrollY > 500) btn.classList.add('visible');
    else btn.classList.remove('visible');
    ticking = false;
  }
  window.addEventListener('scroll', function () {
    if (!ticking) { requestAnimationFrame(update); ticking = true; }
  }, { passive: true });
  btn.addEventListener('click', function () {
    window.scrollTo({ top: 0, behavior: 'smooth' });
  });
  update();
})();
