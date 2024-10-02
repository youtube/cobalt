// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/**
 * Reads and caches partner browser customizations information if it exists.
 */
public class PartnerBrowserCustomizations {
    private static final String TAG = "PartnerCustomize";
    private static final String PROVIDER_AUTHORITY = "com.android.partnerbrowsercustomizations";

    /** Default timeout in ms for reading PartnerBrowserCustomizations provider. */
    private static final int DEFAULT_TIMEOUT_MS = 10_000;

    private static final int HOMEPAGE_URL_MAX_LENGTH = 1000;
    // Private homepage structure.
    @VisibleForTesting
    static final String PARTNER_HOMEPAGE_PATH = "homepage";
    @VisibleForTesting
    static final String PARTNER_DISABLE_BOOKMARKS_EDITING_PATH = "disablebookmarksediting";
    @VisibleForTesting
    static final String PARTNER_DISABLE_INCOGNITO_MODE_PATH = "disableincognitomode";
    private static String sProviderAuthority = PROVIDER_AUTHORITY;
    private static Boolean sIgnoreSystemPackageCheck;
    private static Boolean sValid;

    private static volatile PartnerBrowserCustomizations sInstance;

    private volatile GURL mHomepage;
    private volatile boolean mIncognitoModeDisabled;
    private volatile boolean mBookmarksEditingDisabled;
    private boolean mIsInitialized;

    private final List<Runnable> mInitializeAsyncCallbacks;
    private PartnerHomepageListener mListener;

    /**
     * Provider of partner customizations.
     */
    public interface Provider extends CustomizationProviderDelegate {}

    /**
     * Partner customizations provided by ContentProvider package.
     */
    public static class ProviderPackage implements Provider {
        CustomizationProviderDelegate mDelegate;

        public ProviderPackage() {
            mDelegate = new CustomizationProviderDelegateImpl();
        }

        @Override
        public String getHomepage() {
            return mDelegate.getHomepage();
        }

        @Override
        public boolean isIncognitoModeDisabled() {
            return mDelegate.isIncognitoModeDisabled();
        }

        @Override
        public boolean isBookmarksEditingDisabled() {
            return mDelegate.isBookmarksEditingDisabled();
        }
    }

    /**
     * Interface that listen to homepage URI updates from provider packages.
     */
    public interface PartnerHomepageListener {
        /**
         * Will be called if homepage have any update after {@link #initializeAsync(Context, long)}.
         */
        void onHomepageUpdate();
    }

    private PartnerBrowserCustomizations() {
        mInitializeAsyncCallbacks = new ArrayList<>();
    }

    /**
     * @return singleton instance of {@link PartnerBrowserCustomizations}.
     */
    public static PartnerBrowserCustomizations getInstance() {
        if (sInstance == null) {
            sInstance = new PartnerBrowserCustomizations();
        }
        return sInstance;
    }

    /**
     * @return True if the partner homepage content provider exists and enabled. Note that The data
     * this method reads is not initialized until the asynchronous initialization of this class has
     * been completed.
     */
    public boolean isHomepageProviderAvailableAndEnabled() {
        GURL homepageUrl = getHomePageUrl();
        return homepageUrl != null && !homepageUrl.isEmpty();
    }

    /**
     * @return Whether incognito mode is disabled by the partner.
     */
    @CalledByNative
    public static boolean isIncognitoDisabled() {
        return getInstance().isIncognitoModeDisabled();
    }

    /**
     * Set the listener that will receive updates when partner provided homepage changes.
     * @param listener {@link PartnerHomepageListener} that will receive update when partner
     *         provided homepage changes.
     */
    public void setPartnerHomepageListener(PartnerHomepageListener listener) {
        assert mListener == null;
        mListener = listener;
    }

    /**
     * @return Whether incognito mode is disabled by the partner.
     */
    public boolean isIncognitoModeDisabled() {
        return mIncognitoModeDisabled;
    }

    /**
     * @return Whether partner bookmarks editing is disabled by the partner.
     */
    public boolean isBookmarksEditingDisabled() {
        return mBookmarksEditingDisabled;
    }

    /**
     * @return True, if initialization is finished. Checking that there is no provider, or failing
     * to read provider is also considered initialization.
     */
    public boolean isInitialized() {
        return mIsInitialized;
    }

    /**
     * Constructs an async task that reads PartnerBrowserCustomization provider.
     *
     * @param context   The current application context.
     */
    public void initializeAsync(final Context context) {
        initializeAsync(context, DEFAULT_TIMEOUT_MS);
    }

    /**
     * Constructs an async task that reads PartnerBrowserCustomization provider.
     *
     * @param context   The current application context.
     * @param timeoutMs If initializing takes more than this time, cancels it. The unit is ms.
     */
    @VisibleForTesting
    void initializeAsync(final Context context, long timeoutMs) {
        mIsInitialized = false;
        // Setup an initializing async task.
        final AsyncTask<Void> initializeAsyncTask = new AsyncTask<Void>() {
            private boolean mHomepageUriChanged;
            private long mStartTime;

            @Override
            protected Void doInBackground() {
                try {
                    mStartTime = SystemClock.elapsedRealtime();
                    boolean systemOrPreStable =
                            (context.getApplicationInfo().flags & ApplicationInfo.FLAG_SYSTEM) == 1
                            || !VersionInfo.isStableBuild();
                    if (!systemOrPreStable) {
                        // Only allow partner customization if this browser is a system package, or
                        // is in pre-stable channels.
                        return null;
                    }

                    if (isCancelled()) return null;
                    CustomizationProviderDelegateImpl delegate =
                            new CustomizationProviderDelegateImpl();

                    if (ChromeFeatureList.sPartnerHomepageInitialLoadImprovement.isEnabled()) {
                        // Refresh the homepage first, as it has potential impact on the URL to use
                        // for the initial tab.
                        if (isCancelled()) return null;
                        mHomepageUriChanged = refreshHomepage(delegate);
                    }

                    if (isCancelled()) return null;
                    refreshIncognitoModeDisabled(delegate);

                    if (isCancelled()) return null;
                    refreshBookmarksEditingDisabled(delegate);

                    if (!ChromeFeatureList.sPartnerHomepageInitialLoadImprovement.isEnabled()) {
                        if (isCancelled()) return null;
                        mHomepageUriChanged = refreshHomepage(delegate);
                    }
                } catch (Exception e) {
                    Log.w(TAG, "Fetching partner customizations failed", e);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                onFinalized();
            }

            @Override
            protected void onCancelled(Void result) {
                onFinalized();
            }

            private void onFinalized() {
                boolean isFirstFinalized = !mIsInitialized;
                mIsInitialized = true;
                if (isFirstFinalized) {
                    RecordHistogram.recordTimesHistogram(
                            "Android.PartnerBrowserCustomizationInitDuration",
                            SystemClock.elapsedRealtime() - mStartTime);
                }

                for (Runnable callback : mInitializeAsyncCallbacks) {
                    callback.run();
                }
                mInitializeAsyncCallbacks.clear();

                if (mHomepageUriChanged && mListener != null) {
                    mListener.onHomepageUpdate();
                }
                if (isFirstFinalized) {
                    RecordHistogram.recordTimesHistogram(
                            "Android.PartnerBrowserCustomizationInitDuration.WithCallbacks",
                            SystemClock.elapsedRealtime() - mStartTime);
                }
            }
        };

        initializeAsyncTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        // Cancel the initialization if it reaches timeout.
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT, () -> initializeAsyncTask.cancel(true), timeoutMs);
    }

    @VisibleForTesting
    boolean refreshHomepage(CustomizationProviderDelegate delegate) {
        boolean retVal = false;
        try {
            String homepageUrl = delegate.getHomepage();
            GURL homepageGurl;

            // Loosely check the scheme. We call fixupUrl to handle about: schemes correctly, and
            // also to keep consistent with previous behavior, where chrome: urls were fixed up when
            // checking for NTP in isValidHomepage.
            if (homepageUrl != null
                    && (homepageUrl.startsWith(ContentUrlConstants.ABOUT_URL_SHORT_PREFIX)
                            || homepageUrl.startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX))) {
                homepageGurl = UrlFormatter.fixupUrl(homepageUrl);
            } else {
                homepageGurl = new GURL(homepageUrl);
            }

            if (!isValidHomepage(homepageGurl)) {
                homepageGurl = null;
            }
            if (!Objects.equals(mHomepage, homepageGurl)) {
                retVal = true;
            }
            mHomepage = homepageGurl;
            String valueToWrite =
                    mHomepage == null ? GURL.emptyGURL().serialize() : mHomepage.serialize();
            SharedPreferencesManager.getInstance().writeString(
                    ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, valueToWrite);
        } catch (Exception e) {
            Log.w(TAG, "Partner homepage delegate URL read failed : ", e);
        }

        return retVal;
    }

    @VisibleForTesting
    void refreshIncognitoModeDisabled(CustomizationProviderDelegate delegate) {
        try {
            mIncognitoModeDisabled = delegate.isIncognitoModeDisabled();
        } catch (Exception e) {
            Log.w(TAG, "Partner disable incognito mode read failed : ", e);
        }
    }

    @VisibleForTesting
    void refreshBookmarksEditingDisabled(CustomizationProviderDelegate delegate) {
        try {
            mBookmarksEditingDisabled = delegate.isBookmarksEditingDisabled();
        } catch (Exception e) {
            Log.w(TAG, "Partner disable bookmarks editing read failed : ", e);
        }
    }

    /**
     * Sets a callback that will be executed when the initialization is done.
     *
     * @param callback  This is called when the initialization is done.
     */
    public void setOnInitializeAsyncFinished(final Runnable callback) {
        if (mIsInitialized) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, callback);
        } else {
            mInitializeAsyncCallbacks.add(callback);
        }
    }

    /**
     * Sets a callback that will be executed when the initialization is done.
     *
     * @param callback This is called when the initialization is done.
     * @param timeoutMs If initializing takes more than this time since this function is called,
     * force run |callback| early. The unit is ms.
     */
    public void setOnInitializeAsyncFinished(final Runnable callback, long timeoutMs) {
        mInitializeAsyncCallbacks.add(callback);

        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, () -> {
            if (mInitializeAsyncCallbacks.remove(callback)) {
                if (!mIsInitialized) {
                    Log.w(TAG, "mInitializeAsyncCallbacks executed as timeout expired.");
                }
                callback.run();
            }
        }, mIsInitialized ? 0 : timeoutMs);
    }

    public static void destroy() {
        getInstance().destroyInternal();
        sInstance = null;
    }

    private void destroyInternal() {
        mInitializeAsyncCallbacks.clear();
        mListener = null;
    }

    /**
     * @return Home page URL from Android provider. If null, that means either there is no homepage
     * provider or provider set it to null to disable homepage.
     */
    public GURL getHomePageUrl() {
        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(ChromeSwitches.PARTNER_HOMEPAGE_FOR_TESTING)) {
            return new GURL(
                    commandLine.getSwitchValue(ChromeSwitches.PARTNER_HOMEPAGE_FOR_TESTING));
        }
        return mHomepage;
    }

    @VisibleForTesting
    static boolean isValidHomepage(GURL url) {
        if (url == null) {
            return false;
        }

        if (!url.isValid() || (!UrlUtilities.isHttpOrHttps(url) && !UrlUtilities.isNTPUrl(url))) {
            Log.w(TAG,
                    "Partner homepage must be HTTP(S) or NewTabPage. "
                            + "Got invalid URL \"%s\"",
                    url.getPossiblyInvalidSpec());
            return false;
        }
        if (url.getSpec().length() > HOMEPAGE_URL_MAX_LENGTH) {
            Log.w(TAG, "The homepage URL \"%s\" is too long.", url.getSpec());
            return false;
        }
        return true;
    }

    @VisibleForTesting
    public static void setInstanceForTesting(PartnerBrowserCustomizations instance) {
        sInstance = instance;
    }
}
