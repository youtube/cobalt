// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Build;
import android.os.OutcomeReceiver;
import android.view.View.OnLayoutChangeListener;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.ActivityState;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.WebContents;

import java.util.HashMap;

/**
 * Unit tests for {@link FullscreenHtmlApiHandlerCompat}. TODO(crbug.com/40525786): Can be
 * parametrized with a parametrized Robolectric test runner.
 */
@Features.EnableFeatures({ChromeFeatureList.DISPLAY_EDGE_TO_EDGE_FULLSCREEN})
@RunWith(BaseRobolectricTestRunner.class)
public class FullscreenHtmlApiHandlerCompatUnitTest {
    private static final int DEVICE_WIDTH = 900;
    private static final int DEVICE_HEIGHT = 1600;
    private static final int SYSTEM_UI_HEIGHT = 100;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private Activity mActivity;
    @Mock private TabBrowserControlsConstraintsHelper mTabBrowserControlsConstraintsHelper;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private ContentView mContentView;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private TabModelSelector mTabModelSelector;
    private MultiWindowModeStateDispatcherImpl mMultiWindowModeStateDispatcher;
    private FullscreenHtmlApiHandlerCompat mFullscreenHtmlApiHandlerCompat;
    private ObservableSupplierImpl<Boolean> mAreControlsHidden;
    private UserDataHost mHost;

    @SuppressLint("NewApi")
    private static final Insets NAVIGATION_BAR_INSETS = Insets.of(0, 0, 0, 100);

    private static final Insets STATUS_BAR_INSETS = Insets.of(0, 100, 0, 0);
    private static final Insets SYSTEM_BAR_INSETS = Insets.of(0, 100, 0, 100);

    private static final WindowInsetsCompat VISIBLE_SYSTEM_BARS_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), NAVIGATION_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_BAR_INSETS)
                    .build();

    @Implements(Activity.class)
    public static class FullscreenShadowActivity extends ShadowActivity {
        public HashMap<Integer, Integer> counters = new HashMap<>();

        public FullscreenShadowActivity() {}

        @Implementation(minSdk = 34)
        protected void requestFullscreenMode(
                int request, OutcomeReceiver<Void, Throwable> approvalCallback) {
            if (approvalCallback != null) {
                approvalCallback.onResult(null);
            }
            if (counters.containsKey(request)) {
                counters.put(request, counters.get(request) + 1);
            } else {
                counters.put(request, 1);
            }
        }
    }

    private void assertEqualNumberOfEnterAndExitActivityFullscreenMode(int numberOfEnters) {
        FullscreenShadowActivity shadow = (FullscreenShadowActivity) Shadows.shadowOf(mActivity);
        if (numberOfEnters == 0) {
            assertFalse(shadow.counters.containsKey(Activity.FULLSCREEN_MODE_REQUEST_ENTER));
            assertFalse(shadow.counters.containsKey(Activity.FULLSCREEN_MODE_REQUEST_EXIT));
        } else {
            assertTrue(shadow.counters.containsKey(Activity.FULLSCREEN_MODE_REQUEST_ENTER));
            assertEquals(
                    numberOfEnters,
                    (int) shadow.counters.get(Activity.FULLSCREEN_MODE_REQUEST_ENTER));
            assertTrue(shadow.counters.containsKey(Activity.FULLSCREEN_MODE_REQUEST_EXIT));
            assertEquals(
                    numberOfEnters,
                    (int) shadow.counters.get(Activity.FULLSCREEN_MODE_REQUEST_EXIT));
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mHost = new UserDataHost();
        doReturn(mHost).when(mTab).getUserDataHost();

        mAreControlsHidden = new ObservableSupplierImpl<Boolean>();
        mMultiWindowModeStateDispatcher = new MultiWindowModeStateDispatcherImpl(mActivity);
        mFullscreenHtmlApiHandlerCompat =
                new FullscreenHtmlApiHandlerCompat(
                        mActivity, mAreControlsHidden, false, mMultiWindowModeStateDispatcher) {
                    // This needs a PopupController, which isn't available in the test since we
                    // can't mock statics in this version of mockito.  Even if we could mock it, it
                    // casts to WebContentsImpl and other things that we can't reference due to
                    // restrictions in DEPS.
                    @Override
                    public void destroySelectActionMode(Tab tab) {}

                    @Override
                    protected void updateMultiTouchZoomSupport(boolean enable) {}
                };
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenRequestCanceledAtPendingStateBeforeControlsDisappear() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        // Fullscreen process stops at pending state since controls are not hidden.
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false));

        TabBrowserControlsConstraintsHelper.setForTesting(
                mTab, mTabBrowserControlsConstraintsHelper);

        // Exit is invoked unexpectedly before the controls get hidden. Fullscreen process should be
        // marked as canceled.
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        assertTrue(
                "Fullscreen request should have been canceled",
                mFullscreenHtmlApiHandlerCompat.getPendingFullscreenOptionsForTesting().canceled());

        // Controls are hidden afterwards.
        mAreControlsHidden.set(true);

        // The fullscreen request was canceled. Verify the controls are restored.
        verify(mTabBrowserControlsConstraintsHelper).update(BrowserControlsState.SHOWN, true);
        assertEquals(null, mFullscreenHtmlApiHandlerCompat.getPendingFullscreenOptionsForTesting());

        // Verify that fullscreen mode was exited properly
        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenRequestCanceledAtPendingStateAfterControlsDisappear() {
        // Avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false));

        mAreControlsHidden.set(true);
        TabBrowserControlsConstraintsHelper.setForTesting(
                mTab, mTabBrowserControlsConstraintsHelper);

        // Exit is invoked unexpectedly _after_ the controls get hidden.
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();

        // Verify the browser controls are restored.
        verify(mTabBrowserControlsConstraintsHelper).update(BrowserControlsState.SHOWN, true);
        assertEquals(null, mFullscreenHtmlApiHandlerCompat.getPendingFullscreenOptionsForTesting());

        // Verify that fullscreen mode was exited properly
        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenAddAndRemoveObserver() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        // Fullscreen process stops at pending state since controls are not hidden.
        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer).onEnterFullscreen(mTab, fullscreenOptions);
        Assert.assertEquals(
                "Observer is not added.",
                1,
                mFullscreenHtmlApiHandlerCompat.getObserversForTesting().size());

        // Exit is invoked unexpectedly before the controls get hidden. Fullscreen process should be
        // marked as canceled.
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.destroy();
        Assert.assertEquals(
                "Observer is not removed.",
                0,
                mFullscreenHtmlApiHandlerCompat.getObserversForTesting().size());
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverCalledOncePerSession() {
        // avoid calling GestureListenerManager/SelectionPopupController
        doReturn(null).when(mTab).getWebContents();
        doReturn(true).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(2)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(2)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(3)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(3)).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(3);
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverCalledOncePerSessionWhenWebContentsNotNull() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        doReturn(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets())
                .when(mContentView)
                .getRootWindowInsets();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(2)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(2)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(3)).onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(3)).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(3);
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testNoObserverWhenCanceledBeforeBeingInteractable() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable();

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        // Before the tab becomes interactable, fullscreen exit gets requested.
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);

        verify(observer, never()).onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, never()).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(0);
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverInTabNonInteractableState() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mTab).isUserInteractable(); // Tab not interactable at first.

        mAreControlsHidden.set(false);
        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        mFullscreenHtmlApiHandlerCompat.initialize(mActivityTabProvider, mTabModelSelector);
        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, never()).onEnterFullscreen(mTab, fullscreenOptions);

        // Only after the tab turns interactable does the fullscreen mode is entered.
        mFullscreenHtmlApiHandlerCompat.onTabInteractable(mTab);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        mFullscreenHtmlApiHandlerCompat.destroy();

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    public void testToastIsShownInFullscreenButNotPictureInPicture() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets())
                .when(mContentView)
                .getRootWindowInsets();
        // The window must have focus and already have the controls hidden, else fullscreen will be
        // deferred.  The toast would be deferred with it.
        doReturn(true).when(mContentView).hasWindowFocus();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);

        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);

        // Catch the layout listener, which is an implementation detail but what can one do?  Note
        // that we make the layout appear to have gotten bigger, which is important since the
        // fullscreen handler checks for it.
        ArgumentCaptor<OnLayoutChangeListener> arg =
                ArgumentCaptor.forClass(OnLayoutChangeListener.class);
        verify(mContentView).addOnLayoutChangeListener(arg.capture());
        arg.getValue().onLayoutChange(mContentView, 0, 0, 100, 100, 0, 0, 10, 10);

        // We should now be in fullscreen, with the toast shown.
        assertTrue(
                "Fullscreen toast should be visible in fullscreen",
                mFullscreenHtmlApiHandlerCompat.isToastVisibleForTesting());

        // Losing / gaining the focus should hide / show the toast when it's applicable.  This also
        // covers picture in picture.
        mFullscreenHtmlApiHandlerCompat.onWindowFocusChanged(mActivity, false);
        assertTrue(
                "Fullscreen toast should not be visible when unfocused",
                !mFullscreenHtmlApiHandlerCompat.isToastVisibleForTesting());
        mFullscreenHtmlApiHandlerCompat.onWindowFocusChanged(mActivity, true);
        assertTrue(
                "Fullscreen toast should be visible when focused",
                mFullscreenHtmlApiHandlerCompat.isToastVisibleForTesting());

        // Toast should not be visible when we exit fullscreen.
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        assertTrue(
                "Fullscreen toast should not be visible outside of fullscreen",
                !mFullscreenHtmlApiHandlerCompat.isToastVisibleForTesting());

        // If we gain / lose the focus outside of fullscreen, then nothing interesting should happen
        // with the toast.
        mFullscreenHtmlApiHandlerCompat.onActivityStateChange(mActivity, ActivityState.PAUSED);
        assertTrue(
                "Fullscreen toast should not be visible after pause",
                !mFullscreenHtmlApiHandlerCompat.isToastVisibleForTesting());
        mFullscreenHtmlApiHandlerCompat.onActivityStateChange(mActivity, ActivityState.RESUMED);
        assertTrue(
                "Fullscreen toast should not be visible after resume when not in fullscreen",
                !mFullscreenHtmlApiHandlerCompat.isToastVisibleForTesting());
    }

    @Test
    public void testToastIsShownAtLayoutChangeWithRotation() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        doReturn(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets())
                .when(mContentView)
                .getRootWindowInsets();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);

        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);

        ArgumentCaptor<OnLayoutChangeListener> arg =
                ArgumentCaptor.forClass(OnLayoutChangeListener.class);
        verify(mContentView).addOnLayoutChangeListener(arg.capture());

        // Device rotation swaps device width/height dimension.
        arg.getValue()
                .onLayoutChange(
                        mContentView,
                        0,
                        0,
                        DEVICE_HEIGHT,
                        DEVICE_WIDTH,
                        0,
                        0,
                        /* oldRight= */ DEVICE_WIDTH,
                        /* oldBottom= */ DEVICE_HEIGHT - SYSTEM_UI_HEIGHT);

        // We should now be in fullscreen, with the toast shown.
        assertTrue(
                "Fullscreen toast should be visible in fullscreen",
                mFullscreenHtmlApiHandlerCompat.isToastVisibleForTesting());
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverNotifiedWhenActivityStopped() {
        mFullscreenHtmlApiHandlerCompat =
                new FullscreenHtmlApiHandlerCompat(
                        mActivity, mAreControlsHidden, true, mMultiWindowModeStateDispatcher) {
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        doReturn(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets())
                .when(mContentView)
                .getRootWindowInsets();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);

        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        mFullscreenHtmlApiHandlerCompat.onActivityStateChange(mActivity, ActivityState.STOPPED);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testFullscreenObserverCalledOnceWhenExitPersistentFullscreenModeCalled() {
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        doReturn(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets())
                .when(mContentView)
                .getRootWindowInsets();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);

        FullscreenManager.Observer observer = Mockito.mock(FullscreenManager.Observer.class);
        mFullscreenHtmlApiHandlerCompat.addObserver(observer);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        // Enter full screen.
        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        verify(observer, times(1)).onEnterFullscreen(mTab, fullscreenOptions);

        // Call exitPersistentFullscreenMode followed by onExitFullscreen. Observers should be
        // notified once.
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        mFullscreenHtmlApiHandlerCompat.exitPersistentFullscreenMode();
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        verify(observer, times(1)).onExitFullscreen(mTab);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }

    @Test
    public void testMultiWindowModeChangeIsHandledWhenNoTabIsActive() {
        mFullscreenHtmlApiHandlerCompat =
                new FullscreenHtmlApiHandlerCompat(
                        mActivity, mAreControlsHidden, true, mMultiWindowModeStateDispatcher) {
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(false);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(true);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(false);
    }

    @Test
    @Config(
            shadows = {FullscreenShadowActivity.class},
            sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testMultiWindowModeChangeIsHandledDuringAfterEnterFullscreen() {
        mFullscreenHtmlApiHandlerCompat =
                new FullscreenHtmlApiHandlerCompat(
                        mActivity, mAreControlsHidden, true, mMultiWindowModeStateDispatcher) {
                    @Override
                    public void destroySelectActionMode(Tab tab) {}
                };

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(mContentView).when(mTab).getContentView();
        doReturn(true).when(mTab).isUserInteractable();
        doReturn(true).when(mTab).isHidden();
        doReturn(true).when(mContentView).hasWindowFocus();
        doReturn(VISIBLE_SYSTEM_BARS_WINDOW_INSETS.toWindowInsets())
                .when(mContentView)
                .getRootWindowInsets();
        mAreControlsHidden.set(true);

        mFullscreenHtmlApiHandlerCompat.setTabForTesting(mTab);
        FullscreenOptions fullscreenOptions = new FullscreenOptions(false, false);

        mFullscreenHtmlApiHandlerCompat.onEnterFullscreen(mTab, fullscreenOptions);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(false);
        mFullscreenHtmlApiHandlerCompat.onExitFullscreen(mTab);
        mMultiWindowModeStateDispatcher.dispatchMultiWindowModeChanged(true);

        assertEqualNumberOfEnterAndExitActivityFullscreenMode(1);
    }
}
