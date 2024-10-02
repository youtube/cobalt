// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.xsurface.ImageFetchClient;
import org.chromium.chrome.browser.xsurface.LoggingParameters;
import org.chromium.chrome.browser.xsurface.PersistentKeyValueCache;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;
import org.chromium.components.version_info.VersionConstants;

/**
 * Provides logging and context for all surfaces.
 */
@JNINamespace("feed::android")
public class FeedProcessScopeDependencyProvider implements ProcessScopeDependencyProvider {
    private static final String FEED_SPLIT_NAME = "feedv2";

    private Context mContext;
    private ImageFetchClient mImageFetchClient;
    private FeedPersistentKeyValueCache mPersistentKeyValueCache;
    private LibraryResolver mLibraryResolver;
    private PrivacyPreferencesManager mPrivacyPreferencesManager;
    private String mApiKey;

    public FeedProcessScopeDependencyProvider(
            String apiKey, PrivacyPreferencesManager privacyPreferencesManager) {
        mContext = createFeedContext(ContextUtils.getApplicationContext());
        mImageFetchClient = new FeedImageFetchClient();
        mPersistentKeyValueCache = new FeedPersistentKeyValueCache();
        mPrivacyPreferencesManager = privacyPreferencesManager;
        mApiKey = apiKey;
        if (BundleUtils.isIsolatedSplitInstalled(FEED_SPLIT_NAME)) {
            mLibraryResolver = (libName) -> {
                return BundleUtils.getNativeLibraryPath(libName, FEED_SPLIT_NAME);
            };
        }
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public ImageFetchClient getImageFetchClient() {
        return mImageFetchClient;
    }

    @Override
    public PersistentKeyValueCache getPersistentKeyValueCache() {
        return mPersistentKeyValueCache;
    }

    @Override
    public void logError(String tag, String format, Object... args) {
        Log.e(tag, format, args);
    }

    @Override
    public void logWarning(String tag, String format, Object... args) {
        Log.w(tag, format, args);
    }

    @Override
    public void postTask(int taskType, Runnable task, long delayMs) {
        @TaskTraits
        int traits;
        switch (taskType) {
            case ProcessScopeDependencyProvider.TASK_TYPE_UI_THREAD:
                traits = TaskTraits.UI_DEFAULT;
                break;
            case ProcessScopeDependencyProvider.TASK_TYPE_BACKGROUND_MAY_BLOCK:
                traits = TaskTraits.BEST_EFFORT_MAY_BLOCK;
                break;
            default:
                assert false : "Invalid task type";
                return;
        }
        PostTask.postDelayedTask(traits, task, delayMs);
    }

    @Override
    public LibraryResolver getLibraryResolver() {
        return mLibraryResolver;
    }

    @Override
    public boolean isXsurfaceUsageAndCrashReportingEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.XSURFACE_METRICS_REPORTING)
                && mPrivacyPreferencesManager.isMetricsReportingEnabled();
    }

    public static Context createFeedContext(Context context) {
        return BundleUtils.createContextForInflation(context, FEED_SPLIT_NAME);
    }

    @Override
    public long getReliabilityLoggingId() {
        return FeedServiceBridge.getReliabilityLoggingId();
    }

    @Override
    public String getGoogleApiKey() {
        return mApiKey;
    }

    @Override
    public String getChromeVersion() {
        return VersionConstants.PRODUCT_VERSION;
    }

    @Override
    public int getChromeChannel() {
        return VersionConstants.CHANNEL;
    }

    @Override
    public int getImageMemoryCacheSizePercentage() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE,
                "image_memory_cache_size_percentage",
                /*default=*/100);
    }

    @Override
    public int getBitmapPoolSizePercentage() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.FEED_IMAGE_MEMORY_CACHE_SIZE_PERCENTAGE,
                "bitmap_pool_size_percentage",
                /*default=*/100);
    }

    @Override
    public int[] getExperimentIds() {
        return FeedProcessScopeDependencyProviderJni.get().getExperimentIds();
    }

    @Override
    public ProcessScopeDependencyProvider.FeatureStateProvider getFeatureStateProvider() {
        return new ProcessScopeDependencyProvider.FeatureStateProvider() {
            @Override
            public boolean isFeatureActive(String featureName) {
                return ChromeFeatureList.isEnabled(featureName);
            }

            @Override
            public boolean getBooleanParameterValue(
                    String featureName, String paramName, boolean defaultValue) {
                return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        featureName, paramName, defaultValue);
            }

            @Override
            public int getIntegerParameterValue(
                    String featureName, String paramName, int defaultValue) {
                return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        featureName, paramName, defaultValue);
            }

            @Override
            public double getDoubleParameterValue(
                    String featureName, String paramName, double defaultValue) {
                return ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                        featureName, paramName, defaultValue);
            }
        };
    }

    /**
     * Stores a view FeedAction for eventual upload. 'data' is a serialized FeedAction protobuf
     * message.
     */
    @Override
    public void processViewAction(byte[] data, LoggingParameters loggingParameters) {
        FeedProcessScopeDependencyProviderJni.get().processViewAction(
                data, FeedLoggingParameters.convertToProto(loggingParameters).toByteArray());
    }

    @Override
    public void reportOnUploadVisibilityLog(@VisibilityLogType int logType, boolean success) {
        switch (logType) {
            case VisibilityLogType.VIEW:
                RecordHistogram.recordBooleanHistogram(
                        "ContentSuggestions.Feed.UploadVisibilityLog.View", success);
                break;
            case VisibilityLogType.CLICK:
                RecordHistogram.recordBooleanHistogram(
                        "ContentSuggestions.Feed.UploadVisibilityLog.Click", success);
                break;
            case VisibilityLogType.UNSPECIFIED:
                // UNSPECIFIED deliberately not reported independently, but will be
                // included in the base UploadVisibilityLog histogram below.
                break;
        }
        RecordHistogram.recordBooleanHistogram(
                "ContentSuggestions.Feed.UploadVisibilityLog", success);
    }

    @Override
    public void reportVisibilityLoggingEnabled(boolean enabled) {
        RecordHistogram.recordBooleanHistogram(
                "ContentSuggestions.Feed.VisibilityLoggingEnabled", enabled);
    }

    @VisibleForTesting
    @NativeMethods
    public interface Natives {
        int[] getExperimentIds();
        String getSessionId();
        void processViewAction(byte[] actionData, byte[] loggingParameters);
    }
}
