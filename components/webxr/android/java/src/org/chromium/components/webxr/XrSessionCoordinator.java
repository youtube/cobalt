// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;
import android.content.Context;
import android.view.Surface;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides static methods called by the XrDelegateImpl as well as JNI methods to the C/C++ code
 * in order to interact with the various bits of the Java side of a session. This includes the
 * responsibility to standup/create any needed overlays/SurfaceViews and forwarding events both
 * from them and elsewhere within Chrome (forwarded/registered for via XrDelegate). This class is
 * also responsible for ensuring that there is only one active session at a time and answering
 * questions about that session; mainly via communication of its static members.
 */
@JNINamespace("webxr")
public class XrSessionCoordinator {
    private static final String TAG = "XrSessionCoordinator";
    private static final boolean DEBUG_LOGS = false;

    @IntDef({SessionType.NONE, SessionType.AR, SessionType.VR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SessionType {
        int NONE = 0;
        int AR = 1;
        int VR = 2;
    }

    private long mNativeXrSessionCoordinator;

    // The native ArCoreDevice runtime creates a XrSessionCoordinator instance in its constructor,
    // and keeps a strong reference to it for the lifetime of the device. It creates and
    // owns an XrImmersiveOverlay for the duration of an immersive session, which in
    // turn contains a reference to XrSessionCoordinator for making JNI calls back to the device.
    private XrImmersiveOverlay mImmersiveOverlay;

    // ArDelegateImpl needs to know if there's an active immersive session so that it can handle
    // back button presses from ChromeActivity's onBackPressed(). It's only set while a session is
    // in progress, and reset to null on session end. The XrImmersiveOverlay member has a strong
    // reference to the ChromeActivity, and that shouldn't be retained beyond the duration of a
    // session.
    private static XrSessionCoordinator sActiveSessionInstance;

    private @SessionType int mActiveSessionType = SessionType.NONE;

    private static WebContents sWebContents;

    /** Whether there is a non-null valid {@link #sActiveSessionInstance}. */
    private static XrSessionTypeSupplier sActiveSessionAvailableSupplier =
            new XrSessionTypeSupplier(SessionType.NONE);

    // Helper, obtains android Activity out of passed in WebContents instance.
    // Equivalent to ChromeActivity.fromWebContents(), but does not require that
    // the resulting instance is a ChromeActivity.
    public static Activity getActivity(final WebContents webContents) {
        if (webContents == null) return null;
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getActivity().get();
    }

    @CalledByNative
    private static XrSessionCoordinator create(long nativeXrSessionCoordinator) {
        ThreadUtils.assertOnUiThread();
        return new XrSessionCoordinator(nativeXrSessionCoordinator);
    }

    /**
     * Gets the current application context. Should not be called until after a start*Session call
     * has succeeded, but if called after an endSession call may return null.
     *
     * @return Context The application context.
     */
    @CalledByNative
    private static Context getApplicationContext() {
        if (sWebContents == null) {
            return null;
        }

        return getActivity(sWebContents);
    }

    private XrSessionCoordinator(long nativeXrSessionCoordinator) {
        if (DEBUG_LOGS) {
            Log.i(TAG, "constructor, nativeXrSessionCoordinator=" + nativeXrSessionCoordinator);
        }
        mNativeXrSessionCoordinator = nativeXrSessionCoordinator;
    }

    private void startSession(@SessionType int sessionType,
            XrImmersiveOverlay.Delegate overlayDelegate, final WebContents webContents) {
        assert (sActiveSessionInstance == null);
        assert (sessionType != SessionType.NONE);

        mImmersiveOverlay = new XrImmersiveOverlay();
        mImmersiveOverlay.show(overlayDelegate, webContents, this);

        sWebContents = webContents;
        sActiveSessionInstance = this;
        mActiveSessionType = sessionType;
        sActiveSessionAvailableSupplier.set(sessionType);
    }

    @CalledByNative
    private void startArSession(final ArCompositorDelegateProvider compositorDelegateProvider,
            final WebContents webContents, boolean useOverlay, boolean canRenderDomContent) {
        if (DEBUG_LOGS) Log.i(TAG, "startArSession");
        // The higher levels should have guaranteed that we're only called if there isn't any other
        // active session going on.
        assert (sActiveSessionInstance == null);

        XrImmersiveOverlay.Delegate overlayDelegate = ArClassProvider.getOverlayDelegate(
                compositorDelegateProvider.create(webContents), useOverlay, canRenderDomContent);
        startSession(SessionType.AR, overlayDelegate, webContents);
    }

    @CalledByNative
    private void startVrSession(final VrCompositorDelegateProvider compositorDelegateProvider,
            final WebContents webContents) {
        if (DEBUG_LOGS) Log.i(TAG, "startVrSession");
        // The higher levels should have guaranteed that we're only called if there isn't any other
        // active session going on.
        assert (sActiveSessionInstance == null);

        XrImmersiveOverlay.Delegate overlayDelegate = CardboardClassProvider.getOverlayDelegate(
                compositorDelegateProvider.create(webContents), getActivity(webContents));
        startSession(SessionType.VR, overlayDelegate, webContents);
    }

    @CalledByNative
    private void endSession() {
        if (DEBUG_LOGS) Log.i(TAG, "endSession");
        if (mImmersiveOverlay == null) return;
        assert (sActiveSessionInstance == this);

        mImmersiveOverlay.cleanupAndExit();
        mImmersiveOverlay = null;
        mActiveSessionType = SessionType.NONE;
        sActiveSessionInstance = null;
        sWebContents = null;
        sActiveSessionAvailableSupplier.set(SessionType.NONE);
    }

    // Called from ArDelegateImpl
    public static boolean onBackPressed() {
        if (DEBUG_LOGS) Log.i(TAG, "onBackPressed");
        // If there's an active immersive session, consume the "back" press and shut down the
        // session.
        if (sActiveSessionInstance != null) {
            sActiveSessionInstance.endSession();
            return true;
        }
        return false;
    }

    public static boolean hasActiveArSession() {
        return sActiveSessionInstance.mActiveSessionType == SessionType.AR;
    }

    public static XrSessionTypeSupplier getActiveSessionTypeSupplier() {
        return sActiveSessionAvailableSupplier;
    }

    public void onDrawingSurfaceReady(
            Surface surface, WindowAndroid rootWindow, int rotation, int width, int height) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceReady");
        if (mNativeXrSessionCoordinator == 0) return;
        XrSessionCoordinatorJni.get().onDrawingSurfaceReady(mNativeXrSessionCoordinator,
                XrSessionCoordinator.this, surface, rootWindow, rotation, width, height);
    }

    public void onDrawingSurfaceTouch(
            boolean isPrimary, boolean isTouching, int pointerId, float x, float y) {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceTouch");
        if (mNativeXrSessionCoordinator == 0) return;
        XrSessionCoordinatorJni.get().onDrawingSurfaceTouch(mNativeXrSessionCoordinator,
                XrSessionCoordinator.this, isPrimary, isTouching, pointerId, x, y);
    }

    public void onDrawingSurfaceDestroyed() {
        if (DEBUG_LOGS) Log.i(TAG, "onDrawingSurfaceDestroyed");
        if (mNativeXrSessionCoordinator == 0) return;
        XrSessionCoordinatorJni.get().onDrawingSurfaceDestroyed(
                mNativeXrSessionCoordinator, XrSessionCoordinator.this);
    }

    @CalledByNative
    private void onNativeDestroy() {
        // Native destructors should ends sessions before destroying the native XrSessionCoordinator
        // object.
        assert sActiveSessionInstance == null : "unexpected active session in onNativeDestroy";

        mNativeXrSessionCoordinator = 0;
    }

    @NativeMethods
    interface Natives {
        void onDrawingSurfaceReady(long nativeXrSessionCoordinator, XrSessionCoordinator caller,
                Surface surface, WindowAndroid rootWindow, int rotation, int width, int height);
        void onDrawingSurfaceTouch(long nativeXrSessionCoordinator, XrSessionCoordinator caller,
                boolean primary, boolean touching, int pointerId, float x, float y);
        void onDrawingSurfaceDestroyed(
                long nativeXrSessionCoordinator, XrSessionCoordinator caller);
    }
}
