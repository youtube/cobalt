(async function(testRunner) {
  const {session, dp} = await testRunner.startBlank('Tests Storage.getStorageKey');

  await dp.Storage.enable();

  const response = await dp.Storage.getStorageKey();
  
  if (response.error) {
    testRunner.log(`FAIL: Error getting storage key: ${response.error.message}`);
  } else {
    const storageKey = response.result.storageKey;
    testRunner.log(`SUCCESS: Got storage key`);
    
    if (storageKey) {
        testRunner.log(`Storage key is non-empty.`);
    } else {
        testRunner.log(`FAIL: Storage key is empty.`);
    }
  }

  testRunner.completeTest();
})(testRunner);
