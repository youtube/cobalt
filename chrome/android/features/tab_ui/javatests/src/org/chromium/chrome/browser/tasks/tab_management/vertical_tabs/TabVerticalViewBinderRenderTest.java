// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController.UiTabState;
import org.chromium.chrome.browser.actor.ui.TabIndicatorStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.MediaState;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;

import java.io.IOException;
import java.util.List;

// TODO(crbug.com/524393627): Add tests for Incognito.
// TODO(crbug.com/521987032): Add tests for nested children with visual spine and actor indicator.
// TODO(crbug.com/519325873): Add RenderTest for pinned tab hover state.
// TODO(crbug.com/509226293): Add tests for RTL layout.

/** Render tests for Vertical Tabs UI (TabVerticalViewBinder). */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabVerticalViewBinderRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(2)
                    .build();

    private Activity mActivity;
    private FrameLayout mRenderView;

    public TabVerticalViewBinderRenderTest(boolean isNightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(isNightModeEnabled);
        mRenderTestRule.setNightModeEnabled(isNightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
    }

    private ViewGroup inflateAndAttachView(int layoutResId) {
        FrameLayout parent = new FrameLayout(mActivity);
        mActivity.setContentView(
                parent,
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mRenderView = new FrameLayout(mActivity);

        Context themeContext = new ContextThemeWrapper(mActivity, R.style.Theme_BrowserUI_DayNight);

        mRenderView.setBackgroundColor(
                SemanticColorUtils.getColorSurfaceContainerHighest(themeContext));

        int padding = ViewUtils.dpToPx(mActivity, 8);
        mRenderView.setPadding(padding, padding, padding, padding);

        ViewGroup view =
                (ViewGroup) LayoutInflater.from(mActivity).inflate(layoutResId, mRenderView, false);
        mRenderView.addView(view);
        int width = ViewGroup.LayoutParams.WRAP_CONTENT;

        parent.addView(
                mRenderView,
                new FrameLayout.LayoutParams(width, ViewGroup.LayoutParams.WRAP_CONTENT));

        return view;
    }

    private TabFaviconFetcher createFaviconFetcher() {
        return new TabFaviconFetcher() {
            @Override
            public void fetch(Callback<TabFavicon> callback) {
                Drawable drawable =
                        AppCompatResources.getDrawable(mActivity, R.drawable.ic_globe_24dp);
                callback.onResult(
                        new TabFavicon(drawable, drawable, false) {
                            @Override
                            public boolean equals(Object other) {
                                return false;
                            }
                        });
            }
        };
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Unselected() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Standard Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_unselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Active() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Active Tab");
                    model.set(TabProperties.IS_SELECTED, true);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_active");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Loading() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Loading Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(TabProperties.IS_LOADING, true);
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_loading");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_ActorIndicator_Dynamic() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "AI Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false);
                    model.set(TabProperties.ACTOR_UI_STATE, uiTabState);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_actor_indicator_dynamic");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_ActorIndicator_Static() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "AI Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.STATIC, false);
                    model.set(TabProperties.ACTOR_UI_STATE, uiTabState);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_actor_indicator_static");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Active_ActorIndicator_Dynamic() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Active AI Tab");
                    model.set(TabProperties.IS_SELECTED, true);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.DYNAMIC, false);
                    model.set(TabProperties.ACTOR_UI_STATE, uiTabState);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_active_actor_indicator_dynamic");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Active_ActorIndicator_Static() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Active AI Tab");
                    model.set(TabProperties.IS_SELECTED, true);
                    UiTabState uiTabState =
                            new UiTabState(0, null, null, TabIndicatorStatus.STATIC, false);
                    model.set(TabProperties.ACTOR_UI_STATE, uiTabState);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_active_actor_indicator_static");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_MediaIndicator() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Media Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(TabProperties.MEDIA_INDICATOR, MediaState.AUDIBLE);
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "standard_tab_media_indicator");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStandardTab_Hovered() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTab);
                    model.set(TabProperties.TITLE, "Hovered Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(
                            TabProperties.TAB_ACTION_BUTTON_DATA,
                            new TabActionButtonData(
                                    TabActionButtonType.CLOSE, /* tabActionListener= */ null));
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    android.view.MotionEvent event =
                            android.view.MotionEvent.obtain(
                                    0,
                                    0,
                                    android.view.MotionEvent.ACTION_HOVER_ENTER,
                                    0.0f,
                                    0.0f,
                                    0);
                    view[0].dispatchGenericMotionEvent(event);
                });

        mRenderTestRule.render(mRenderView, "standard_tab_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_Unselected() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_pinned_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindPinnedTab);
                    model.set(TabProperties.TITLE, "Pinned Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "pinned_tab_unselected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_Active() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_pinned_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindPinnedTab);
                    model.set(TabProperties.TITLE, "Pinned Tab");
                    model.set(TabProperties.IS_SELECTED, true);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "pinned_tab_active");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPinnedTab_Loading() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_pinned_item);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindPinnedTab);
                    model.set(TabProperties.TITLE, "Loading Pinned Tab");
                    model.set(TabProperties.IS_SELECTED, false);
                    model.set(TabProperties.FAVICON_FETCHER, createFaviconFetcher());
                    model.set(TabProperties.IS_LOADING, true);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "pinned_tab_loading");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Collapsed() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_group_header);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTabGroupHeader);
                    model.set(TabProperties.TITLE, "Collapsed Group");
                    model.set(TabProperties.TAB_GROUP_CARD_COLOR, TabGroupColorId.BLUE);
                    model.set(TabProperties.IS_COLLAPSED, true);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "tab_group_header_collapsed");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Expanded() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_group_header);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTabGroupHeader);
                    model.set(TabProperties.TITLE, "Expanded Group");
                    model.set(TabProperties.TAB_GROUP_CARD_COLOR, TabGroupColorId.BLUE);
                    model.set(TabProperties.IS_COLLAPSED, false);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        mRenderTestRule.render(mRenderView, "tab_group_header_expanded");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Collapsed_Hovered() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_group_header);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTabGroupHeader);
                    model.set(TabProperties.TITLE, "Hovered Group (Collapsed)");
                    model.set(TabProperties.TAB_GROUP_CARD_COLOR, TabGroupColorId.BLUE);
                    model.set(TabProperties.IS_COLLAPSED, true);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    android.view.MotionEvent event =
                            android.view.MotionEvent.obtain(
                                    0,
                                    0,
                                    android.view.MotionEvent.ACTION_HOVER_ENTER,
                                    0.0f,
                                    0.0f,
                                    0);
                    view[0].dispatchGenericMotionEvent(event);
                });

        mRenderTestRule.render(mRenderView, "tab_group_header_collapsed_hovered");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTabGroupHeader_Expanded_Hovered() throws IOException {
        ViewGroup[] view = new ViewGroup[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view[0] = inflateAndAttachView(R.layout.vertical_tab_group_header);
                    PropertyModel model =
                            new PropertyModel.Builder(TabProperties.ALL_KEYS_VERTICAL_TAB)
                                    .with(TabProperties.IS_INCOGNITO, false)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, view[0], TabVerticalViewBinder::bindTabGroupHeader);
                    model.set(TabProperties.TITLE, "Hovered Group (Expanded)");
                    model.set(TabProperties.TAB_GROUP_CARD_COLOR, TabGroupColorId.BLUE);
                    model.set(TabProperties.IS_COLLAPSED, false);
                });
        CriteriaHelper.pollUiThread(() -> view[0].getHeight() > 0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    android.view.MotionEvent event =
                            android.view.MotionEvent.obtain(
                                    0,
                                    0,
                                    android.view.MotionEvent.ACTION_HOVER_ENTER,
                                    0.0f,
                                    0.0f,
                                    0);
                    view[0].dispatchGenericMotionEvent(event);
                });

        mRenderTestRule.render(mRenderView, "tab_group_header_expanded_hovered");
    }
}
