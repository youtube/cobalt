// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that exporting console messages produces proper output.\n`);

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function log() {
        var nonUrl = "foo";
        var url = "www.chromium.org";
        var longNonUrl = "baaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaar";
        var longUrl = "www.loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooongurl.com";
        console.log(nonUrl);
        console.log(url);
        console.log(longNonUrl);
        console.log(longUrl);
        console.log(url + " " + longUrl + " " + nonUrl + " " + longNonUrl + " " + url + " " + longUrl);
        console.trace("My important trace");
        console.error("My error");
    }
    log();
  `);

  TestRunner.addResult('\nDumping messages');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.addResult('\nDumping export strings');
  var consoleView = Console.ConsoleView.instance();
  consoleView.visibleViewMessages.forEach(message => TestRunner.addResult(message.toExportString()));
  TestRunner.completeTest();
})();
