// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Editing inline styles should play nice with inline scripts.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');

  await TestRunner.navigatePromise('./resources/inline-style.html');
  const uiSourceCode = await TestRunner.waitForUISourceCode('inline-style.html', Workspace.projectTypes.Network);

  await uiSourceCode.requestContent(); // prefetch content to fix flakiness
  const headers = TestRunner.cssModel.headersForSourceURL(uiSourceCode.url());
  // Sort headers in the order they appear in the file to avoid flakiness.
  headers.sort((a, b) => a.startLine - b.startLine);
  const styleSheets = headers.map(header => header.id);
  const scripts = TestRunner.debuggerModel.scriptsForSourceURL(uiSourceCode.url());
  const locationPool = new Bindings.LiveLocationPool();
  let i = 0;
  const locationUpdates = new Map();
  for (const script of scripts) {
    const rawLocation = TestRunner.debuggerModel.createRawLocation(script, script.lineOffset, script.columnOffset);
    await Bindings.debuggerWorkspaceBinding.createLiveLocation(
      rawLocation, updateDelegate.bind(null, 'script' + i), locationPool);
    i++;
  }

  i = 0;
  for (const styleSheetId of styleSheets) {
    const header = TestRunner.cssModel.styleSheetHeaderForId(styleSheetId);
    const rawLocation = new SDK.CSSLocation(header, header.startLine, header.startColumn);
    await Bindings.cssWorkspaceBinding.createLiveLocation(
      rawLocation, updateDelegate.bind(null, 'style' + i), locationPool);
    i++;
  }

  await TestRunner.waitForPendingLiveLocationUpdates();
  printLocationUpdates();

  i = 0;
  for (const styleSheetId of styleSheets) {
    TestRunner.addResult('Adding rule' + i)
    await TestRunner.cssModel.addRule(styleSheetId, `.new-rule {
  --new: true;
}`, TextUtils.TextRange.createFromLocation(0, 0));
    await TestRunner.waitForPendingLiveLocationUpdates();
    printLocationUpdates();
    i++;
  }

  async function updateDelegate(name, location) {
    const uiLocation = await location.uiLocation();
    locationUpdates.set(name, `LiveLocation '${name}' was updated ${uiLocation.lineNumber}:${uiLocation.columnNumber}`);
  }

  function printLocationUpdates() {
    const keys = [...locationUpdates.keys()].sort();
    for (const key of keys) {
      TestRunner.addResult(locationUpdates.get(key));
    }
    locationUpdates.clear();
  }

  TestRunner.completeTest();
})();
