(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Tests that setBreakpointByUrl works across navigations.`);

  await dp.Debugger.enable();

  const scripts = new Map();
  dp.Debugger.onScriptParsed(event => {
    const url = event.params.url;
    if (url.endsWith('/script-url.js')) {
      scripts.set('url', url);
    }
    if (url.endsWith('/script-hash.js')) {
      scripts.set('hash', event.params.hash);
    }
  });

  let pausesExpected = 0;
  let pausesSaw = 0;
  let pauseResolve = null;

  function expectPauses(count) {
    pausesExpected = count;
    pausesSaw = 0;
    return new Promise(resolve => {
      pauseResolve = resolve;
      if (count === 0) resolve();
    });
  }

  // Navigate to page 1 to discover scripts.
  await session.navigate('./resources/set-breakpoint-by-url-navigate-1.html');

  const scriptUrl = scripts.get('url');
  const hash = scripts.get('hash');
  testRunner.log(`Discovered scripts: ${scriptUrl && hash ? 'yes' : 'no'}`);

  testRunner.log('Setting breakpoints with conditions...');
  const bpUrl = await dp.Debugger.setBreakpointByUrl({
    url: scriptUrl,
    lineNumber: 0,
    condition: 'window.shouldPauseUrl'
  });
  const bpRegex = await dp.Debugger.setBreakpointByUrl({
    urlRegex: '.*script-regex\\.js',
    lineNumber: 0,
    condition: 'window.shouldPauseRegex'
  });
  const bpHash = await dp.Debugger.setBreakpointByUrl({
    scriptHash: hash,
    lineNumber: 0,
    condition: 'window.shouldPauseHash'
  });

  dp.Debugger.onPaused(async event => {
    const hit = event.params.hitBreakpoints ? event.params.hitBreakpoints[0] : null;
    let bpName = 'unknown';
    if (hit === bpUrl.result.breakpointId) bpName = 'by url';
    if (hit === bpRegex.result.breakpointId) bpName = 'by url regexp';
    if (hit === bpHash.result.breakpointId) bpName = 'by script hash';
    testRunner.log(`Hit breakpoint: ${bpName}`);
    pausesSaw++;
    if (pausesSaw === pausesExpected && pauseResolve) {
      pauseResolve();
    }
    await dp.Debugger.resume();
  });

  testRunner.log('Navigating to page 2 (should hit url and script hash breakpoints)...');
  const p2 = expectPauses(2);
  await session.navigate('./resources/set-breakpoint-by-url-navigate-2.html');
  await p2;

  testRunner.log('Navigating to page 3 (should hit url regexp breakpoint)...');
  const p3 = expectPauses(1);
  await session.navigate('./resources/set-breakpoint-by-url-navigate-3.html');
  await p3;

  testRunner.completeTest();
})
