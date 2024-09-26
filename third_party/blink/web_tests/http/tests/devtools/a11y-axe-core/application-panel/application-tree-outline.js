// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadTestModule('axe_core_test_runner');

  TestRunner.addResult('Tests accessibility of Tree outline sidepane in Application Panel.');

  await TestRunner.showPanel('resources');
  const applicationSidebar = UI.panels.resources.panelSidebarElement();
  await AxeCoreTestRunner.runValidation(applicationSidebar);
  TestRunner.completeTest();
})();
