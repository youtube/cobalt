// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests for {@link TabGroupUiViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabGroupUiViewBinderTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private ImageView mShowGroupDialogButton;
    private ImageView mNewTabButton;
    private ViewGroup mContainerView;
    private View mMainContent;
    private FrameLayout mImageTilesContainer;
    ColorStateList mTint1;
    ColorStateList mTint2;

    private PropertyModel mModel;
    private PropertyModelChangeProcessor mMCP;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewGroup parentView = new FrameLayout(sActivity);
                    TabGroupUiToolbarView toolbarView =
                            (TabGroupUiToolbarView)
                                    LayoutInflater.from(sActivity)
                                            .inflate(
                                                    R.layout.dynamic_bottom_tab_strip_toolbar,
                                                    parentView,
                                                    false);
                    mShowGroupDialogButton =
                            toolbarView.findViewById(R.id.toolbar_show_group_dialog_button);
                    mNewTabButton = toolbarView.findViewById(R.id.toolbar_new_tab_button);
                    mContainerView = toolbarView.findViewById(R.id.toolbar_container_view);
                    mMainContent = toolbarView.findViewById(R.id.main_content);
                    mImageTilesContainer =
                            toolbarView.findViewById(R.id.toolbar_image_tiles_container);
                    mTint1 =
                            ContextCompat.getColorStateList(
                                    sActivity, R.color.default_text_color_link_tint_list);
                    mTint2 =
                            ContextCompat.getColorStateList(
                                    sActivity, R.color.default_icon_color_white_tint_list);
                    RecyclerView recyclerView =
                            (TabListRecyclerView)
                                    LayoutInflater.from(sActivity)
                                            .inflate(
                                                    R.layout.tab_list_recycler_view_layout,
                                                    parentView,
                                                    false);
                    recyclerView.setLayoutManager(
                            new LinearLayoutManager(
                                    sActivity, LinearLayoutManager.HORIZONTAL, false));

                    mModel =
                            new PropertyModel.Builder(TabGroupUiProperties.ALL_KEYS)
                                    .with(TabGroupUiProperties.BACKGROUND_COLOR, Color.WHITE)
                                    .build();
                    mMCP =
                            PropertyModelChangeProcessor.create(
                                    mModel,
                                    new TabGroupUiViewBinder.ViewHolder(toolbarView, recyclerView),
                                    TabGroupUiViewBinder::bind);
                });
    }

    @After
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(mMCP::destroy);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetShowGroupDialogOnClickListener() {
        AtomicBoolean clicked = new AtomicBoolean();
        clicked.set(false);
        mShowGroupDialogButton.performClick();
        assertFalse(clicked.get());

        mModel.set(
                TabGroupUiProperties.SHOW_GROUP_DIALOG_ON_CLICK_LISTENER,
                (View view) -> clicked.set(true));

        mShowGroupDialogButton.performClick();
        assertTrue(clicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetNewTabButtonOnClickListener() {
        AtomicBoolean clicked = new AtomicBoolean();
        clicked.set(false);
        mNewTabButton.performClick();
        assertFalse(clicked.get());

        mModel.set(
                TabGroupUiProperties.NEW_TAB_BUTTON_ON_CLICK_LISTENER,
                (View view) -> clicked.set(true));

        mNewTabButton.performClick();
        assertTrue(clicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetImageTilesContainerOnClickListener() {
        AtomicBoolean clicked = new AtomicBoolean();
        clicked.set(false);
        mImageTilesContainer.performClick();
        assertFalse(clicked.get());

        mModel.set(
                TabGroupUiProperties.SHOW_GROUP_DIALOG_ON_CLICK_LISTENER,
                (View view) -> clicked.set(true));

        mImageTilesContainer.performClick();
        assertTrue(clicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetMainContentVisibility() {
        View contentView = new View(sActivity);
        mContainerView.addView(contentView);
        contentView.setVisibility(View.GONE);

        mModel.set(TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE, true);
        assertEquals(View.VISIBLE, contentView.getVisibility());

        mModel.set(TabGroupUiProperties.IS_MAIN_CONTENT_VISIBLE, false);
        assertEquals(View.INVISIBLE, contentView.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTint() {
        mModel.set(TabGroupUiProperties.TINT, mTint1);
        assertEquals(mTint1, mShowGroupDialogButton.getImageTintList());
        assertEquals(mTint1, mNewTabButton.getImageTintList());

        mModel.set(TabGroupUiProperties.TINT, mTint2);
        assertEquals(mTint2, mShowGroupDialogButton.getImageTintList());
        assertEquals(mTint2, mNewTabButton.getImageTintList());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetBackgroundColor() {
        mModel.set(TabGroupUiProperties.BACKGROUND_COLOR, Color.WHITE);
        assertEquals(Color.WHITE, ((ColorDrawable) mMainContent.getBackground()).getColor());

        mModel.set(TabGroupUiProperties.BACKGROUND_COLOR, Color.BLACK);
        assertEquals(Color.BLACK, ((ColorDrawable) mMainContent.getBackground()).getColor());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testShowGroupDialogButtonVisibility() {
        mModel.set(TabGroupUiProperties.SHOW_GROUP_DIALOG_BUTTON_VISIBLE, false);
        assertEquals(View.GONE, mShowGroupDialogButton.getVisibility());

        mModel.set(TabGroupUiProperties.SHOW_GROUP_DIALOG_BUTTON_VISIBLE, true);
        assertEquals(View.VISIBLE, mShowGroupDialogButton.getVisibility());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testImageTilesContainerVisibility() {
        mModel.set(TabGroupUiProperties.IMAGE_TILES_CONTAINER_VISIBLE, true);
        assertEquals(View.VISIBLE, mImageTilesContainer.getVisibility());

        mModel.set(TabGroupUiProperties.IMAGE_TILES_CONTAINER_VISIBLE, false);
        assertEquals(View.GONE, mImageTilesContainer.getVisibility());
    }
}
