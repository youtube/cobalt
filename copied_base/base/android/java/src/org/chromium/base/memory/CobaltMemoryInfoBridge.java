// Copyright 2026 The Cobalt Authors. All Rights Reserved.

package org.chromium.base.memory;

import android.os.Build;
import android.os.Debug;
import org.chromium.base.annotations.CalledByNative;

/**
 * Android Java bridge for Cobalt to retrieve system memory metrics.
 */
public class CobaltMemoryInfoBridge {

    @CalledByNative
    public static int getGraphicsMemoryKb(Debug.MemoryInfo memoryInfo) {
        // getMemoryStat requires API level 23 (Marshmallow).
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            try {
                String graphicsStr = memoryInfo.getMemoryStat("summary.graphics");
                if (graphicsStr != null) {
                    return Integer.parseInt(graphicsStr);
                }
            } catch (NumberFormatException e) {
                // Ignore.
            }
        }
        return 0;
    }
}
