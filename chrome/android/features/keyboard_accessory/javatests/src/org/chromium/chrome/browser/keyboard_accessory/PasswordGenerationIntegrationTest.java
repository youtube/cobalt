// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.chromium.base.test.util.Matchers.is;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationTestHelper.acceptPasswordInGenerationBottomSheet;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationTestHelper.rejectPasswordInGenerationBottomSheet;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlockingNoException;

import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.Window;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.IntegrationTest;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.password_manager.FakePasswordStoreAndroidBackend;
import org.chromium.chrome.browser.password_manager.FakePasswordStoreAndroidBackendFactoryImpl;
import org.chromium.chrome.browser.password_manager.FakePasswordSyncControllerDelegateFactoryImpl;
import org.chromium.chrome.browser.password_manager.PasswordStoreAndroidBackendFactory;
import org.chromium.chrome.browser.password_manager.PasswordStoreBridge;
import org.chromium.chrome.browser.password_manager.PasswordStoreCredential;
import org.chromium.chrome.browser.password_manager.PasswordSyncControllerDelegateFactory;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.components.signin.AccountUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Integration tests for password generation. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(
        reason =
                "TODO(crbug.com/1346583): add resetting logic for"
                        + "FakePasswordStoreAndroidBackend to allow batching")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
@DisableIf.Build(
        message = "crbug.com/1496214",
        sdk_is_greater_than = VERSION_CODES.R,
        sdk_is_less_than = VERSION_CODES.TIRAMISU)
public class PasswordGenerationIntegrationTest {
    /**
     * The number of buttons currently available in the keyboard accessory bar. The offered options
     * are: passwords, addresses and payments.
     */
    public static final int KEYBOARD_ACCESSORY_BAR_ITEM_COUNT = 3;

    @Rule public SyncTestRule mSyncTestRule = new SyncTestRule();

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private static final String PASSWORD_NODE_ID = "password_field";
    private static final String PASSWORD_NODE_ID_MANUAL = "password_field_manual";
    private static final String SUBMIT_NODE_ID = "input_submit_button";
    private static final String SUBMIT_NODE_ID_MANUAL = "input_submit_button_manual";
    private static final String FORM_URL =
            "/chrome/test/data/password/filled_simple_signup_form.html";
    private static final String DONE_URL = "/chrome/test/data/password/done.html";
    private static final String PASSWORD_ATTRIBUTE_NAME = "password_creation_field";
    private static final String ELIGIBLE_FOR_GENERATION = "1";
    private static final String USERNAME_TEXT = "username";

    private EmbeddedTestServer mTestServer;
    private ManualFillingTestHelper mHelper;
    private PasswordStoreBridge mPasswordStoreBridge;
    private ChromeTabbedActivity mActivity;
    private RecyclerView mKeyboardAccessoryBarItems;
    private TextView mGeneratedPasswordTextView;
    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetTestSupport;

    @Before
    public void setUp() throws InterruptedException {
        PasswordStoreAndroidBackendFactory.setFactoryInstanceForTesting(
                new FakePasswordStoreAndroidBackendFactoryImpl());
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((FakePasswordStoreAndroidBackend)
                                    PasswordStoreAndroidBackendFactory.getInstance()
                                            .createBackend())
                            .setSyncingAccount(
                                    AccountUtils.createAccountFromName(
                                            SigninTestRule.TEST_ACCOUNT_EMAIL));
                });
        PasswordSyncControllerDelegateFactory.setFactoryInstanceForTesting(
                new FakePasswordSyncControllerDelegateFactoryImpl());

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        ManualFillingTestHelper.disableServerPredictions();

        runOnUiThreadBlocking(
                () -> {
                    mPasswordStoreBridge = new PasswordStoreBridge();
                    mBottomSheetController =
                            BottomSheetControllerProvider.from(
                                    mSyncTestRule.getActivity().getWindowAndroid());
                    mBottomSheetTestSupport = new BottomSheetTestSupport(mBottomSheetController);
                });

        mTestServer =
                EmbeddedTestServer.createAndStartHTTPSServer(
                        InstrumentationRegistry.getInstrumentation().getContext(),
                        ServerCertificate.CERT_OK);
        mSyncTestRule.loadUrl(mTestServer.getURL(FORM_URL));
        mHelper = new ManualFillingTestHelper(mSyncTestRule);
        mHelper.updateWebContentsDependentState();
        mActivity = mSyncTestRule.getActivity();
    }

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @IntegrationTest
    public void testAutomaticGenerationCancel() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID);
        dismissBottomSheetIfNeeded();
        // Focus again, because the sheet steals the focus from web contents.
        focusField(PASSWORD_NODE_ID);
        mHelper.waitForKeyboardAccessoryToBeShown(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ButtonCompat suggestStrongPassword =
                            (ButtonCompat) mHelper.getFirstAccessorySuggestion();
                    Assert.assertNotNull(suggestStrongPassword);
                    suggestStrongPassword.performClick();
                });
        waitForGenerationDialog();
        rejectPasswordInGenerationDialog();
        assertPasswordTextEmpty(PASSWORD_NODE_ID);
        assertNoInfobarsAreShown();
        CriteriaHelper.pollUiThread(
                () -> {
                    PasswordStoreCredential[] credentials =
                            mPasswordStoreBridge.getAllCredentials();
                    Criteria.checkThat(
                            "Should have added no passwords.", credentials.length, is(0));
                });
    }

    @Test
    @IntegrationTest
    public void testManualGenerationCancel() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID_MANUAL);
        mHelper.waitForKeyboardAccessoryToBeShown();
        toggleAccessorySheet();
        pressManualGenerationSuggestion();
        waitForGenerationDialog();
        rejectPasswordInGenerationDialog();
        assertPasswordTextEmpty(PASSWORD_NODE_ID_MANUAL);
        assertNoInfobarsAreShown();
        CriteriaHelper.pollUiThread(
                () -> {
                    PasswordStoreCredential[] credentials =
                            mPasswordStoreBridge.getAllCredentials();
                    Criteria.checkThat(
                            "Should have added no passwords.", credentials.length, is(0));
                });
    }

    @Test
    @IntegrationTest
    public void testAutomaticGenerationUsePassword() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID);
        dismissBottomSheetIfNeeded();
        // Focus again, because the sheet steals the focus from web contents.
        focusField(PASSWORD_NODE_ID);
        mHelper.waitForKeyboardAccessoryToBeShown(true);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ButtonCompat suggestStrongPassword =
                            (ButtonCompat) mHelper.getFirstAccessorySuggestion();
                    Assert.assertNotNull(suggestStrongPassword);
                    suggestStrongPassword.performClick();
                });
        waitForGenerationDialog();
        String generatedPassword = acceptPasswordInGenerationDialog();
        CriteriaHelper.pollInstrumentationThread(
                () -> !mHelper.getFieldText(PASSWORD_NODE_ID).isEmpty());
        assertPasswordText(PASSWORD_NODE_ID, generatedPassword);
        clickNode(SUBMIT_NODE_ID);
        ChromeTabUtils.waitForTabPageLoaded(
                mSyncTestRule.getActivity().getActivityTab(), mTestServer.getURL(DONE_URL));
        waitForMessageShown();
        CriteriaHelper.pollUiThread(
                () -> {
                    PasswordStoreCredential[] credentials =
                            mPasswordStoreBridge.getAllCredentials();
                    Criteria.checkThat(
                            "Should have added one password.", credentials.length, is(1));
                    Criteria.checkThat(credentials[0].getUsername(), is(USERNAME_TEXT));
                });
    }

    @Test
    @IntegrationTest
    public void testManualGenerationUsePassword() throws InterruptedException, TimeoutException {
        waitForGenerationLabel();
        focusField(PASSWORD_NODE_ID_MANUAL);
        mHelper.waitForKeyboardAccessoryToBeShown();
        toggleAccessorySheet();
        pressManualGenerationSuggestion();
        waitForGenerationDialog();
        String generatedPassword = acceptPasswordInGenerationDialog();
        CriteriaHelper.pollInstrumentationThread(
                () -> !mHelper.getFieldText(PASSWORD_NODE_ID_MANUAL).isEmpty());
        assertPasswordText(PASSWORD_NODE_ID_MANUAL, generatedPassword);
        clickNode(SUBMIT_NODE_ID_MANUAL);
        ChromeTabUtils.waitForTabPageLoaded(
                mSyncTestRule.getActivity().getActivityTab(), mTestServer.getURL(DONE_URL));
        waitForMessageShown();
        CriteriaHelper.pollUiThread(
                () -> {
                    PasswordStoreCredential[] credentials =
                            mPasswordStoreBridge.getAllCredentials();
                    Criteria.checkThat(
                            "Should have added one password.", credentials.length, is(1));
                    Criteria.checkThat(credentials[0].getUsername(), is(USERNAME_TEXT));
                });
    }

    private void pressManualGenerationSuggestion() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return mActivity.findViewById(R.id.passwords_sheet) != null;
                });
        ArrayList<View> selectedViews = new ArrayList();
        (mActivity.findViewById(R.id.passwords_sheet))
                .findViewsWithText(
                        selectedViews,
                        mActivity
                                .getResources()
                                .getString(R.string.password_generation_accessory_button),
                        View.FIND_VIEWS_WITH_TEXT);
        View generationButton = selectedViews.get(0);
        runOnUiThreadBlockingNoException(generationButton::callOnClick);
    }

    private void toggleAccessorySheet() {
        CriteriaHelper.pollUiThread(
                () -> {
                    mKeyboardAccessoryBarItems =
                            (RecyclerView) mActivity.findViewById(R.id.bar_items_view);
                    return mKeyboardAccessoryBarItems != null;
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return mKeyboardAccessoryBarItems.findViewHolderForLayoutPosition(0) != null;
                });
        KeyboardAccessoryButtonGroupView keyboardAccessoryView =
                (KeyboardAccessoryButtonGroupView)
                        mKeyboardAccessoryBarItems.findViewHolderForLayoutPosition(0).itemView;
        CriteriaHelper.pollUiThread(
                () -> {
                    return keyboardAccessoryView.getButtons().size()
                            == KEYBOARD_ACCESSORY_BAR_ITEM_COUNT;
                });
        ArrayList<ChromeImageButton> buttons = keyboardAccessoryView.getButtons();
        ChromeImageButton keyButton = buttons.get(0);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        runOnUiThreadBlocking(
                () -> {
                    keyButton.callOnClick();
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private void focusField(String node) throws TimeoutException, InterruptedException {
        DOMUtils.clickNode(mHelper.getWebContents(), node);
        // TODO(crbug.com/1440955): Remove the code below. Manually calling focus and scroll is
        // needed because this test uses a screen keyboard stub instead of the real screen keyboard.
        // Integration tests in general should use the real keyboard to reflect the production
        // behavior better.
        DOMUtils.focusNode(mHelper.getWebContents(), node);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHelper.getWebContents().scrollFocusedEditableNodeIntoView();
                });
    }

    private void clickNode(String node) throws InterruptedException, TimeoutException {
        DOMUtils.clickNodeWithJavaScript(mHelper.getWebContents(), node);
    }

    private void assertPasswordTextEmpty(String passwordNode)
            throws InterruptedException, TimeoutException {
        assertPasswordText(passwordNode, "");
    }

    private void assertPasswordText(String passwordNode, String text)
            throws InterruptedException, TimeoutException {
        Assert.assertEquals(text, mHelper.getFieldText(passwordNode));
    }

    private void waitForGenerationLabel() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    String attribute =
                            mHelper.getAttribute(PASSWORD_NODE_ID, PASSWORD_ATTRIBUTE_NAME);
                    return ELIGIBLE_FOR_GENERATION.equals(attribute);
                });
    }

    private void waitForGenerationDialog() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_GENERATION_BOTTOM_SHEET)) {
            BottomSheetTestSupport.waitForOpen(mBottomSheetController);
            return;
        }
        waitForModalDialogPresenter();
        ModalDialogManager manager =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        mSyncTestRule.getActivity()::getModalDialogManager);
        CriteriaHelper.pollUiThread(
                () -> {
                    Window window =
                            ((AppModalPresenter) manager.getCurrentPresenterForTest()).getWindow();
                    mGeneratedPasswordTextView =
                            window.getDecorView()
                                    .getRootView()
                                    .findViewById(R.id.generated_password);
                    return mGeneratedPasswordTextView != null;
                });
    }

    private void waitForModalDialogPresenter() {
        CriteriaHelper.pollUiThread(
                () ->
                        mSyncTestRule
                                        .getActivity()
                                        .getModalDialogManager()
                                        .getCurrentPresenterForTest()
                                != null);
    }

    private void assertNoInfobarsAreShown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            InfoBarContainer.from(mSyncTestRule.getActivity().getActivityTab())
                                    .hasInfoBars());
                });
    }

    private static String getTextFromTextView(int id) {
        AtomicReference<String> textRef = new AtomicReference<>();
        onView(withId(id))
                .check((view, error) -> textRef.set(((TextView) view).getText().toString()));
        return textRef.get();
    }

    private void waitForMessageShown() {
        WindowAndroid window = mSyncTestRule.getActivity().getWindowAndroid();
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Message is not enqueued.",
                            MessagesTestHelper.getMessageCount(window),
                            Matchers.is(1));
                });
    }

    private void rejectPasswordInGenerationDialog() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_GENERATION_BOTTOM_SHEET)) {
            rejectPasswordInGenerationBottomSheet();
        } else {
            onView(withId(R.id.negative_button)).perform(click());
        }
    }

    private String acceptPasswordInGenerationDialog() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_GENERATION_BOTTOM_SHEET)) {
            return acceptPasswordInGenerationBottomSheet();
        } else {
            String generatedPassword = getTextFromTextView(R.id.generated_password);
            onView(withId(R.id.positive_button)).perform(click());
            return generatedPassword;
        }
    }

    private void dismissBottomSheetIfNeeded() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_GENERATION_BOTTOM_SHEET)) {
            return;
        }
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBottomSheetTestSupport.forceClickOutsideTheSheet();
                });
    }
}
