// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {dp} =
      await testRunner.startBlank('Tests --window-size switch handling.');

  const {result: {value}} =
      (await dp.Runtime.evaluate({
        expression: `window.outerWidth + 'x' + window.outerHeight`
      })).result;

  testRunner.log('Window size: ' + value);

  testRunner.completeTest();
})
