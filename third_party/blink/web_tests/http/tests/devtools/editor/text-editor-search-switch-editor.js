// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that switching editor tabs after searching does not affect editor selection and viewport.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.addScriptTag('../sources/debugger/resources/edit-me.js');
  await TestRunner.addScriptTag('resources/search-me.js');

  var textEditor;
  var searchString = 'FINDME';
  var searchableView = UI.panels.sources.searchableView();
  var sourceFrame;
  SourcesTestRunner.showScriptSource('search-me.js', didShowScriptSource);

  function didShowScriptSource(shownSourceFrame) {
    sourceFrame = shownSourceFrame;
    textEditor = sourceFrame.textEditor;
    // We are probably still updating the editor in current callstack, so postpone the test execution.
    queueMicrotask(() => {
      textEditorUpdated();
    });
  }

  function textEditorUpdated(sourceFrame) {
    searchableView.showSearchField();

    TestRunner.addResult('Performing search...');
    searchableView.searchInputElement.value = searchString;
    searchableView.performSearch(true, true);
    TestRunner.addResult('Recording editor viewport after searching...');

    var originalViewport = {from: textEditor.firstVisibleLine(), to: textEditor.lastVisibleLine()};
    var originalSelectionRange = textEditor.selection();

    SourcesTestRunner.showScriptSource('edit-me.js', didShowAnotherSource);

    function didShowAnotherSource(anotherSourceFrame) {
      SourcesTestRunner.showScriptSource('search-me.js', didShowScriptSourceAgain);
    }

    function didShowScriptSourceAgain(sourceFrame) {
      TestRunner.addResult('Recording editor viewport after switching tabs...');
      var newViewport = {from: textEditor.firstVisibleLine(), to: textEditor.lastVisibleLine()};
      var newSelectionRange = textEditor.selection();
      TestRunner.addResult('Comparing viewports...');
      if (originalViewport.from === newViewport.from && originalViewport.to === newViewport.to)
        TestRunner.addResult('  viewports match, SUCCESS');
      else
        TestRunner.addResult('  viewports do not match, FAIL');
      TestRunner.addResult('Comparing selection ranges...');
      TestRunner.addResult('  original selection range: ' + originalSelectionRange.toString());
      TestRunner.addResult('  current selection range: ' + newSelectionRange.toString());
      TestRunner.completeTest();
    }
  }
})();
