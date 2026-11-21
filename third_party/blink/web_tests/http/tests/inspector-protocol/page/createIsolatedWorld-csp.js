(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      'Verifies fetch() is blocked by CSP in isolated world and can be cleared');

  await dp.Runtime.enable();
  await dp.Page.enable();

  const getResourceTreeResponse = await dp.Page.getResourceTree();
  const mainFrameId = getResourceTreeResponse.result.frameTree.frame.id;

  // Case 1: Create the isolated world with a CSP that blocks network access.
  // Any fetch initiated from this world should fail.
  testRunner.log('Creating isolated world with CSP...');
  const isolatedWorldResponse = await dp.Page.createIsolatedWorld({
    frameId: mainFrameId,
    worldName: 'Test world',
    grantUniveralAccess: false,
    contentSecurityPolicy: 'connect-src \'none\''
  });
  const contextId = isolatedWorldResponse.result.executionContextId;

  const result = await dp.Runtime.evaluate({
    contextId: contextId,
    returnByValue: true,
    awaitPromise: true,
    expression: `
        fetch('/inspector-protocol/resources/test-page.html')
          .then(r => 'SUCCESS')
          .catch(e => 'FAILED: ' + e.toString())
    `
  });
  testRunner.log('Fetch result (with CSP): ' + result.result.result.value);

  // Case 2: Re-create the same world but omit the CSP parameter.
  // This clears the CSP in the global manager. However, because the current
  // document's window has already cached the CSP from Case 1, the fetch is
  // expected to still fail in this active context.
  testRunner.log('Re-creating isolated world without CSP (should clear it)...');
  const isolatedWorldResponse2 = await dp.Page.createIsolatedWorld({
    frameId: mainFrameId,
    worldName: 'Test world',
    grantUniveralAccess: false
  });
  const contextId2 = isolatedWorldResponse2.result.executionContextId;
  testRunner.log('Context IDs match: ' + (contextId === contextId2));

  const result2 = await dp.Runtime.evaluate({
    contextId: contextId2,
    returnByValue: true,
    awaitPromise: true,
    expression: `
        fetch('/inspector-protocol/resources/test-page.html')
          .then(r => 'SUCCESS')
          .catch(e => 'FAILED: ' + e.toString())
    `
  });
  testRunner.log('Fetch result (after clearing CSP, same document): ' +
                 result2.result.result.value);

  // Case 3: Navigate to a new document (which clears the window's CSP cache)
  // and re-create the world without CSP. Since the global manager had its CSP
  // cleared in Case 2, the fetch in this new document should now succeed.
  testRunner.log('Navigating and re-creating without CSP...');
  await page.navigate('/inspector-protocol/resources/test-page.html');

  // Re-create again without CSP
  const isolatedWorldResponse3 = await dp.Page.createIsolatedWorld({
    frameId: mainFrameId,
    worldName: 'Test world',
    grantUniveralAccess: false
  });
  const contextId3 = isolatedWorldResponse3.result.executionContextId;

  const result3 = await dp.Runtime.evaluate({
    contextId: contextId3,
    returnByValue: true,
    awaitPromise: true,
    expression: `
        fetch('/inspector-protocol/resources/test-page.html')
          .then(r => 'SUCCESS')
          .catch(e => 'FAILED: ' + e.toString())
    `
  });
  testRunner.log('Fetch result (after clearing CSP, new document): ' +
                 result3.result.result.value);

  testRunner.completeTest();
})
