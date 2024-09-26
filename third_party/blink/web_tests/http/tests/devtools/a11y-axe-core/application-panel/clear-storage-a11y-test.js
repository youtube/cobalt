// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests accessibility in the Storage view using the axe-core linter.');
  await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.loadTestModule('axe_core_test_runner');
  await ApplicationTestRunner.resetState();
  await TestRunner.showPanel('resources');
  await UI.viewManager.showView('resources');

  const parent = UI.panels.resources.sidebar.applicationTreeElement;
  const storageElement = parent.children().find(child => child.title === 'Storage');
  storageElement.select();
  const storageView = UI.panels.resources.visibleView;
  TestRunner.addResult('Storage view is visible: ' + ApplicationTestRunner.isStorageView(storageView));

  async function writeArray() {
    const array = Array(1).fill(0);
    const mainFrameId = TestRunner.resourceTreeModel.mainFrame.id;
    await new Promise(resolve => ApplicationTestRunner.createDatabase(mainFrameId, 'Database1', resolve));
    await new Promise(
        resolve => ApplicationTestRunner.createObjectStore(mainFrameId, 'Database1', 'Store1', 'id', true, resolve));
    await new Promise(
        resolve =>
            ApplicationTestRunner.addIDBValue(mainFrameId, 'Database1', 'Store1', {key: 1, value: array}, '', resolve));
  }

  await writeArray();
  await AxeCoreTestRunner.runValidation(storageView.contentElement);
  TestRunner.completeTest();
})();
