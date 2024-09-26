// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverAboveTheFoldRenderResult;
import org.chromium.components.feed.proto.wire.ReliabilityLoggingEnums.DiscoverLaunchResult;

/** JNI bridge making reliability logging methods available to native code. */
@JNINamespace("feed::android")
public class FeedReliabilityLoggingBridge {
    private final long mNativePtr;
    private FeedLaunchReliabilityLogger mLaunchLogger;
    private DiscoverAboveTheFoldRenderResult mRenderResult;
    private boolean mRenderingStarted;
    private DiscoverLaunchResult mLaunchResult;

    public static org.chromium.base.JniStaticTestMocker<FeedReliabilityLoggingBridge.Natives>
    getTestHooksForTesting() {
        return FeedReliabilityLoggingBridgeJni.TEST_HOOKS;
    }

    public FeedReliabilityLoggingBridge() {
        // mLaunchLogger should be null until FeedStream.bind() calls setLogger(). We don't expect
        // mLaunchLogger to be used until then.
        mNativePtr = FeedReliabilityLoggingBridgeJni.get().init(this);
    }

    public void setLogger(FeedReliabilityLogger logger) {
        if (logger != null) {
            mLaunchLogger = logger.getLaunchLogger();
        }
        // The launch logger may not be provided if FeedReliabilityLogger is mocked in testing.
        // In this case, use the default no-op instance.
        if (mLaunchLogger == null) {
            mLaunchLogger = new FeedLaunchReliabilityLogger() {};
        }
    }

    public long getNativePtr() {
        return mNativePtr;
    }

    public void destroy() {
        FeedReliabilityLoggingBridgeJni.get().destroy(mNativePtr);
    }

    @CalledByNative
    public void logOtherLaunchStart(long timestamp) {
        mLaunchLogger.logFeedLaunchOtherStart(timestamp);
    }

    @CalledByNative
    public void logCacheReadStart(long timestamp) {
        mLaunchLogger.logCacheReadStart(timestamp);
    }

    @CalledByNative
    public void logCacheReadEnd(long timestamp, int cacheReadResult) {
        mLaunchLogger.logCacheReadEnd(timestamp, cacheReadResult);
    }

    @CalledByNative
    public void logFeedRequestStart(int requestId, long timestamp) {
        mLaunchLogger.getNetworkRequestReliabilityLogger(requestId).logFeedQueryRequestStart(
                timestamp);
    }

    @CalledByNative
    public void logWebFeedRequestStart(int requestId, long timestamp) {
        mLaunchLogger.getNetworkRequestReliabilityLogger(requestId).logWebFeedRequestStart(
                timestamp);
    }

    @CalledByNative
    public void logSingleWebFeedRequestStart(int requestId, long timestamp) {
        mLaunchLogger.getNetworkRequestReliabilityLogger(requestId).logSingleWebFeedRequestStart(
                timestamp);
    }

    @CalledByNative
    public void logActionsUploadRequestStart(int requestId, long timestamp) {
        mLaunchLogger.getNetworkRequestReliabilityLogger(requestId).logActionsUploadRequestStart(
                timestamp);
    }

    @CalledByNative
    public void logRequestSent(int requestId, long timestamp) {
        mLaunchLogger.getNetworkRequestReliabilityLogger(requestId).logRequestSent(timestamp);
    }

    @CalledByNative
    public void logResponseReceived(int requestId, long serverRecvTimestamp,
            long serverSendTimestamp, long clientRecvTimestamp) {
        mLaunchLogger.getNetworkRequestReliabilityLogger(requestId).logResponseReceived(
                serverRecvTimestamp, serverSendTimestamp, clientRecvTimestamp);
    }

    @CalledByNative
    public void logRequestFinished(int requestId, long timestamp, int canonicalStatus) {
        mLaunchLogger.getNetworkRequestReliabilityLogger(requestId).logRequestFinished(
                timestamp, canonicalStatus);
    }

    @CalledByNative
    public void logAboveTheFoldRender(long timestamp, int aboveTheFoldRenderResult) {
        // It's possible to be called multiple times per launch, so we only log "render start" from
        // the first call.
        if (!mRenderingStarted) {
            mLaunchLogger.logAtfRenderStart(timestamp);
            mRenderingStarted = true;
        }

        // Record mRenderResult to be logged later by onStreamUpdateFinished().
        mRenderResult = DiscoverAboveTheFoldRenderResult.forNumber(aboveTheFoldRenderResult);
        if (mRenderResult == null) {
            mRenderResult = DiscoverAboveTheFoldRenderResult.INTERNAL_ERROR;
        }
    }

    @CalledByNative
    public void logLoadingIndicatorShown(long timestamp) {
        mLaunchLogger.logLoadingIndicatorShown(timestamp);
    }

    @CalledByNative
    public void logLaunchFinishedAfterStreamUpdate(int discoverLaunchResult) {
        if (mLaunchResult != null) return;

        mLaunchResult = DiscoverLaunchResult.forNumber(discoverLaunchResult);
        if (mLaunchResult == null) {
            mLaunchResult = DiscoverLaunchResult.ABORTED_DUE_TO_INVALID_STATE;
        }
    }

    public void onStreamUpdateFinished() {
        if (!mLaunchLogger.isLaunchInProgress()) return;

        if (mRenderResult != null) {
            mLaunchLogger.logAtfRenderEnd(System.nanoTime(), mRenderResult.getNumber());
            mRenderResult = null;
        }

        if (mLaunchResult != null) {
            mLaunchLogger.logLaunchFinished(System.nanoTime(), mLaunchResult.getNumber());
            mLaunchResult = null;
        }

        mRenderingStarted = false;
    }

    public void onStreamUpdateError() {
        if (!mLaunchLogger.isLaunchInProgress()) return;
        mLaunchLogger.logAtfRenderEnd(
                System.nanoTime(), DiscoverAboveTheFoldRenderResult.INTERNAL_ERROR.getNumber());
        mLaunchLogger.logLaunchFinished(
                System.nanoTime(), DiscoverLaunchResult.FAILED_TO_RENDER.getNumber());
        mRenderingStarted = false;
        mRenderResult = null;
        mLaunchResult = null;
    }

    @NativeMethods
    public interface Natives {
        long init(FeedReliabilityLoggingBridge thisRef);
        void destroy(long nativeFeedReliabilityLoggingBridge);
    }
}
