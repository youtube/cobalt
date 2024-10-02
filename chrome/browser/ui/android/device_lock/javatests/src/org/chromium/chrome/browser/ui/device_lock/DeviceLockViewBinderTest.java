// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ALL_KEYS;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.DEVICE_SUPPORTS_PIN_CREATION_INTENT;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.IN_SIGN_IN_FLOW;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_CREATE_DEVICE_LOCK_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_DISMISS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_GO_TO_OS_SETTINGS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.ON_USER_UNDERSTANDS_CLICKED;
import static org.chromium.chrome.browser.ui.device_lock.DeviceLockProperties.PREEXISTING_DEVICE_LOCK;

import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for {@link DeviceLockViewBinder}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DeviceLockViewBinderTest extends BlankUiTestActivityTestCase {
    private AtomicBoolean mCreateDeviceLockButtonClicked = new AtomicBoolean();
    private AtomicBoolean mGoToOSSettingsButtonClicked = new AtomicBoolean();
    private AtomicBoolean mUserUnderstandsButtonClicked = new AtomicBoolean();
    private AtomicBoolean mDismissButtonClicked = new AtomicBoolean();

    private DeviceLockView mView;
    private PropertyModel mViewModel;
    private PropertyModelChangeProcessor mModelChangeProcessor;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        ViewGroup view = new LinearLayout(getActivity());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            getActivity().setContentView(view);

            mView = DeviceLockView.create(getActivity().getLayoutInflater());
            view.addView(mView);

            mViewModel = new PropertyModel.Builder(ALL_KEYS)
                                 .with(PREEXISTING_DEVICE_LOCK, false)
                                 .with(DEVICE_SUPPORTS_PIN_CREATION_INTENT, true)
                                 .with(IN_SIGN_IN_FLOW, false)
                                 .with(ON_CREATE_DEVICE_LOCK_CLICKED,
                                         v -> mCreateDeviceLockButtonClicked.set(true))
                                 .with(ON_GO_TO_OS_SETTINGS_CLICKED,
                                         v -> mGoToOSSettingsButtonClicked.set(true))
                                 .with(ON_USER_UNDERSTANDS_CLICKED,
                                         v -> mUserUnderstandsButtonClicked.set(true))
                                 .with(ON_DISMISS_CLICKED, v -> mDismissButtonClicked.set(true))
                                 .build();

            mModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mViewModel, mView, DeviceLockViewBinder::bind);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mModelChangeProcessor::destroy);
        super.tearDownTest();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_preExistingLock_showsUserUnderstandsButton() {
        mViewModel.set(PREEXISTING_DEVICE_LOCK, true);

        verifyPageText();
        assertEquals("The continue button should show 'user understands' text when there"
                        + "is a pre-existing device lock.",
                mView.getContinueButton().getText(),
                getActivity().getResources().getString(
                        R.string.device_lock_user_understands_button));
        assertEquals("The continue button should always be visible.",
                mView.getContinueButton().getVisibility(), View.VISIBLE);
        assertEquals(
                "The dismiss button should be invisible when there is a pre-existing device lock.",
                mView.getDismissButton().getVisibility(), View.INVISIBLE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_supportsPINCreationIntent_showsDeviceLockCreationButton() {
        mViewModel.set(PREEXISTING_DEVICE_LOCK, false);
        mViewModel.set(DEVICE_SUPPORTS_PIN_CREATION_INTENT, true);

        verifyPageText();
        assertEquals("The continue button should show device lock creation text when "
                        + "device supports the PIN creation intent.",
                mView.getContinueButton().getText(),
                getActivity().getResources().getString(R.string.device_lock_create_lock_button));
        assertEquals("The continue button should always be visible.",
                mView.getContinueButton().getVisibility(), View.VISIBLE);
        assertEquals(
                "The dismiss button should be visible when there is no pre-existing device lock.",
                mView.getDismissButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_doesNotSupportPINCreationIntent_showsGoToOSSettingsButton() {
        mViewModel.set(PREEXISTING_DEVICE_LOCK, false);
        mViewModel.set(DEVICE_SUPPORTS_PIN_CREATION_INTENT, false);

        verifyPageText();
        assertEquals("The continue button should show 'go to OS settings' text when "
                        + "device does not support the PIN creation intent.",
                mView.getContinueButton().getText(),
                getActivity().getResources().getString(R.string.go_to_os_settings));
        assertEquals("The continue button should always be visible.",
                mView.getContinueButton().getVisibility(), View.VISIBLE);
        assertEquals(
                "The dismiss button should be visible when there is no pre-existing device lock.",
                mView.getDismissButton().getVisibility(), View.VISIBLE);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_inSignInFlow_dismissButtonHasDismissedSignInText() {
        mViewModel.set(IN_SIGN_IN_FLOW, true);

        verifyPageText();
        assertEquals("The dismiss button should show fre dismissal text when in the sign in flow.",
                mView.getDismissButton().getText(),
                getActivity().getResources().getString(R.string.signin_fre_dismiss_button));
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_notInSignInFlow_dismissButtonHasNoThanksText() {
        mViewModel.set(IN_SIGN_IN_FLOW, false);

        verifyPageText();
        assertEquals(
                "The dismiss button should show 'no thanks' text when not in the sign in flow.",
                mView.getDismissButton().getText(),
                getActivity().getResources().getString(R.string.no_thanks));
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_createDeviceLockButtonClicked_triggersOnClick() {
        mViewModel.set(PREEXISTING_DEVICE_LOCK, false);
        mViewModel.set(DEVICE_SUPPORTS_PIN_CREATION_INTENT, true);
        mCreateDeviceLockButtonClicked.set(false);

        mView.getContinueButton().performClick();
        assertTrue(
                "A click on the continue button should trigger the device lock creation handler.",
                mCreateDeviceLockButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_goToOSSettingsButtonClicked_triggersOnClick() {
        mViewModel.set(PREEXISTING_DEVICE_LOCK, false);
        mViewModel.set(DEVICE_SUPPORTS_PIN_CREATION_INTENT, false);
        mGoToOSSettingsButtonClicked.set(false);

        mView.getContinueButton().performClick();
        assertTrue("A click on the continue button should trigger the 'go to OS settings' handler.",
                mGoToOSSettingsButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_userUnderstandsButtonClicked_triggersOnClick() {
        mViewModel.set(PREEXISTING_DEVICE_LOCK, true);
        mUserUnderstandsButtonClicked.set(false);

        mView.getContinueButton().performClick();
        assertTrue("A click on the continue button should trigger the 'user understands' handler.",
                mUserUnderstandsButtonClicked.get());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testDeviceLockView_dismissButtonClicked_triggersOnClick() {
        mDismissButtonClicked.set(false);

        mView.getDismissButton().performClick();
        assertTrue("A click on the dismiss button should trigger the dismissal handler.",
                mDismissButtonClicked.get());
    }

    private void verifyPageText() {
        assertEquals("The title text should not change.", mView.getTitle().getText(),
                getActivity().getResources().getString(R.string.device_lock_title));
        assertEquals("The description text should not change.", mView.getDescription().getText(),
                getActivity().getResources().getString(R.string.device_lock_description));
        assertEquals("The notice text should not change.", mView.getHighlightedNotice().getText(),
                getActivity().getResources().getString(R.string.device_lock_highlighted_notice));
    }
}
