// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabDataObserver;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * Tests for {@link Tab}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.TAB_STATE_V1_OPTIMIZATIONS)
public class TabUnitTest {
    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    private WindowAndroid mWindowAndroid;
    @Mock
    private LoadUrlParams mLoadUrlParams;
    @Mock
    private EmptyTabObserver mObserver;
    @Mock
    private Context mContext;
    @Mock
    private WeakReference<Context> mWeakReferenceContext;
    @Mock
    private WeakReference<Activity> mWeakReferenceActivity;
    @Mock
    private Activity mActivity;
    @Mock
    private CriticalPersistedTabData mCriticalPersistedTabData;
    @Mock
    private CriticalPersistedTabDataObserver mCriticalPersistedTabDataObserver;
    @Mock
    private NativePage mNativePage;
    @Mock
    private TabDelegateFactory mDelegateFactory;
    @Mock
    private TabWebContentsDelegateAndroid mTabWebContentsDelegateAndroid;
    @Mock
    private WebContents mWebContents;
    @Mock
    private View mNativePageView;
    @Mock
    private ChromeActivity mChromeActivity;
    @Mock
    TabImpl.Natives mNativeMock;

    private TabImpl mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doReturn(mWeakReferenceActivity).when(mWindowAndroid).getActivity();
        doReturn(mWeakReferenceContext).when(mWindowAndroid).getContext();
        doReturn(mActivity).when(mWeakReferenceActivity).get();
        doReturn(mContext).when(mWeakReferenceContext).get();
        doReturn(mContext).when(mContext).getApplicationContext();

        mTab = new TabImpl(TAB1_ID, false, null, null) {
            @Override
            public boolean isInitialized() {
                return true;
            }
        };
        mTab.addObserver(mObserver);
        CriticalPersistedTabData.from(mTab).addObserver(mCriticalPersistedTabDataObserver);
    }

    @Test
    @SmallTest
    public void testSetRootIdWithChange() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertThat(CriticalPersistedTabData.from(mTab).getRootId(), equalTo(TAB1_ID));

        CriticalPersistedTabData.from(mTab).setRootId(TAB2_ID);

        verify(mCriticalPersistedTabDataObserver).onRootIdChanged(mTab, TAB2_ID);

        assertThat(CriticalPersistedTabData.from(mTab).getRootId(), equalTo(TAB2_ID));
        assertThat(TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.DIRTY));
    }

    @Test
    @SmallTest
    public void testSetRootIdWithoutChange() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertThat(TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
        assertThat(CriticalPersistedTabData.from(mTab).getRootId(), equalTo(TAB1_ID));
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        CriticalPersistedTabData.from(mTab).setRootId(TAB1_ID);

        verify(mCriticalPersistedTabDataObserver, never())
                .onRootIdChanged(any(Tab.class), anyInt());
        assertThat(CriticalPersistedTabData.from(mTab).getRootId(), equalTo(TAB1_ID));
        assertThat(TabStateAttributes.from(mTab).getDirtinessState(),
                equalTo(TabStateAttributes.DirtinessState.CLEAN));
    }

    @Test
    @SmallTest
    public void testFreezeDetachedNativePage() {
        mocker.mock(TabImplJni.TEST_HOOKS, mNativeMock);

        doReturn(mTabWebContentsDelegateAndroid)
                .when(mDelegateFactory)
                .createWebContentsDelegate(any(Tab.class));
        doReturn(mNativePage)
                .when(mDelegateFactory)
                .createNativePage(any(String.class), any(), any(Tab.class));
        doReturn(false).when(mNativePage).isFrozen();
        doReturn(mNativePageView).when(mNativePage).getView();
        doReturn(mWindowAndroid).when(mWebContents).getTopLevelNativeWindow();
        doReturn(mChromeActivity).when(mWeakReferenceContext).get();

        mTab = new TabImpl(TAB1_ID, false, null, null) {
            @Override
            void updateWindowAndroid(WindowAndroid windowAndroid) {}
            @Override
            public WebContents getWebContents() {
                return mWebContents;
            }
            @Override
            public boolean isNativePage() {
                return true;
            }
            @Override
            void pushNativePageStateToNavigationEntry() {}
        };
        mTab.updateAttachment(mWindowAndroid, mDelegateFactory);

        // A valid, non-null NativeFrozenPage object should be instantiated when a Tab is
        // told to freeze its native page in a currently detached state.
        assertEquals(mTab.getNativePage(), mNativePage);
        mTab.freezeNativePage();
        assertNotEquals(mTab.getNativePage(), mNativePage);
    }
}
