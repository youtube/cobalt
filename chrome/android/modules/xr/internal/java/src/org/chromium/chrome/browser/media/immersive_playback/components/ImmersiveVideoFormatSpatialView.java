// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;

/**
 * Wrapper holding the ImmersiveVideoFormatRadioGroup (root view) and its corresponding XR panel
 * entity holder.
 */
@NullMarked
public class ImmersiveVideoFormatSpatialView {
    public final ImmersiveVideoFormatView androidView;
    public final XrPanelEntityHolder<?> spatialEntityHolder;

    /**
     * Creates a new {@link ImmersiveVideoFormatSpatialView}.
     *
     * @param androidView The {@link ImmersiveVideoFormatView}.
     * @param spatialEntityHolder The {@link XrPanelEntityHolder}.
     */
    public ImmersiveVideoFormatSpatialView(
            ImmersiveVideoFormatView androidView, XrPanelEntityHolder<?> spatialEntityHolder) {
        this.androidView = androidView;
        this.spatialEntityHolder = spatialEntityHolder;
    }
}
