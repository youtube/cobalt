(async function(testRunner) {
  const {dp, session} = await testRunner.startBlank(
      `Tests that clearing data works for DOMStorage with storageKey\n`);

  await dp.DOMStorage.enable();
  await dp.Page.enable();

  const addedPromise = dp.DOMStorage.onceDomStorageItemAdded();
  session.evaluate('window.localStorage.setItem("testKey", "testItem")');
  const event = await addedPromise;
  const storageId = event.params.storageId;
  const storageKey = storageId.storageKey;

  testRunner.log(`Set item: ${await session.evaluate('window.localStorage.getItem("testKey")')}`)
  testRunner.log(`storageId.storageKey: ${typeof storageKey}`)
  testRunner.log(storageKey ? "not empty\n" : "empty\n");

  await dp.Storage.clearDataForStorageKey({storageKey: storageKey, storageTypes: 'local_storage'});

  testRunner.log("Clear data");
  testRunner.log(`Get item: ${await session.evaluate('window.localStorage.getItem("testKey")')}`);

  testRunner.completeTest();
})
