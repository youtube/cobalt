// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.graphics.RectF;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/** The public interface for the top toolbar texture component. */
public class TopToolbarOverlayCoordinator implements SceneOverlay {
    /** The view state for this overlay. */
    private final PropertyModel mModel;

    /** A handle to the 'view' for this component as the layout manager requires access to it. */
    private final TopToolbarSceneLayer mSceneLayer;

    /** Handles processing updates to the model. */
    private final CompositorModelChangeProcessor mChangeProcessor;

    /** Business logic for this overlay. */
    private final TopToolbarOverlayMediator mMediator;

    private final Context mContext;

    public TopToolbarOverlayCoordinator(
            Context context,
            LayoutManager layoutManager,
            Callback<ClipDrawableProgressBar.DrawingInfo> progressInfoCallback,
            ObservableSupplier<Tab> tabSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            Supplier<ResourceManager> resourceManagerSupplier,
            TopUiThemeColorProvider topUiThemeColorProvider,
            Supplier<Integer> bottomToolbarControlsOffsetSupplier,
            int layoutsToShowOn,
            boolean isVisibilityManuallyControlled) {
        // If BCIV is enabled, we always show the hairline on the composited
        // toolbar, and let renderer+viz control the visibility during scrolls.
        mContext = context;
        boolean showHairline = ChromeFeatureList.sBrowserControlsInViz.isEnabled();
        mModel =
                new PropertyModel.Builder(TopToolbarOverlayProperties.ALL_KEYS)
                        .with(TopToolbarOverlayProperties.RESOURCE_ID, R.id.control_container)
                        .with(
                                TopToolbarOverlayProperties.URL_BAR_RESOURCE_ID,
                                R.drawable.modern_location_bar)
                        .with(TopToolbarOverlayProperties.VISIBLE, true)
                        .with(TopToolbarOverlayProperties.X_OFFSET, 0)
                        .with(
                                TopToolbarOverlayProperties.CONTENT_OFFSET,
                                browserControlsStateProvider.getContentOffset())
                        .with(TopToolbarOverlayProperties.ANONYMIZE, false)
                        .with(TopToolbarOverlayProperties.SHOW_SHADOW, showHairline)
                        .build();
        mSceneLayer = new TopToolbarSceneLayer(resourceManagerSupplier);
        mChangeProcessor =
                layoutManager.createCompositorMCP(mModel, mSceneLayer, TopToolbarSceneLayer::bind);

        mMediator =
                new TopToolbarOverlayMediator(
                        mModel,
                        context,
                        layoutManager,
                        progressInfoCallback,
                        tabSupplier,
                        browserControlsStateProvider,
                        topUiThemeColorProvider,
                        bottomToolbarControlsOffsetSupplier,
                        layoutsToShowOn,
                        isVisibilityManuallyControlled);
    }

    /**
     * Set whether the android view corresponding with this overlay is showing.
     * @param isVisible Whether the android view is visible.
     */
    public void setIsAndroidViewVisible(boolean isVisible) {
        mMediator.setIsAndroidViewVisible(isVisible);
    }

    /** @param visible Whether the overlay and shadow should be visible despite other signals. */
    public void setManualVisibility(boolean visible) {
        mMediator.setManualVisibility(visible);
    }

    /** @param xOffset The x offset of the toolbar. */
    public void setXOffset(float xOffset) {
        mMediator.setXOffset(xOffset);
    }

    /** @param anonymize Whether the URL should be hidden when the layer is rendered. */
    public void setAnonymize(boolean anonymize) {
        mMediator.setAnonymize(anonymize);
    }

    /** Clean up this component. */
    public void destroy() {
        mChangeProcessor.destroy();
        mMediator.destroy();
        mSceneLayer.destroy();
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        return mSceneLayer;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mMediator.shouldBeAttachedToTree();
    }

    @Override
    public EventFilter getEventFilter() {
        return null;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {
        mMediator.setViewportHeight(height * mContext.getResources().getDisplayMetrics().density);
    }

    @Override
    public void getVirtualViews(List<VirtualView> views) {}

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        return false;
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean handlesTabCreating() {
        return false;
    }
}
