// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static android.view.ViewGroup.LayoutParams.WRAP_CONTENT;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for the tab switcher long-press menu popup on the Start Surface.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
public class StartSurfaceTabSwitcherActionMenuRenderTest extends BlankUiTestActivityTestCase {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_START)
                    .build();

    private View mView;

    public StartSurfaceTabSwitcherActionMenuRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Override
    public void tearDownTest() throws Exception {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
        super.tearDownTest();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest", "StartSurface"})
    public void testRender_StartSurfaceTabSwitcherActionMenu() throws IOException {
        IncognitoUtils.setEnabledForTesting(true);
        showMenu();
        mRenderTestRule.render(mView, "start_surface_tab_switcher_action_menu");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest", "StartSurface"})
    public void testRender_StartSurfaceTabSwitcherActionMenu_IncognitoDisabled()
            throws IOException {
        IncognitoUtils.setEnabledForTesting(false);
        showMenu();
        mRenderTestRule.render(mView, "start_surface_tab_switcher_action_menu_incognito_disabled");
    }

    private void showMenu() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            StartSurfaceTabSwitcherActionMenuCoordinator coordinator =
                    new StartSurfaceTabSwitcherActionMenuCoordinator();

            coordinator.displayMenu(activity, new ListMenuButton(activity, null),
                    coordinator.buildMenuItems(), null);

            mView = coordinator.getContentView();
            if (mView.getParent() != null) {
                ((ViewGroup) mView.getParent()).removeView(mView);
            }

            int popupWidth =
                    activity.getResources().getDimensionPixelSize(R.dimen.tab_switcher_menu_width);
            mView.setBackground(
                    AppCompatResources.getDrawable(activity, R.drawable.menu_bg_tinted));
            activity.setContentView(mView, new LayoutParams(popupWidth, WRAP_CONTENT));
        });
    }
}
