// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package org.chromium.content.browser.androidoverlay;

import android.content.Context;
import android.os.IBinder;

import org.chromium.gfx.mojom.Rect;
import org.chromium.media.mojom.AndroidOverlayConfig;

/**
 * Common interface for AndroidOverlay core implementations.
 */
public interface OverlayCore {
    /**
     * Initializes the overlay core.
     *
     * @param context Context.
     * @param config Initial overlay configuration.
     * @param host Host interface to send callbacks to.
     * @param asPanel True if the overlay should be a panel (used for testing).
     */
    void initialize(Context context, AndroidOverlayConfig config, DialogOverlayCore.Host host, boolean asPanel);

    /**
     * Called when the overlay is being released.
     */
    void release();

    /**
     * Layout the overlay with new bounds.
     *
     * @param rect New bounds.
     */
    void layoutSurface(Rect rect);

    /**
     * Receives a new window token.
     *
     * @param token New token, or null if lost.
     */
    void onWindowToken(IBinder token);
}