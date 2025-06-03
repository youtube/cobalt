// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import android.content.Context;
import android.os.SystemClock;
import android.view.LayoutInflater;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.components.crash.PureJavaExceptionReporter;
import org.chromium.ui.widget.Toast;

import java.util.concurrent.TimeUnit;

/**
 * Controls the strategy to start branding, and the duration to show branding.
 */
public class BrandingController {
    private static final String TAG = "CctBrand";
    private static final String PARAM_BRANDING_CADENCE_NAME = "branding_cadence";
    private static final String PARAM_MAX_BLANK_TOOLBAR_TIMEOUT_MS = "max_blank_toolbar_timeout";
    private static final String PARAM_USE_TEMPORARY_STORAGE = "use_temporary_storage";
    private static final String PARAM_ANIMATE_TOOLBAR_ICON_TRANSITION =
            "animate_toolbar_transition";
    private static final int DEFAULT_BRANDING_CADENCE_MS = (int) TimeUnit.HOURS.toMillis(1);
    private static final int DEFAULT_MAX_BLANK_TOOLBAR_TIMEOUT_MS = 500;
    /**
     * The maximum time allowed from CCT Toolbar initialized until it should show the URL and title.
     */
    @VisibleForTesting
    static final int TOTAL_BRANDING_DELAY_MS = 1800;
    /**
     * The maximum time allowed to leave CCT Toolbar blank until showing branding or URL and title.
     */
    public static final IntCachedFieldTrialParameter MAX_BLANK_TOOLBAR_TIMEOUT_MS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
                    PARAM_MAX_BLANK_TOOLBAR_TIMEOUT_MS, DEFAULT_MAX_BLANK_TOOLBAR_TIMEOUT_MS);
    /**
     * The minimum time required between two branding events to shown. If time elapse since last
     * branding is less than this cadence, the branding check decision will be {@link
     * BrandingDecision.NONE}.
     */
    public static final IntCachedFieldTrialParameter BRANDING_CADENCE_MS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
                    PARAM_BRANDING_CADENCE_NAME, DEFAULT_BRANDING_CADENCE_MS);
    /**
     * Use temporary storage for branding launch time. The launch time will not persists to the
     * shared pref, but instead only lasts as long as Chrome is alive. This param is added for
     * easier manual testing and should not be used for official channels.
     */
    public static final BooleanCachedFieldTrialParameter USE_TEMPORARY_STORAGE =
            new BooleanCachedFieldTrialParameter(
                    ChromeFeatureList.CCT_BRAND_TRANSPARENCY, PARAM_USE_TEMPORARY_STORAGE, false);

    /**
     * Whether animation transition will be used for the security icon during toolbar branding.
     * If set to false, the icon transition will be disabled.
     */
    public static final BooleanCachedFieldTrialParameter ANIMATE_TOOLBAR_ICON_TRANSITION =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.CCT_BRAND_TRANSPARENCY,
                    PARAM_ANIMATE_TOOLBAR_ICON_TRANSITION, true);

    private final CallbackController mCallbackController = new CallbackController();
    private final @BrandingDecision OneshotSupplierImpl<Integer> mBrandingDecision =
            new OneshotSupplierImpl<>();
    private final BrandingChecker mBrandingChecker;
    private final Context mContext;
    private final String mBrowserName;
    private final boolean mEnableIconAnimation;
    @Nullable
    private final PureJavaExceptionReporter mExceptionReporter;
    private ToolbarBrandingDelegate mToolbarBrandingDelegate;
    private @Nullable Toast mToast;
    private long mToolbarInitializedTime;
    private boolean mIsDestroyed;
    private boolean mReleaseStorageOnFinished;

    /**
     * Branding controller responsible for showing branding.
     * @param context Context used to fetch package information for embedded app.
     * @param appId The ID for the embedded app.
     * @param browserName The browser name shown on the branding toast.
     * @param exceptionReporter Optional reporter that reports wrong state quietly.
     */
    public BrandingController(Context context, String appId, String browserName,
            @Nullable PureJavaExceptionReporter exceptionReporter) {
        mContext = context;
        mBrowserName = browserName;
        mExceptionReporter = exceptionReporter;
        mEnableIconAnimation = ANIMATE_TOOLBAR_ICON_TRANSITION.getValue();
        mBrandingDecision.onAvailable(
                mCallbackController.makeCancelable((decision) -> maybeMakeBrandingDecision()));
        mReleaseStorageOnFinished =
                ChromeFeatureList.sCctBrandTransparencyMemoryImprovement.isEnabled();

        // TODO(https://crbug.com/1350661): Start branding checker during CCT warm up.
        mBrandingChecker = new BrandingChecker(appId,
                SharedPreferencesBrandingTimeStorage.getInstance(), mBrandingDecision::set,
                BRANDING_CADENCE_MS.getValue(), BrandingDecision.TOAST);
        mBrandingChecker.executeWithTaskTraits(TaskTraits.USER_VISIBLE_MAY_BLOCK);
    }

    /**
     * Register the {@link ToolbarBrandingDelegate} from CCT Toolbar.
     * @param delegate {@link ToolbarBrandingDelegate} instance from CCT Toolbar.
     */
    public void onToolbarInitialized(@NonNull ToolbarBrandingDelegate delegate) {
        if (mIsDestroyed) {
            reportErrorMessage("BrandingController should not be access after destroyed.");
            return;
        }

        mToolbarInitializedTime = SystemClock.elapsedRealtime();
        mToolbarBrandingDelegate = delegate;
        mToolbarBrandingDelegate.setIconTransitionEnabled(mEnableIconAnimation);

        // Start the task to timeout the branding check. If mBrandingChecker already finished,
        // canceling the task does nothing. Does not interrupt if the task is running, since the
        // BrandingChecker#doInBackground will collect metrics at the end.
        PostTask.postDelayedTask(TaskTraits.UI_USER_VISIBLE,
                mCallbackController.makeCancelable(
                        () -> mBrandingChecker.cancel(/*mayInterruptIfRunning*/ false)),
                MAX_BLANK_TOOLBAR_TIMEOUT_MS.getValue());

        // Set location bar to empty as controller is waiting for mBrandingDecision.
        // This should not cause any UI jank even if a decision is made immediately, as
        // state set in CustomTabToolbar#showEmptyLocationBar should be unset in any newer state.
        mToolbarBrandingDelegate.showEmptyLocationBar();

        maybeMakeBrandingDecision();
    }

    /**
     * Make decision after BrandingChecker and mToolbarBrandingDelegate is ready.
     */
    private void maybeMakeBrandingDecision() {
        if (mToolbarBrandingDelegate == null || mBrandingDecision.get() == null) return;

        long timeToolbarEmpty = SystemClock.elapsedRealtime() - mToolbarInitializedTime;
        long remainingBrandingTime = TOTAL_BRANDING_DELAY_MS - timeToolbarEmpty;

        @BrandingDecision
        int brandingDecision = mBrandingDecision.get();
        switch (brandingDecision) {
            case BrandingDecision.NONE:
                mToolbarBrandingDelegate.showRegularToolbar();
                break;
            case BrandingDecision.TOOLBAR:
                showToolbarBranding(remainingBrandingTime);
                break;
            case BrandingDecision.TOAST:
                mToolbarBrandingDelegate.showRegularToolbar();
                showToastBranding(remainingBrandingTime);
                break;
            default:
                assert false : "Unreachable state!";
        }

        finish();
    }

    private void showToolbarBranding(long durationMs) {
        mToolbarBrandingDelegate.showBrandingLocationBar();

        Runnable hideToolbarBranding = () -> {
            mToolbarBrandingDelegate.showRegularToolbar();
        };
        PostTask.postDelayedTask(TaskTraits.UI_DEFAULT,
                mCallbackController.makeCancelable(hideToolbarBranding), durationMs);
    }

    private void showToastBranding(long durationMs) {
        if (mIsDestroyed) {
            reportErrorMessage("Toast should not get accessed after destroyed.");
            return;
        }

        String toastText = mContext.getResources().getString(
                R.string.twa_running_in_chrome_template, mBrowserName);
        TextView runInChromeTextView = (TextView) LayoutInflater.from(mContext).inflate(
                R.layout.custom_tabs_toast_branding_layout, null, false);
        runInChromeTextView.setText(toastText);

        Toast toast =
                new Toast(mContext.getApplicationContext(), /*toastView*/ runInChromeTextView);
        if (mReleaseStorageOnFinished) {
            toast.setDuration(Toast.LENGTH_LONG);
            toast.show();
            PostTask.postDelayedTask(TaskTraits.UI_BEST_EFFORT,
                    mCallbackController.makeCancelable(toast::cancel), durationMs);
            return;
        }
        mToast = toast;
        mToast.setDuration((int) durationMs);
        mToast.show();
    }

    /** Prevent any updates to this instance and cancel all scheduled callbacks. */
    public void destroy() {
        mIsDestroyed = true;
        mCallbackController.destroy();
        mBrandingChecker.cancel(true);
        if (mToast != null) {
            mToast.cancel();
        }
    }

    private void reportErrorMessage(String message) {
        Log.e(TAG, message);
        if (mExceptionReporter != null) {
            mExceptionReporter.createAndUploadReport(new Throwable(message));
        }
    }

    private void finish() {
        // Post the task as it's not important to be complete during branding check.
        PostTask.postTask(TaskTraits.BEST_EFFORT, mCallbackController.makeCancelable(() -> {
            int numberOfPackages = SharedPreferencesBrandingTimeStorage.getInstance().getSize();
            RecordHistogram.recordCount100Histogram(
                    "CustomTabs.Branding.NumberOfClients", numberOfPackages);

            // Release the in-memory share pref from the current session if branding checker didn't
            // timeout.
            if (mReleaseStorageOnFinished && !mBrandingChecker.isCancelled()
                    && !USE_TEMPORARY_STORAGE.getValue()) {
                SharedPreferencesBrandingTimeStorage.resetInstance();
            }
        }));
    }

    @BrandingDecision
    Integer getBrandingDecisionForTest() {
        return mBrandingDecision.get();
    }
}
