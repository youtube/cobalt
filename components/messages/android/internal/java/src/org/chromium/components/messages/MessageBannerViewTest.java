// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.app.Activity;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.swiperefreshlayout.widget.CircularProgressDrawable;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton.PopupMenuShownListener;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

import java.util.concurrent.ExecutionException;

/** Instrumentation tests for MessageBannerView. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class MessageBannerViewTest {
    private static final String PRIMARY_BUTTON_TEXT = "PrimaryButtonText";
    private static final String SECONDARY_BUTTON_MENU_TEXT = "SecondaryActionText";

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private static ViewGroup sContentView;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock Runnable mPrimaryActionCallback;
    @Mock Runnable mSecondaryActionCallback;

    MessageBannerView mMessageBannerView;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity = sActivityTestRule.getActivity();
                    sContentView = new FrameLayout(sActivity);
                    sActivity.setContentView(sContentView);
                });
    }

    @Before
    public void setupTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sContentView.removeAllViews();
                    mMessageBannerView =
                            (MessageBannerView)
                                    LayoutInflater.from(sActivity)
                                            .inflate(
                                                    R.layout.message_banner_view,
                                                    sContentView,
                                                    false);
                    sContentView.addView(mMessageBannerView);
                });
    }

    /**
     * Tests that, when SECONDARY_BUTTON_MENU_TEXT is not specified, clicking on secondary button
     * triggers ON_SECONDARY_ACTION callback invocation.
     */
    @Test
    @MediumTest
    public void testSecondaryActionDirectCallback() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                            android.R.drawable.ic_menu_add)
                                    .with(
                                            MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK,
                                            mSecondaryActionCallback)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });
        onView(withId(R.id.message_secondary_button)).perform(click());
        Mockito.verify(mSecondaryActionCallback).run();
    }

    /**
     * Tests that clicking on secondary button opens a menu with an item with
     * SECONDARY_BUTTON_MENU_TEXT. Clicking on this item triggers ON_SECONDARY_ACTION callback
     * invocation.
     */
    @Test
    @MediumTest
    public void testSecondaryActionMenu() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                            android.R.drawable.ic_menu_add)
                                    .with(
                                            MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT,
                                            SECONDARY_BUTTON_MENU_TEXT)
                                    .with(
                                            MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK,
                                            mSecondaryActionCallback)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });
        onView(withId(R.id.message_secondary_button)).perform(click());
        onView(withText(SECONDARY_BUTTON_MENU_TEXT)).perform(click());
        Mockito.verify(mSecondaryActionCallback).run();
    }

    /**
     * Tests that clicking on the secondary button opens a menu with an item with
     * SECONDARY_BUTTON_MENU_TEXT, the message auto-dismiss timer is cancelled at this time and the
     * message banner is shown as long as the menu is open. When the menu is dismissed, the message
     * auto-dismiss timer is (re)started with expected parameters.
     */
    @Test
    @MediumTest
    public void testSecondaryActionMenuInvokesPopupMenuEventHandlers() {
        PopupMenuShownListener listener = Mockito.mock(PopupMenuShownListener.class);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                            android.R.drawable.ic_menu_add)
                                    .with(
                                            MessageBannerProperties.SECONDARY_BUTTON_MENU_TEXT,
                                            SECONDARY_BUTTON_MENU_TEXT)
                                    .with(
                                            MessageBannerProperties.ON_SECONDARY_BUTTON_CLICK,
                                            mSecondaryActionCallback)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                    // Simulate the invocation of #setPopupMenuShownListener by the
                    // MessageBannerCoordinator ctor.
                    mMessageBannerView.setPopupMenuShownListener(listener);
                });

        // Click on the secondary icon to open the popup menu, verify that #onPopupMenuShown is
        // invoked.
        onView(withId(R.id.message_secondary_button)).perform(click());
        Mockito.verify(listener).onPopupMenuShown();

        // Click on the message banner view holding the popup menu to dismiss the menu, verify that
        // #onPopupMenuDismissed is invoked.
        onView(withChild(withText(SECONDARY_BUTTON_MENU_TEXT))).perform(click());
        Mockito.verify(listener).onPopupMenuDismissed();
    }

    /**
     * Tests that clicking on secondary button opens a menu determined by
     * SECONDARY_MENU_BUTTON_DELEGATE.
     */
    @Test
    @MediumTest
    public void testSecondaryActionMenuWithCustomDelegate() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    MVCListAdapter.ModelList menuItems = new MVCListAdapter.ModelList();
                    menuItems.add(
                            new MVCListAdapter.ListItem(
                                    BasicListMenu.ListMenuItemType.MENU_ITEM,
                                    new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                            .with(
                                                    ListMenuItemProperties.TITLE,
                                                    SECONDARY_BUTTON_MENU_TEXT)
                                            .with(ListMenuItemProperties.ENABLED, true)
                                            .build()));

                    BasicListMenu listMenu =
                            new BasicListMenu(
                                    sActivity,
                                    menuItems,
                                    (PropertyModel menuItem) -> {
                                        assert menuItem == menuItems.get(0).model;
                                        mSecondaryActionCallback.run();
                                    });

                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID,
                                            android.R.drawable.ic_menu_add)
                                    .with(
                                            MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE,
                                            new ListMenuButtonDelegate() {
                                                @Override
                                                public ListMenu getListMenu() {
                                                    return listMenu;
                                                }
                                            })
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });
        onView(withId(R.id.message_secondary_button)).perform(click());
        onView(withText(SECONDARY_BUTTON_MENU_TEXT)).perform(click());
        Mockito.verify(mSecondaryActionCallback).run();
    }

    /**
     * Setting PRIMARY_WIDGET_APPEARANCE to BUTTON_IF_TEXT_IS_SET without setting the
     * PRIMARY_BUTTON_TEXT should mean that no primary widget is visible.
     */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceButtonWithUnsetText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                                            PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });

        Assert.assertEquals(
                View.GONE,
                mMessageBannerView.findViewById(R.id.message_primary_button).getVisibility());
    }

    /**
     * Setting PRIMARY_WIDGET_APPEARANCE to BUTTON_IF_TEXT_IS_SET with PRIMARY_BUTTON_TEXT set to
     * null should mean that no primary widget is visible.
     */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceButtonWithNullText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                                            PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET)
                                    .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, null)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });

        Assert.assertEquals(
                View.GONE,
                mMessageBannerView.findViewById(R.id.message_primary_button).getVisibility());
    }

    /**
     * Setting PRIMARY_WIDGET_APPEARANCE to BUTTON_IF_TEXT_IS_SET with PRIMARY_BUTTON_TEXT set to an
     * empty string should mean that no primary widget is visible.
     */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceButtonWithEmptyText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                                            PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET)
                                    .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "")
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });

        Assert.assertEquals(
                View.GONE,
                mMessageBannerView.findViewById(R.id.message_primary_button).getVisibility());
    }

    /**
     * Setting PRIMARY_WIDGET_APPEARANCE to BUTTON_IF_TEXT_IS_SET with PRIMARY_BUTTON_TEXT set to a
     * non-empty string should show the primary action button.
     */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceButtonWithNonEmptyText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                                            PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET)
                                    .with(
                                            MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                            PRIMARY_BUTTON_TEXT)
                                    .with(
                                            MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER,
                                            new View.OnClickListener() {
                                                @Override
                                                public void onClick(View v) {
                                                    mPrimaryActionCallback.run();
                                                }
                                            })
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });

        var primaryButton = mMessageBannerView.findViewById(R.id.message_primary_button);
        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertFalse(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback).run();
    }

    /**
     * Changing PRIMARY_BUTTON_TEXT to a non-empty string when PRIMARY_WIDGET_APPEARANCE is set to
     * BUTTON_IF_TEXT_IS_SET should show the primary action button.
     */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceButtonChangeTextFromEmptyToNonEmpty() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                                            PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET)
                                    .with(MessageBannerProperties.PRIMARY_BUTTON_TEXT, "")
                                    .with(
                                            MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER,
                                            new View.OnClickListener() {
                                                @Override
                                                public void onClick(View v) {
                                                    mPrimaryActionCallback.run();
                                                }
                                            })
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                    // Change the PRIMARY_BUTTON_TEXT to a non-empty string after the view has
                    // already been put together.
                    propertyModel.set(
                            MessageBannerProperties.PRIMARY_BUTTON_TEXT, PRIMARY_BUTTON_TEXT);
                });

        var primaryButton = mMessageBannerView.findViewById(R.id.message_primary_button);
        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertFalse(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback).run();
    }

    /** Setting PRIMARY_WIDGET_APPEARANCE to PROGRESS_SPINNER should show the progress spinner. */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceProgressSpinner() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                                            PrimaryWidgetAppearance.PROGRESS_SPINNER)
                                    .with(
                                            MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER,
                                            new View.OnClickListener() {
                                                @Override
                                                public void onClick(View v) {
                                                    mPrimaryActionCallback.run();
                                                }
                                            })
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });

        var primaryButton = mMessageBannerView.findViewById(R.id.message_primary_button);
        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertTrue(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback, Mockito.never()).run();
    }

    /** Changing PRIMARY_WIDGET_APPEARANCE to PROGRESS_SPINNER should show the progress spinner. */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceChangeFromButtonToProgressSpinner()
            throws ExecutionException {
        var model =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PropertyModel propertyModel =
                                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                            .with(
                                                    MessageBannerProperties.MESSAGE_IDENTIFIER,
                                                    MessageIdentifier.TEST_MESSAGE)
                                            .with(
                                                    MessageBannerProperties
                                                            .PRIMARY_WIDGET_APPEARANCE,
                                                    PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET)
                                            .with(
                                                    MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                                    PRIMARY_BUTTON_TEXT)
                                            .with(
                                                    MessageBannerProperties
                                                            .PRIMARY_BUTTON_CLICK_LISTENER,
                                                    new View.OnClickListener() {
                                                        @Override
                                                        public void onClick(View v) {
                                                            mPrimaryActionCallback.run();
                                                        }
                                                    })
                                            .build();
                            PropertyModelChangeProcessor.create(
                                    propertyModel,
                                    mMessageBannerView,
                                    MessageBannerViewBinder::bind);
                            return propertyModel;
                        });

        var primaryButton = mMessageBannerView.findViewById(R.id.message_primary_button);
        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertFalse(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback).run();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Change the PRIMARY_WIDGET_APPEARANCE to PROGRESS_SPINNER after the view has
                    // already been put together.
                    model.set(
                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                            PrimaryWidgetAppearance.PROGRESS_SPINNER);
                });

        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertTrue(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback).run();
    }

    /** Changing PRIMARY_WIDGET_APPEARANCE to BUTTON_IF_TEXT_IS_SET should show the text. */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceChangeFromProgressSpinnerToButton()
            throws ExecutionException {
        var model =
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            PropertyModel propertyModel =
                                    new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                            .with(
                                                    MessageBannerProperties.MESSAGE_IDENTIFIER,
                                                    MessageIdentifier.TEST_MESSAGE)
                                            .with(
                                                    MessageBannerProperties
                                                            .PRIMARY_WIDGET_APPEARANCE,
                                                    PrimaryWidgetAppearance.PROGRESS_SPINNER)
                                            .with(
                                                    MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                                    PRIMARY_BUTTON_TEXT)
                                            .with(
                                                    MessageBannerProperties
                                                            .PRIMARY_BUTTON_CLICK_LISTENER,
                                                    new View.OnClickListener() {
                                                        @Override
                                                        public void onClick(View v) {
                                                            mPrimaryActionCallback.run();
                                                        }
                                                    })
                                            .build();
                            PropertyModelChangeProcessor.create(
                                    propertyModel,
                                    mMessageBannerView,
                                    MessageBannerViewBinder::bind);
                            return propertyModel;
                        });

        var primaryButton = mMessageBannerView.findViewById(R.id.message_primary_button);
        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertTrue(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback, Mockito.never()).run();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Change the PRIMARY_WIDGET_APPEARANCE to PROGRESS_SPINNER after the view has
                    // already been put together.
                    model.set(
                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                            PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET);
                });

        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertFalse(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback).run();
    }

    /**
     * Setting the PRIMARY_BUTTON_TEXT to a non-empty string should not override the
     * PROGRESS_SPINNER appearance, so the progress spinner should be shown.
     */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceProgressSpinnerWithNonEmptyButtonText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE,
                                            PrimaryWidgetAppearance.PROGRESS_SPINNER)
                                    .with(
                                            MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                            PRIMARY_BUTTON_TEXT)
                                    .with(
                                            MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER,
                                            new View.OnClickListener() {
                                                @Override
                                                public void onClick(View v) {
                                                    mPrimaryActionCallback.run();
                                                }
                                            })
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });

        var primaryButton = mMessageBannerView.findViewById(R.id.message_primary_button);
        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertTrue(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback, Mockito.never()).run();
    }

    /**
     * With neither PRIMARY_WIDGET_APPEARANCE nor PRIMARY_BUTTON_TEXT set, no primary widget should
     * be visible, since PRIMARY_WIDGET_APPEARANCE should default to BUTTON_IF_TEXT_IS_SET.
     */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceUnsetWithUnsetText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });

        Assert.assertEquals(
                View.GONE,
                mMessageBannerView.findViewById(R.id.message_primary_button).getVisibility());
    }

    /**
     * When PRIMARY_WIDGET_APPEARANCE is left unset, it should default to BUTTON_IF_TEXT_IS_SET, so
     * setting PRIMARY_BUTTON_TEXT to a non-empty string should show the primary action button.
     */
    @Test
    @MediumTest
    public void testPrimaryWidgetAppearanceUnsetWithNonEmptyText() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel propertyModel =
                            new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(
                                            MessageBannerProperties.PRIMARY_BUTTON_TEXT,
                                            PRIMARY_BUTTON_TEXT)
                                    .with(
                                            MessageBannerProperties.PRIMARY_BUTTON_CLICK_LISTENER,
                                            new View.OnClickListener() {
                                                @Override
                                                public void onClick(View v) {
                                                    mPrimaryActionCallback.run();
                                                }
                                            })
                                    .build();
                    PropertyModelChangeProcessor.create(
                            propertyModel, mMessageBannerView, MessageBannerViewBinder::bind);
                });

        var primaryButton = mMessageBannerView.findViewById(R.id.message_primary_button);
        Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
        Assert.assertFalse(primaryButton.getBackground() instanceof CircularProgressDrawable);

        onView(withId(R.id.message_primary_button)).perform(click());
        Mockito.verify(mPrimaryActionCallback).run();
    }

    /**
     * Test the content description of secondary icon/button is correctly updated based on the given
     * title and its content description.
     */
    @Test
    @MediumTest
    public void testSecondaryIconContentDescription() {
        PropertyModel model =
                ThreadUtils.runOnUiThreadBlockingNoException(
                        () -> {
                            return new PropertyModel.Builder(MessageBannerProperties.ALL_KEYS)
                                    .with(
                                            MessageBannerProperties.MESSAGE_IDENTIFIER,
                                            MessageIdentifier.TEST_MESSAGE)
                                    .with(MessageBannerProperties.TITLE, "42")
                                    .build();
                        });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModelChangeProcessor.create(
                            model, mMessageBannerView, MessageBannerViewBinder::bind);
                });
        Resources res = mMessageBannerView.getResources();
        ListMenuButton btn = mMessageBannerView.getSecondaryButtonForTesting();
        Assert.assertEquals(
                res.getString(R.string.message_more_options, "42"), btn.getContentDescription());

        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(MessageBannerProperties.TITLE, "41"));
        Assert.assertEquals(
                "Content description should be up-to-date after title is updated.",
                res.getString(R.string.message_more_options, "41"),
                btn.getContentDescription());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(MessageBannerProperties.TITLE_CONTENT_DESCRIPTION, "-42"));
        Assert.assertEquals(
                "Content description should be up-to-date if title content description is set.",
                res.getString(R.string.message_more_options, "-42"),
                btn.getContentDescription());

        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(MessageBannerProperties.TITLE, "40"));
        Assert.assertEquals(
                "Content description should be up-to-date if title content description is set.",
                res.getString(R.string.message_more_options, "-42"),
                btn.getContentDescription());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    model.set(
                            MessageBannerProperties.SECONDARY_ICON_CONTENT_DESCRIPTION,
                            "secondary icon content description");
                });
        Assert.assertEquals(
                "Content description should be up-to-date if secondary icon content description is"
                        + " set.",
                "secondary icon content description",
                btn.getContentDescription());

        TestThreadUtils.runOnUiThreadBlocking(() -> model.set(MessageBannerProperties.TITLE, "39"));
        Assert.assertEquals(
                "Content description should be up-to-date if secondary icon content description is"
                        + " set.",
                "secondary icon content description",
                btn.getContentDescription());
    }
}
