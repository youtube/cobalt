// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests type checks in DevToolsHost.showContextMenuAtPoint\n`);
  InspectorFrontendHost.showContextMenuAtPoint(1.1, 2.2, [0x41414141]);
  TestRunner.completeTest();
})();

