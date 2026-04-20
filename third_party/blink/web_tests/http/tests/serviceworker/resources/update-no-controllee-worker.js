const rejectAfter = (timeout) => {
  return new Promise((resolve, reject) => {
    setTimeout(() => reject(), timeout);
  });
}

const startUpdate = () => {
  let p = [];
  for (let i = 0; i < 10; i++) {
    p.push(self.registration.update());
  }
  return Promise.all(p);
};

const update = () => {
  // update() rejects in one of these cases:
  //   1. at least one update() rejects, or
  //   2. at least one update() does not resolve after 15 seconds.
  return Promise.race([startUpdate(), rejectAfter(150000)]);
};

self.addEventListener('message', (e) => {
  const port = e.data;

  port.onmessage = (e) => {
    update().then(() => {
      port.postMessage('success');
    }).catch((e) => {
      port.postMessage('failure');
    });
  };
});
