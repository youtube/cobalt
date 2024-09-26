// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.HEIGHT;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.SHOW_KEYBOARD_CALLBACK;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TOP_SHADOW_VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.AsyncViewProvider;
import org.chromium.ui.AsyncViewStub;
import org.chromium.ui.ViewProvider;
import org.chromium.ui.modelutil.LazyConstructionPropertyMcp;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.ViewUtils;

import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;

/**
 * View tests for the keyboard accessory sheet component.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AccessorySheetViewTest {
    private PropertyModel mModel;
    private BlockingQueue<AccessorySheetView> mViewPager;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AsyncViewStub viewStub = mActivityTestRule.getActivity().findViewById(
                    R.id.keyboard_accessory_sheet_stub);
            int height = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                    R.dimen.keyboard_accessory_sheet_height);
            mModel = AccessorySheetProperties.defaultPropertyModel().with(HEIGHT, height).build();
            ViewProvider<AccessorySheetView> provider =
                    AsyncViewProvider.of(viewStub, R.id.keyboard_accessory_sheet_container);
            mViewPager = new ArrayBlockingQueue<>(1);
            LazyConstructionPropertyMcp.create(
                    mModel, VISIBLE, provider, AccessorySheetViewBinder::bind);
            provider.whenLoaded(mViewPager::add);
        });
    }

    @Test
    @MediumTest
    public void testAccessoryVisibilityChangedByModel() throws InterruptedException {
        // Initially, there shouldn't be a view yet.
        assertNull(mViewPager.poll());

        // After setting the visibility to true, the view should exist and be visible.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, true); });
        AccessorySheetView viewPager = mViewPager.take();
        assertEquals(viewPager.getVisibility(), View.VISIBLE);

        // After hiding the view, the view should still exist but be invisible.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mModel.set(VISIBLE, false); });
        assertNotEquals(viewPager.getVisibility(), View.VISIBLE);
    }

    @Test
    @MediumTest
    public void testAddingTabToModelRendersTabsView() throws InterruptedException {
        final String kSampleAction = "Some Action";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(TABS).add(new Tab("Passwords", null, null, R.layout.empty_accessory_sheet,
                    AccessoryTabType.PASSWORDS, new Tab.Listener() {
                        @Override
                        public void onTabCreated(ViewGroup view) {
                            assertNotNull("The tab must have been created!", view);
                            assertTrue("Empty tab is a layout.", view instanceof LinearLayout);
                            LinearLayout baseLayout = (LinearLayout) view;
                            TextView sampleTextView = new TextView(mActivityTestRule.getActivity());
                            sampleTextView.setText(kSampleAction);
                            baseLayout.addView(sampleTextView);
                        }

                        @Override
                        public void onTabShown() {}
                    }));
            mModel.set(ACTIVE_TAB_INDEX, 0);
        });
        // Shouldn't cause the view to be inflated.
        assertNull(mViewPager.poll());

        // Setting visibility should cause the Tab to be rendered.
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        assertNotNull(mViewPager.take());

        onView(withText(kSampleAction)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSettingActiveTabIndexChangesTab() {
        final String kFirstTab = "First Tab";
        final String kSecondTab = "Second Tab";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(TABS).add(createTestTabWithTextView(kFirstTab));
            mModel.get(TABS).add(createTestTabWithTextView(kSecondTab));
            mModel.set(ACTIVE_TAB_INDEX, 0);
            mModel.set(VISIBLE, true);
        }); // Render view.

        onViewWaiting(withText(kFirstTab)).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(ACTIVE_TAB_INDEX, 1));

        onViewWaiting(withText(kSecondTab));
    }

    @Test
    @MediumTest
    public void testRemovingTabDeletesItsView() {
        final String kFirstTab = "First Tab";
        final String kSecondTab = "Second Tab";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(TABS).add(createTestTabWithTextView(kFirstTab));
            mModel.get(TABS).add(createTestTabWithTextView(kSecondTab));
            mModel.set(ACTIVE_TAB_INDEX, 0);
            mModel.set(VISIBLE, true);
        }); // Render view.

        onViewWaiting(withText(kFirstTab)).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mModel.get(TABS).remove(mModel.get(TABS).get(0)));

        onView(withText(kFirstTab)).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testReplaceLastTab() {
        final String kFirstTab = "First Tab";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(TABS).add(createTestTabWithTextView(kFirstTab));
            mModel.set(ACTIVE_TAB_INDEX, 0);
            mModel.set(VISIBLE, true);
        }); // Render view.

        // Remove the last tab.
        onViewWaiting(withText(kFirstTab)).check(matches(isDisplayed()));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mModel.get(TABS).remove(mModel.get(TABS).get(0)); });
        onView(withText(kFirstTab)).check(doesNotExist());

        // Add a new first tab.
        final String kSecondTab = "Second Tab";
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(TABS).add(createTestTabWithTextView(kSecondTab));
            mModel.set(ACTIVE_TAB_INDEX, 0);
        });
        onViewWaiting(withText(kSecondTab));
    }

    @Test
    @MediumTest
    public void testTopShadowVisiblitySetByModel() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(TABS).add(createTestTabWithTextView("SomeTab"));
            mModel.set(TOP_SHADOW_VISIBLE, false);
            mModel.set(VISIBLE, true);
        }); // Render view.
        onView(isRoot()).check(
                waitForView(withId(R.id.accessory_sheet_shadow), ViewUtils.VIEW_INVISIBLE));

        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(TOP_SHADOW_VISIBLE, true));
        onView(withId(R.id.accessory_sheet_shadow)).check(matches(isDisplayed()));

        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(TOP_SHADOW_VISIBLE, false));
        onView(isRoot()).check(
                waitForView(withId(R.id.accessory_sheet_shadow), ViewUtils.VIEW_INVISIBLE));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
    public void testHeader() {
        Runnable runnable = mock(Runnable.class);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.get(TABS).add(createTestTabWithTextView("Header"));
            mModel.set(ACTIVE_TAB_INDEX, 0);
            mModel.set(SHOW_KEYBOARD_CALLBACK, runnable);
            mModel.set(VISIBLE, true);
        });

        onViewWaiting(withId(R.id.show_keyboard)).perform(click());

        verify(runnable, times(1)).run();

        onView(withId(R.id.sheet_title)).check(matches(withText("Passwords")));
        onViewWaiting(withId(R.id.sheet_header_shadow));
    }

    private Tab createTestTabWithTextView(String textViewCaption) {
        return new Tab("Passwords", null, null, R.layout.empty_accessory_sheet,
                AccessoryTabType.PASSWORDS, new Tab.Listener() {
                    @Override
                    public void onTabCreated(ViewGroup view) {
                        TextView sampleTextView = new TextView(mActivityTestRule.getActivity());
                        sampleTextView.setText(textViewCaption);
                        view.addView(sampleTextView);
                    }

                    @Override
                    public void onTabShown() {}
                });
    }
}
