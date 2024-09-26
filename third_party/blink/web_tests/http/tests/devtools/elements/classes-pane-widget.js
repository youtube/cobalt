// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that classes pane widget shows correct suggestions.\n`);

  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  await TestRunner.loadHTML(`
    <style>
      .b { width: 1px; }
      .abc { width: 1px; }
      .a1 { width: 1px; }
      .a2 { width: 1px; }
    </style>
    <div id="myDiv"></div>
  `);

  var classesPane = new Elements.ClassesPaneWidget();
  ElementsTestRunner.selectNodeWithId('myDiv', onNodeSelected);

  async function onNodeSelected() {
    await testCompletion('.');
    await testCompletion('a');
    TestRunner.addResult('\nAdding class "abc"');
    await testCompletion('a');
    TestRunner.completeTest();
  }

  /**
   * @param {string} prefix
   */
  async function testCompletion(prefix) {
    TestRunner.addResult('\nCompletion for prefix: ' + prefix);
    var completions =
        await classesPane.prompt.buildClassNameCompletions('', prefix);
    for (var completion of completions)
      TestRunner.addResult(completion.text);
  }
})();
