// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.version_info;

import org.chromium.base.annotations.CalledByNative;

/**
 * Bridge between native and VersionConstants.java.
 */
public class VersionConstantsBridge {
    @CalledByNative
    public static int getChannel() {
        return VersionConstants.CHANNEL;
    }
}
