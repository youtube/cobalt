// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.browser;

import android.content.Context;
import android.widget.FrameLayout;
import org.chromium.components.embedder_support.view.ContentViewRenderView;

/**
 * Extends {@link ContentViewRenderView} to provide a custom rendering surface for Cobalt.
 * This class primarily overrides the {@code createSurfaceBridge()} method to return a
 * {@link CobaltSurfaceBridge} instance, enabling Cobalt-specific surface management.
 *
 * <p>It inherits the rendering capabilities and lifecycle management from
 * {@link ContentViewRenderView}, which is a core component in Chromium for
 * displaying web content in an Android {@link android.view.View}.</p>
 */
public class CobaltViewRenderView extends ContentViewRenderView {

    public CobaltViewRenderView(Context context) {
        super(context);
    }

    public SurfaceBridge createSurfaceBridge() {
        return new CobaltSurfaceBridge();
    }

    /**
     * An inner class that extends {@link ContentViewRenderView.SurfaceBridge} to provide
     * Cobalt-specific initialization and management of the rendering surface.
     *
     * <p>This bridge is responsible for creating and setting up the {@link android.view.SurfaceView}
     * that Chromium's rendering engine will draw into. It ensures the surface is correctly
     * added to the view hierarchy and initially hidden.</p>
     */
    public static class CobaltSurfaceBridge extends SurfaceBridge {
        @Override
        public void initialize(ContentViewRenderView renderView) {
            mSurfaceView = renderView.createSurfaceView(renderView.getContext());
            mSurfaceView.setZOrderMediaOverlay(true);

            renderView.addView(mSurfaceView,
                    new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                            FrameLayout.LayoutParams.MATCH_PARENT));
            mSurfaceView.setVisibility(GONE);
        }
    }
}
