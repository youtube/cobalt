// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  TestRunner.addResult(`Tests that ignore-listed sourcemaps properly detach on reload crbug.com/888688`);
  var content =
    `console.log(1);
//# sourceMappingURL=data:application/json;base64,eyJ2ZXJzaW9uIjozLCJmaWxlIjoiZXZhbC1pbi5qcyIsInNvdXJjZVJvb3QiOiIiLCJzb3VyY2VzIjpbImV2YWwtaW4iXSwibmFtZXMiOltdLCJtYXBwaW5ncyI6IkFBQUEsT0FBTyxDQUFDLEdBQUcsQ0FBQyxDQUFDLENBQUMsQ0FBQyIsInNvdXJjZXNDb250ZW50IjpbImNvbnNvbGUubG9nKDEpOyJdfQ==`;

  TestRunner.addSniffer(Bindings.IgnoreListManager.prototype, 'patternChangeFinishedForTests', step1);
  var frameworkRegexString = '.*';
  Common.settingForTest('skipStackFramesPattern').set('.*');

  async function step1() {
    TestRunner.addResult('Evaluating script with source map');
    await TestRunner.evaluateInPageAnonymously(content);
    await new Promise(resolve => TestRunner.addSniffer(Bindings.CompilerScriptMapping.prototype, "sourceMapAttachedForTest", resolve));
    await ConsoleTestRunner.waitForConsoleMessagesPromise(1);

    await TestRunner.reloadPagePromise();
    TestRunner.addResult(`After reload, Console message count: ${SDK.ConsoleModel.allMessagesUnordered().length}`);
    TestRunner.completeTest();
  }
})();

