var interval = setInterval(function () {
  postMessage('1');
  // TODO (b/233788170): the below clearInterval line shouldn't be needed once
  // worker appropriately stops timers on freeze.
  clearInterval(interval);
}, 10);
