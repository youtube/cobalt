// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import org.chromium.base.Callback;

/** Test for CallbackBindCheck. */
public class CallbackBindCheckTest6 {
    public void testTrigger(Callback<Object> callback, String value) {
        // Trigger 6: Using composite local expression.
        Runnable r6 = () -> callback.onResult(value + "suffix");
    }
}
