// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests Application Panel response to a main frame navigation.\n`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('resources');

  function createIndexedDB(callback) {
    var mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    var model = TestRunner.mainTarget.model(Resources.IndexedDBModel);
    ApplicationTestRunner.createDatabase(mainFrameId, 'Database1', () => {
      var event = model.addEventListener(Resources.IndexedDBModel.Events.DatabaseAdded, () => {
        Common.EventTarget.removeEventListeners([event]);
        callback();
      });
      model.refreshDatabaseNames();
    });
  }

  function dump(node, prefix) {
    for (var child of node.children()) {
      TestRunner.addResult(prefix + child.listItemElement.textContent);
      dump(child, prefix + '  ');
    }
  }

  function dumpCurrentState(label) {
    var view = UI.panels.resources;
    TestRunner.addResult(label);
    dump(view.sidebar.sidebarTree.rootElement(), '');
    TestRunner.addResult('Visible view is a query view: ' + (view.visibleView instanceof Resources.DatabaseQueryView));
  }

  function fireFrameNavigated() {
    var rtm = TestRunner.resourceTreeModel;
    rtm.dispatchEventToListeners(SDK.ResourceTreeModel.Events.FrameNavigated, rtm.mainFrame);
  }

  await new Promise(createIndexedDB);
  await ApplicationTestRunner.createWebSQLDatabase('database-for-test');
  await UI.viewManager.showView('resources');
  UI.panels.resources.sidebar.databasesListTreeElement.firstChild().select(false, true);
  dumpCurrentState('Initial state:');
  await TestRunner.reloadPagePromise();
  dumpCurrentState('After navigation:');
  TestRunner.completeTest();
})();
