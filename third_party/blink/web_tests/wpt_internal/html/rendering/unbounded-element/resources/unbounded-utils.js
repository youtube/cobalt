function visual_assert_true(condition, message) {
  if (condition) {
    return;
  }
  const failDiv = document.createElement('div');
  failDiv.style.color = 'red';
  failDiv.style.fontWeight = 'bold';
  failDiv.style.fontSize = '24px';
  failDiv.textContent = `FAIL: ${message || 'Assertion failed'}`;
  document.body.insertBefore(failDiv, document.body.firstChild);
}

async function unbounded_appearance_test(test_function) {
  visual_assert_true(document.documentElement.classList.contains('reftest-wait'),
      "HTML element must have reftest-wait class");
  try {
    await test_function();
  } catch (e) {
    visual_assert_true(false, e);
  } finally {
    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
    document.documentElement.classList.remove('reftest-wait');
  }
}

async function showUnboundedElement(element) {
  if (window.test_driver) {
    await test_driver.bless("show unbounded");
    await element.showUnboundedElement();
    visual_assert_true(element.matches(':unbounded'), "Element should match :unbounded");
  } else {
    const button = document.createElement('button');
    button.style.position = 'absolute';
    button.style.top = '5px';
    button.style.left = '5px';
    button.onclick = () => element.showUnboundedElement();
    setInterval(() => {
      const state = element.matches(':unbounded') ? 'Showing' : 'Hidden';
      button.textContent = `Show Unbounded (Manual, ${state})`;
    }, 100);
    document.body.appendChild(button);
  }
}

// Auto-run for simple script-free appearance tests
window.addEventListener('load', () => {
  const target = document.getElementById('target');
  if (!target || !target.hasAttribute('unbounded')) {
    return;
  }
  if (!document.documentElement.classList.contains('reftest-wait')) {
    return;
  }
  const hasInlineScripts = Array.from(document.querySelectorAll('script')).some(s => !s.src);
  if (hasInlineScripts) {
    return;
  }

  // Run the standard test
  unbounded_appearance_test(() => showUnboundedElement(target));
});
