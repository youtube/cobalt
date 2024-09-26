// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that color-related mix-cased CSS properties are actually color aware.\n`);
  await TestRunner.loadLegacyModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.showPanel('elements');

  var colorAwareProperties = ['bAckground-ColoR', 'COloR', 'Border-coLoR', 'border-right-color', 'BOX-SHADOW'];
  for (var i = 0; i < colorAwareProperties.length; ++i) {
    var isColorAware = SDK.cssMetadata().isColorAwareProperty(colorAwareProperties[i]);
    TestRunner.addResult(colorAwareProperties[i] + (isColorAware ? ' is' : ' is NOT') + ' color aware');
  }
  TestRunner.completeTest();
})();
