// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import org.chromium.base.Callback;

/** Test for CallbackBindCheck. */
public class CallbackBindCheckTest1 {
    public void testTrigger(Callback<Object> callback, String value) {
        // Trigger 1: Simple lambda expression with local var.
        Runnable r1 = () -> callback.onResult(value);
    }
}
