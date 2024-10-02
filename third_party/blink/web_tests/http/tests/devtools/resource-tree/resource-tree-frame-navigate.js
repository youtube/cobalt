// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests resource tree model on iframe navigation, compares resource tree against golden. Every line is important.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.showPanel('resources');
  await TestRunner.loadHTML(`
    <iframe id="iframe"></iframe>
    <script>
      function navigateIframe(url) {
        var iframe = document.getElementById("iframe");
        iframe.setAttribute("src", url);
        return new Promise(x => iframe.onload = x);
      }
    </script>
  `);
  await TestRunner.addStylesheetTag('resources/styles-initial.css');

  TestRunner.addResult('Before navigation');
  TestRunner.addResult('====================================');
  var url = TestRunner.url("resources/resource-tree-frame-navigate-iframe-before.html");
  await TestRunner.evaluateInPageAsync(`navigateIframe("${url}")`);
  ApplicationTestRunner.dumpResourceTreeEverything();

  TestRunner.addResult('');
  TestRunner.addResult('After navigation');
  TestRunner.addResult('====================================');
  var url = TestRunner.url("resources/resource-tree-frame-navigate-iframe-after.html");
  await TestRunner.evaluateInPageAsync(`navigateIframe("${url}")`);
  ApplicationTestRunner.dumpResourceTreeEverything();
  TestRunner.completeTest();
})();
