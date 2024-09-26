// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayerJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementFieldTrial;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.LocalizationUtils;

/** Tests for {@link StripLayoutHelperManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.TAB_STRIP_REDESIGN})
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class StripLayoutHelperManagerTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private TabStripSceneLayer.Natives mTabStripSceneMock;
    @Mock
    private LayoutManagerHost mManagerHost;
    @Mock
    private LayoutUpdateHost mUpdateHost;
    @Mock
    private LayoutRenderHost mRenderHost;
    @Mock
    private Supplier<LayerTitleCache> mLayerTitleCacheSupplier;
    @Mock
    private ActivityLifecycleDispatcher mLifecycleDispatcher;

    private StripLayoutHelperManager mStripLayoutHelperManager;
    private Context mContext;
    private static final float SCREEN_WIDTH = 800.f;
    private static final float SCREEN_HEIGHT = 1600.f;
    private static final float VISIBLE_VIEWPORT_Y = 200.f;
    private static final int ORIENTATION = 2;
    private static final float BUTTON_END_PADDING_FOLIO = 10.f;
    private static final float BUTTON_END_PADDING_DETACHED = 9.f;

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(TabStripSceneLayerJni.TEST_HOOKS, mTabStripSceneMock);
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        TabStripSceneLayer.setTestFlag(true);
        initializeTest();
    }

    @After
    public void tearDown() {
        TabStripSceneLayer.setTestFlag(false);
        LocalizationUtils.setRtlForTesting(false);
    }

    private void initializeTest() {
        mStripLayoutHelperManager = new StripLayoutHelperManager(mContext, mManagerHost,
                mUpdateHost, mRenderHost, mLayerTitleCacheSupplier, mLifecycleDispatcher);
    }

    private void initializeFolioTest() {
        // Since we check TSR arm and determine model selector button width inside constructor, so
        // need to set TSR arm before initialize test.
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        mStripLayoutHelperManager = new StripLayoutHelperManager(mContext, mManagerHost,
                mUpdateHost, mRenderHost, mLayerTitleCacheSupplier, mLifecycleDispatcher);
    }

    private void initializeDetachedTest() {
        // Since we check TSR arm and determine model selector button width inside constructor, so
        // need to set TSR arm before initialize test.
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        mStripLayoutHelperManager = new StripLayoutHelperManager(mContext, mManagerHost,
                mUpdateHost, mRenderHost, mLayerTitleCacheSupplier, mLifecycleDispatcher);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetBackgroundColorDetached() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_DETACHED.setForTesting(true);
        mStripLayoutHelperManager.onContextChanged(mContext);
        assertEquals(ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_0),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testGetBackgroundColorFolio() {
        TabManagementFieldTrial.TAB_STRIP_REDESIGN_ENABLE_FOLIO.setForTesting(true);
        mStripLayoutHelperManager.onContextChanged(mContext);
        assertEquals(ChromeColors.getSurfaceColor(mContext, R.dimen.default_elevation_2),
                mStripLayoutHelperManager.getBackgroundColor());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonPosition_Folio() {
        // setup
        initializeFolioTest();

        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button position.
        // stripWidth(800) - buttonEndPadding(10) - MsbWidth(36) = 754
        assertEquals("Model selector button position is not as expected", 754.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(), 0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonPosition_RTL_Folio() {
        // setup
        initializeFolioTest();

        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button position.
        assertEquals("Model selector button position is not as expected", BUTTON_END_PADDING_FOLIO,
                mStripLayoutHelperManager.getModelSelectorButton().getX(), 0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonPosition_Detached() {
        // setup
        initializeDetachedTest();

        // Set model selector button position.
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button position.
        // stripWidth(800) - buttonEndPadding(9) - MsbWidth(38) = 753
        assertEquals("Model selector button position is not as expected", 753.f,
                mStripLayoutHelperManager.getModelSelectorButton().getX(), 0.0);
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testModelSelectorButtonPosition_RTL_Detached() {
        // setup
        initializeDetachedTest();

        // Set model selector button position.
        LocalizationUtils.setRtlForTesting(true);
        mStripLayoutHelperManager.onSizeChanged(
                SCREEN_WIDTH, SCREEN_HEIGHT, VISIBLE_VIEWPORT_Y, ORIENTATION);

        // Verify model selector button position.
        assertEquals("Model selector button position is not as expected",
                BUTTON_END_PADDING_DETACHED,
                mStripLayoutHelperManager.getModelSelectorButton().getX(), 0.0);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Left() {
        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected", R.drawable.tab_strip_fade_short,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Right() {
        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected", R.drawable.tab_strip_fade_short,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.TAB_STRIP_REDESIGN)
    public void testFadeDrawable_Right_ModelSelectorButtonVisible() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected", R.drawable.tab_strip_fade_long,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Right_ModelSelectorButtonVisible_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_long_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Right_TSR() {
        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_medium_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Left_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short_tsr,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Left_RTL_ModelSelectorButtonVisible_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_long_tsr,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Left_RTL_TSR() {
        // setup
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_medium_tsr,
                mStripLayoutHelperManager.getLeftFadeDrawable());
    }

    @Test
    @Feature("Tab Strip Redesign")
    public void testFadeDrawable_Right_RTL_TSR() {
        // setup
        mStripLayoutHelperManager.setModelSelectorButtonVisibleForTesting(true);
        LocalizationUtils.setRtlForTesting(true);

        // Verify fade drawable.
        assertEquals("Fade drawable resource is not as expected",
                R.drawable.tab_strip_fade_short_tsr,
                mStripLayoutHelperManager.getRightFadeDrawable());
    }
}
