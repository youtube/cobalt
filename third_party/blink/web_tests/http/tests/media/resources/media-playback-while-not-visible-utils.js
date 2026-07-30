function queryPlayerStatus(iframe) {
  return new Promise(resolve => {
    window.addEventListener('message', function handler(event) {
      if (event.data.type === 'queryPlayerStatus') {
        window.removeEventListener('message', handler);
        resolve(event.data.status);
      }
    });
    iframe.contentWindow.postMessage({action: 'queryPlayerStatus'}, '*');
  });
}

function playMediaInIframe(iframe) {
  return new Promise(resolve => {
    window.addEventListener('message', function handler(event) {
      if (event.data.type === 'play') {
        window.removeEventListener('message', handler);
        resolve(event.data.status);
      }
    });
    iframe.contentWindow.postMessage({action: 'play'}, '*');
  });
}

function pauseMediaInIframe(iframe) {
  return new Promise(resolve => {
    window.addEventListener('message', function handler(event) {
      if (event.data.type === 'pause') {
        window.removeEventListener('message', handler);
        resolve(event.data.status);
      }
    });
    iframe.contentWindow.postMessage({action: 'pause'}, '*');
  });
}

function expectMediaPlayerStateChangeInIframe() {
  return new Promise(resolve => {
    function handler(event) {
      if (event.data.type === 'statechange') {
        window.removeEventListener('message', handler);
        resolve(event.data.newState);
      }
    }

    window.addEventListener('message', handler);
    setTimeout(() => {
      window.removeEventListener('message', handler);
      resolve('Timed out waiting for statechange');
    }, 2000);
  });
}

function hideFrame(iframe, type) {
  if (type === 'display') {
    iframe.style.setProperty('display', 'none');
  } else if (type === 'visibility') {
    iframe.style.setProperty('visibility', 'hidden');
  } else if (type === 'zeroSize') {
    iframe.style.setProperty('width', '0');
    iframe.style.setProperty('height', '0');
  }
}

function showFrame(iframe, type) {
  if (type === 'display') {
    iframe.style.setProperty('display', 'block');
  } else if (type === 'visibility') {
    iframe.style.setProperty('visibility', 'visible');
  } else if (type === 'zeroSize') {
    iframe.style.removeProperty('width');
    iframe.style.removeProperty('height');
  }
}

// Creates a cross-origin iframe. If `type` is 'nested', the iframe will be
// created inside another intermediate same-origin (cross-origin in relation to
// the top-level document) iframe. Otherwise, the iframe will be created
// directly in the test page.
async function setUpCrossOriginIframe(t, type) {
  const iframe = document.createElement('iframe');
  if (type === 'nested') {
    iframe.id = 'intermediate-iframe';
    iframe.src =
        'http://localhost:8000/media/resources/media-playback-while-not-visible-intermediate-iframe.html';
  } else {
    iframe.allow = 'media-playback-while-not-visible \'none\'; autoplay *';
    iframe.src =
        'http://localhost:8000/media/resources/media-playback-while-not-visible-iframe.html';
  }
  document.body.appendChild(iframe);
  await new Promise(resolve => {
    iframe.addEventListener('load', resolve);
  });

  if (type === 'nested') {
    // Give some time for the nested iframe to load.
    await new Promise(resolve => setTimeout(resolve, 500));
  }

  t.add_cleanup(() => {
    document.body.removeChild(iframe);
  });

  return iframe;
}

// Creates an iframe and ensures the media player is paused.
async function setUpCrossOriginIframeAndPause(t, type) {
  if (document.readyState !== 'complete') {
    await new Promise(resolve => {window.addEventListener('load', resolve)})
  }

  const iframe = await setUpCrossOriginIframe(t, type);
  assert_equals(await pauseMediaInIframe(iframe), 'Success');

  return iframe;
}
