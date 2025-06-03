// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import androidx.fragment.app.testing.FragmentScenario;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridgeJni;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;

/** JUnit tests of the class {@link PreloadFragment} */
@RunWith(BaseRobolectricTestRunner.class)
public class PreloadFragmentTest {
    // TODO(crbug.com/1357003): Use Espresso for view interactions.
    @Rule public JniMocker mMocker = new JniMocker();
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock private PreloadPagesSettingsBridge.Natives mNativeMock;
    @Mock private OneshotSupplierImpl<BottomSheetController> mBottomSheetControllerSupplier;

    private FragmentScenario mScenario;
    private RadioButtonWithDescriptionAndAuxButton mStandardPreloadingButton;
    private RadioButtonWithDescription mDisabledPreloadingButton;
    private final UserActionTester mActionTester = new UserActionTester();

    @Before
    public void setUp() {
        mMocker.mock(PreloadPagesSettingsBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
        mActionTester.tearDown();
    }

    private void initFragmentWithPreloadState(@PreloadPagesState int state) {
        when(mNativeMock.getState()).thenReturn(state);
        mScenario =
                FragmentScenario.launchInContainer(
                        PreloadFragment.class, Bundle.EMPTY, R.style.Theme_MaterialComponents);
        mScenario.onFragment(
                fragment -> {
                    mStandardPreloadingButton =
                            fragment.getView().findViewById(R.id.standard_option);
                    mDisabledPreloadingButton =
                            fragment.getView().findViewById(R.id.disabled_option);
                });
    }

    @Test
    public void testInitWhenPreloadStandard() {
        initFragmentWithPreloadState(PreloadPagesState.STANDARD_PRELOADING);
        assertTrue(mStandardPreloadingButton.isChecked());
        assertFalse(mDisabledPreloadingButton.isChecked());
    }

    @Test
    public void testInitWhenPreloadDisabled() {
        initFragmentWithPreloadState(PreloadPagesState.NO_PRELOADING);
        assertFalse(mStandardPreloadingButton.isChecked());
        assertTrue(mDisabledPreloadingButton.isChecked());
    }

    @Test(expected = AssertionError.class)
    public void testInitWhenPreloadOff() {
        initFragmentWithPreloadState(PreloadPagesState.EXTENDED_PRELOADING);
    }

    @Test
    public void testSelectStandard() {
        initFragmentWithPreloadState(PreloadPagesState.NO_PRELOADING);
        mStandardPreloadingButton.performClick();
        verify(mNativeMock).setState(PreloadPagesState.STANDARD_PRELOADING);
    }

    @Test
    public void testSelectDisabled() {
        initFragmentWithPreloadState(PreloadPagesState.STANDARD_PRELOADING);
        mDisabledPreloadingButton.performClick();
        verify(mNativeMock).setState(PreloadPagesState.NO_PRELOADING);
    }
}
