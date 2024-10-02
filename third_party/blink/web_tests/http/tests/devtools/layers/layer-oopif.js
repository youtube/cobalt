// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests renderer does not crash with an out-of-process iframe`);
  await TestRunner.loadTestModule('layers_test_runner');
  await TestRunner.loadHTML(`
      <style>
      #frame {
        width: 200px;
        height: 200px;
      }
      </style>
    `);
  await TestRunner.addIframe('http://localhost:8000/devtools/layers/resources/composited-iframe.html', {id: 'frame'});
  await LayersTestRunner.requestLayers();
  TestRunner.completeTest();
})();
