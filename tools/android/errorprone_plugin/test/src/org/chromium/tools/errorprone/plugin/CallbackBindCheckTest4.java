// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import org.chromium.base.Callback;

/** Test for CallbackBindCheck. */
public class CallbackBindCheckTest4 {
    public void testTrigger(Callback<Object> callback) {
        // Trigger 4: Using 'this' (allowed because it is constant identity).
        Runnable r4 = () -> callback.onResult(this);
    }
}
