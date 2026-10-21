window.addEventListener('load', () => {
  fetch('/invalid_char.html').catch(err => {
    console.err(`fetch('/invalid_char.html') failed with error: ${err}`);
  });
});
