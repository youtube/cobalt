// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.util.DisplayMetrics;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.feed.hooks.FeedHooks;
import org.chromium.chrome.browser.feed.hooks.FeedHooksImpl;
import org.chromium.chrome.browser.feed.v2.ContentOrder;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.xsurface.ImageCacheHelper;
import org.chromium.chrome.browser.xsurface.ProcessScope;

import java.lang.reflect.InvocationTargetException;
import java.util.Locale;

/**
 * Bridge for FeedService-related calls.
 */
@JNINamespace("feed")
public final class FeedServiceBridge {
    // Access to JNI test hooks for other libraries. This can go away once more Feed code is
    // migrated to chrome/browser/feed.
    public static org.chromium.base.JniStaticTestMocker<FeedServiceBridge.Natives>
    getTestHooksForTesting() {
        return FeedServiceBridgeJni.TEST_HOOKS;
    }

    private static FeedServiceDependencyProviderFactory getDependencyProviderFactory() {
        Class<?> dependencyProviderFactoryClazz;
        try {
            dependencyProviderFactoryClazz = Class.forName(
                    "org.chromium.chrome.browser.app.feed.FeedServiceDependencyProviderFactoryImpl");
        } catch (ClassNotFoundException e) {
            return null;
        }
        try {
            return (FeedServiceDependencyProviderFactory) dependencyProviderFactoryClazz
                    .getDeclaredMethod("getInstance")
                    .invoke(null);
        } catch (NoSuchMethodException e) {
        } catch (InvocationTargetException e) {
        } catch (IllegalAccessException e) {
        }
        return null;
    }

    private static ProcessScope sXSurfaceProcessScope;

    public static ProcessScope xSurfaceProcessScope() {
        if (sXSurfaceProcessScope != null) {
            return sXSurfaceProcessScope;
        }
        FeedHooks feedHooks = FeedHooksImpl.getInstance();
        if (!feedHooks.isEnabled()) {
            return null;
        }
        sXSurfaceProcessScope = feedHooks.createProcessScope(
                getDependencyProviderFactory().createProcessScopeDependencyProvider());
        return sXSurfaceProcessScope;
    }

    public static void setProcessScopeForTesting(ProcessScope processScope) {
        sXSurfaceProcessScope = processScope;
    }

    private static FeedServiceUtil sFeedServiceUtil;

    public static FeedServiceUtil feedServiceUtil() {
        if (sFeedServiceUtil == null) {
            sFeedServiceUtil = getDependencyProviderFactory().createFeedServiceUtil();
        }
        return sFeedServiceUtil;
    }

    public static boolean isEnabled() {
        return FeedServiceBridgeJni.get().isEnabled();
    }

    /** Returns the top user specified locale. */
    private static Locale getLocale(Context context) {
        return context.getResources().getConfiguration().getLocales().get(0);
    }

    // Java functionality needed for the native FeedService.
    @CalledByNative
    public static String getLanguageTag() {
        return getLocale(ContextUtils.getApplicationContext()).toLanguageTag();
    }
    @CalledByNative
    public static double[] getDisplayMetrics() {
        DisplayMetrics metrics =
                ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
        double[] result = {metrics.density, metrics.widthPixels, metrics.heightPixels};
        return result;
    }

    @CalledByNative
    public static void clearAll() {
        FeedSurfaceTracker.getInstance().clearAll();
    }

    @CalledByNative
    public static void prefetchImage(String url) {
        ProcessScope processScope = xSurfaceProcessScope();
        if (processScope != null) {
            ImageCacheHelper imageCacheHelper = processScope.provideImageCacheHelper();
            if (imageCacheHelper != null) {
                imageCacheHelper.prefetchImage(url);
            }
        }
    }

    @CalledByNative
    public static @TabGroupEnabledState int getTabGroupEnabledState() {
        return feedServiceUtil().getTabGroupEnabledState();
    }

    /** Called at startup to trigger creation of |FeedService|. */
    public static void startup() {
        FeedServiceBridgeJni.get().startup();
    }

    /** Retrieves the config value for load_more_trigger_lookahead. */
    public static int getLoadMoreTriggerLookahead() {
        return FeedServiceBridgeJni.get().getLoadMoreTriggerLookahead();
    }

    /** Retrieves the config value for load_more_trigger_scroll_distance_dp. */
    public static int getLoadMoreTriggerScrollDistanceDp() {
        return FeedServiceBridgeJni.get().getLoadMoreTriggerScrollDistanceDp();
    }

    public static void reportOpenVisitComplete(long visitTimeMs) {
        FeedServiceBridgeJni.get().reportOpenVisitComplete(visitTimeMs);
    }

    public static @VideoPreviewsType int getVideoPreviewsTypePreference() {
        return FeedServiceBridgeJni.get().getVideoPreviewsTypePreference();
    }

    public static void setVideoPreviewsTypePreference(@VideoPreviewsType int videoPreviewsType) {
        FeedServiceBridgeJni.get().setVideoPreviewsTypePreference(videoPreviewsType);
    }

    public static long getReliabilityLoggingId() {
        return FeedServiceBridgeJni.get().getReliabilityLoggingId();
    }

    public static boolean isAutoplayEnabled() {
        return FeedServiceBridgeJni.get().isAutoplayEnabled();
    }

    @ContentOrder
    public static int getContentOrderForWebFeed() {
        return FeedServiceBridgeJni.get().getContentOrderForWebFeed();
    }

    public static void setContentOrderForWebFeed(@ContentOrder int contentOrder) {
        FeedServiceBridgeJni.get().setContentOrderForWebFeed(contentOrder);
    }

    /**
     * Reports that a user action occurred which is untied to a Feed tab. Use
     * FeedStream.reportOtherUserAction for stream-specific actions.
     */
    public static void reportOtherUserAction(
            @StreamKind int streamKind, @FeedUserActionType int userAction) {
        FeedServiceBridgeJni.get().reportOtherUserAction(streamKind, userAction);
    }

    /**
     * @return True if the user is signed in for feed purposes (i.e. if a personalized feed can be
     *         requested).
     */
    public static boolean isSignedIn() {
        return FeedServiceBridgeJni.get().isSignedIn();
    }

    /** Observes whether or not the Feed stream contains unread content */
    public static class UnreadContentObserver {
        private long mNativePtr;

        /**
         * Begins observing.
         *
         * @param isWebFeed  Whether to observe the Web Feed, or the For-you Feed.
         */
        public UnreadContentObserver(boolean isWebFeed) {
            mNativePtr = FeedServiceBridgeJni.get().addUnreadContentObserver(this, isWebFeed);
        }

        /** Stops observing. Must be called when this observer is no longer needed */
        public void destroy() {
            FeedServiceBridgeJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }

        /**
         * Called to signal whether unread content is available. Called once after the observer is
         * initialized, and after that, called each time unread content status changes.
         */
        @CalledByNative("UnreadContentObserver")
        public void hasUnreadContentChanged(boolean hasUnreadContent) {}
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabled();
        void startup();
        int getLoadMoreTriggerLookahead();
        int getLoadMoreTriggerScrollDistanceDp();
        void reportOpenVisitComplete(long visitTimeMs);
        int getVideoPreviewsTypePreference();
        void setVideoPreviewsTypePreference(int videoPreviewsType);
        long getReliabilityLoggingId();
        boolean isAutoplayEnabled();
        void reportOtherUserAction(@StreamKind int streamKind, @FeedUserActionType int userAction);
        @ContentOrder
        int getContentOrderForWebFeed();
        void setContentOrderForWebFeed(@ContentOrder int contentOrder);

        long addUnreadContentObserver(Object object, boolean isWebFeed);
        boolean isSignedIn();
        @NativeClassQualifiedName("feed::JavaUnreadContentObserver")
        void destroy(long nativePtr);
    }
}
