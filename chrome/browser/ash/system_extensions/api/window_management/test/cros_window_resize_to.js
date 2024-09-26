// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

importScripts('test_support.js', 'cros_window_test_utils.js');

promise_test(async () => {
  let [window] = await chromeos.windowManagement.getWindows();

  let width = window.width;
  let height = window.height;
  width -= 20;
  height -= 20;

  await resizeToAndTest(width, height);
});
