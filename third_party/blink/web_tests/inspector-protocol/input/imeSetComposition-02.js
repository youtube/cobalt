(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(`Tests that Input.imeSetComposition works and waits for JavaScript event handlers to finish.`);
  let { init } = await testRunner.loadScript('resources/composition-test-helper.js');
  init(session, 'abcd');

  function dumpError(message) {
    if (message.error)
      testRunner.log('Error: ' + message.error.message);
  }

  testRunner.log('Dispatching event');

  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'rawKeyDown',
    key: 'ArrowLeft',
    windowsVirtualKeyCode: 37,
  }));
  dumpError(await dp.Input.dispatchKeyEvent({
    type: 'keyUp',
    key: 'ArrowLeft',
    windowsVirtualKeyCode: 37,
  }));
  dumpError(await dp.Input.imeSetComposition({
      text: 'ｓ',
      selectionStart: 1,
      selectionEnd: 1,
  }));
  dumpError(await dp.Input.imeSetComposition({
    text: 'す',
    selectionStart: 1,
    selectionEnd: 1,
  }));
  dumpError(await dp.Input.insertText({
    text: 'す',
  }));


  testRunner.log(await session.evaluate('window.logs.join("\\n")'));



  testRunner.completeTest();
})
