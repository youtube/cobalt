// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import org.chromium.base.Callback;

/** Test for CallbackBindCheck. */
public class CallbackBindCheckTest3 {
    public void postRunnable(Runnable r) {}

    public void testTrigger(Callback<String> callback, String value) {
        // Trigger 3: Passed as Runnable method parameter.
        postRunnable(() -> callback.onResult(value));
    }
}
