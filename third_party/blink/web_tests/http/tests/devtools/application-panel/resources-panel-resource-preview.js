// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Application Panel preview for resources of different types.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadLegacyModule('source_frame');
  await TestRunner.showPanel('resources');
  await TestRunner.loadHTML(`
      <img src="../resources/image.png">
    `);
  await TestRunner.evaluateInPagePromise(`
      function parse(val) {
          // This is here for the JSON file imported via the script tag below
      }
    `);
  await TestRunner.addScriptTag('../resources/json-value.js');

  function dump(node, prefix) {
    for (var child of node.children()) {
      TestRunner.addResult(prefix + child.listItemElement.textContent + (child.selected ? ' (selected)' : ''));
      dump(child, prefix + '  ');
    }
  }

  function dumpCurrentState(label) {
    var types = new Map([
      [SourceFrame.ResourceSourceFrame, 'source'], [SourceFrame.ImageView, 'image'], [SourceFrame.JSONView, 'json']
    ]);

    var view = UI.panels.resources;
    TestRunner.addResult(label);
    dump(view.sidebar.sidebarTree.rootElement(), '');
    var visibleView = view.visibleView;
    if (visibleView instanceof UI.SearchableView)
      visibleView = visibleView.children()[0];
    var typeLabel = 'unknown';
    for (var type of types) {
      if (!(visibleView instanceof type[0]))
        continue;
      typeLabel = type[1];
      break;
    }
    console.log('visible view: ' + typeLabel);
  }

  async function revealResourceWithDisplayName(name) {
    var target = SDK.targetManager.primaryPageTarget();
    var model = target.model(SDK.ResourceTreeModel);
    var resource = null;
    for (var r of model.mainFrame.resources()) {
      if (r.displayName !== name)
        continue;
      resource = r;
      break;
    }

    if (!r) {
      TestRunner.addResult(name + ' was not found');
      return;
    }
    await Common.Revealer.reveal(r);
    dumpCurrentState('Revealed ' + name + ':');
  }

  await UI.viewManager.showView('resources');
  dumpCurrentState('Initial state:');
  await revealResourceWithDisplayName('json-value.js');
  await revealResourceWithDisplayName('image.png');
  await revealResourceWithDisplayName('resources-panel-resource-preview.js');

  TestRunner.completeTest();
})();
