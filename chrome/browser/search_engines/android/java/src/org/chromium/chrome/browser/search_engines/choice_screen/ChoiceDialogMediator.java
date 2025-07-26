// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.SearchEngineChoiceService.RefreshReason;
import org.chromium.components.search_engines.SearchEnginesFeatureUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Handles signals coming from various observers, services etc and updates the state of the dialog
 * via its {@link Delegate}.
 *
 * <p>Documentation of the internal state transitions:
 *
 * <ul>
 *   <li>On startup ({@link #startObserving}):
 *       <ul>
 *         <li>this mediator is created, the type is set to {@link DialogType#LOADING} and {@link
 *             #mObservationStartedTimeMillis} is set.
 *         <li>If the supplier response is not available yet, we schedule a task to show the dialog
 *             after {@link
 *             SearchEnginesFeatureUtils#clayBlockingDialogSilentlyPendingDurationMillis}.
 *       </ul>
 *   <li>On supplier update before the dialog is shown ({@link #onIsDeviceChoiceRequiredChanged}):
 *       <ul>
 *         <li>we set the type to {@link DialogType#CHOICE_LAUNCH} and show the dialog. Otherwise,
 *             if the dialog should not be shown, we dismiss the (possibly pending) dialog and
 *             destroy the mediator.
 *       </ul>
 *   <li>On dialog added ({@link #onDialogAdded}):
 *       <ul>
 *         <li>We set {@link #mDialogAddedTimeMillis}, which will then be used to signal that the
 *             dialog was shown. if we didn't get a backend signal at this point, we schedule a task
 *             to auto-unblock the dialog after {@link
 *             SearchEnginesFeatureUtils#clayBlockingDialogTimeoutMillis}.
 *       </ul>
 *   <li>On other supplier updates ({@link #onIsDeviceChoiceRequiredChanged}):
 *       <ul>
 *         <li>If the blocking the user is needed, we set the type to {@link
 *             DialogType#CHOICE_LAUNCH} which will let the users launch the choice screen.
 *         <li>If blocking the user is not needed, we set the type to {@link
 *             DialogType#CHOICE_CONFIRM} to make the dialog non-blocking. If we get this signal
 *             while the dialog is not visible, we destroy the mediator.
 *       </ul>
 * </ul>
 */
class ChoiceDialogMediator {
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused.
    // LINT.IfChange
    @IntDef({
        DialogType.UNKNOWN,
        DialogType.LOADING,
        DialogType.CHOICE_LAUNCH,
        DialogType.CHOICE_CONFIRM
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface DialogType {
        int UNKNOWN = 0;
        int LOADING = 1;
        int CHOICE_LAUNCH = 2;
        int CHOICE_CONFIRM = 3;
        int COUNT = 4;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:OsDefaultsChoiceDialogStatus)

    /** See {@link #startObserving}. */
    interface Delegate {
        /** Rebuilds the view tp match the requested {@code dialogType}. */
        void updateDialogType(@DialogType int dialogType);

        /** Triggers the dialog to be shown. */
        void showDialog();

        /** Dismisses the dialog, whether it's currently shown or pending to be shown. */
        void dismissDialog();

        /**
         * To be called when the mediator is getting destroyed. It does not want to get new updates
         * about clicks, dialog events, etc.
         */
        void onMediatorDestroyed();
    }

    private static final String TAG = "ChoiceDialogMediator";

    /** Duration for which we suppress repeated taps to launch the choice selection screen. */
    @VisibleForTesting static final int DEBOUNCE_TIME_MILLIS = 1000;

    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final SearchEngineChoiceService mSearchEngineChoiceService;
    private final ObservableSupplier<Boolean> mIsDeviceChoiceRequiredSupplier;
    private final Callback<Boolean> mIsDeviceChoiceRequiredObserver;
    private final PauseResumeWithNativeObserver mActivityLifecycleObserver;

    private @DialogType int mDialogType = DialogType.UNKNOWN;

    /**
     * Either the time at which the blocking dialog was shown, {@code null} indicating that the
     * dialog was not shown yet, or {@link Long#MIN_VALUE} indicating that the dialog has been
     * dismissed.
     */
    private @Nullable Long mDialogAddedTimeMillis;

    /**
     * Either the time at which observing the service started, or {@code null} if it didn't happen
     * yet.
     */
    private @Nullable Long mObservationStartedTimeMillis;

    /**
     * Either the time at which the first service event was received, or {@code null} if it didn't
     * happen yet.
     */
    private @Nullable Long mFirstServiceEventTimeMillis;

    /**
     * Time at which the "next" button on the dialog has been tapped and triggered an "initial"
     * request to launch the choice screen. Can be {@code null} if the tap it didn't happen yet, or
     * if Chrome lost the active app status (used as heuristic to detect that we effectively
     * switched to the choice screen).
     *
     * @see #maybeLaunchChoiceScreen()
     */
    private @Nullable Long mLaunchChoiceScreenTimeMillis;

    /**
     * Time at which the "next" button on the dialog has been tapped and triggered a request
     * ("initial" or "repeated") to launch the choice screen. Is used to prevent multi-taps from
     * requesting the choice screen too often. Can be {@code null} if the tap it didn't happen yet,
     * or if Chrome lost the active app status (used as heuristic to detect that we effectively
     * switched to the choice screen).
     *
     * @see #maybeLaunchChoiceScreen()
     */
    private @Nullable Long mLatestAcceptedTapTimeMillis;

    private @Nullable Delegate mDelegate;

    /**
     * Constructs the mediator for the device choice dialog. To become active and start piloting the
     * dialog, call {@link #startObserving}.
     *
     * @param searchEngineChoiceService The service backing the dialog. It is used to determine
     *     whether it needs to be shown, process user actions, etc.
     */
    ChoiceDialogMediator(
            ActivityLifecycleDispatcher lifecycleDispatcher,
            SearchEngineChoiceService searchEngineChoiceService) {
        mLifecycleDispatcher = lifecycleDispatcher;
        mSearchEngineChoiceService = searchEngineChoiceService;
        mIsDeviceChoiceRequiredSupplier =
                searchEngineChoiceService.getIsDeviceChoiceRequiredSupplier();

        // Need to store the lambda reference. As it changes on subsequent calls, it would otherwise
        // be impossible to remove the observer.
        mIsDeviceChoiceRequiredObserver = this::onIsDeviceChoiceRequiredChanged;

        mActivityLifecycleObserver =
                new PauseResumeWithNativeObserver() {
                    @Override
                    public void onResumeWithNative() {
                        searchEngineChoiceService.refreshDeviceChoiceRequiredNow(
                                RefreshReason.APP_RESUME);
                        RecordHistogram.recordEnumeratedHistogram(
                                "Search.OsDefaultsChoice.DialogStatusOnAppOpen",
                                mDialogType,
                                DialogType.COUNT);
                    }

                    @Override
                    public void onPauseWithNative() {
                        if (mLaunchChoiceScreenTimeMillis == null) {
                            // We are navigating away, might be a user-initiated app switch since
                            // we don't have a timestamp recorded for having tapped the button to
                            // launch the choice screen.
                            return;
                        }

                        // Since we have a timestamp here, we assume that the pause is caused by the
                        // choice screen being launched, record it and rearm the delay tracking.
                        recordLaunchChoiceScreenDelay(
                                TimeUtils.currentTimeMillis() - mLaunchChoiceScreenTimeMillis);
                        mLaunchChoiceScreenTimeMillis = null;
                        mLatestAcceptedTapTimeMillis = null;
                    }
                };
    }

    /**
     * Makes the dialog subscribe to changes from the service.
     *
     * @param delegate processes state changes communicated by the mediator and updates the state of
     *     the UI.
     */
    void startObserving(@NonNull Delegate delegate) {
        assert mDelegate == null;
        mDelegate = delegate;

        mObservationStartedTimeMillis = TimeUtils.currentTimeMillis();
        changeDialogType(DialogType.LOADING);

        if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
            // TODO(b/355186707): Temporary log to be removed after e2e validation.
            Log.i(TAG, "Mediator initializing");
        }

        int silentlyPendingDurationMillis =
                SearchEnginesFeatureUtils.clayBlockingDialogSilentlyPendingDurationMillis();
        if (!mIsDeviceChoiceRequiredSupplier.hasValue() && silentlyPendingDurationMillis > 0) {
            // An initial response from the supplier is still pending, so it won't call the observer
            // on registration by itself. It's unclear how long it would take.
            // If a positive `clayBlockingDialogSilentlyPendingDurationMillis()` grace period
            // duration is provided, we proactively trigger the blocking dialog after this time
            // elapses.
            ThreadUtils.postOnUiThreadDelayed(
                    () -> {
                        if (mDialogType != DialogType.LOADING) {
                            // The backend responded quickly enough, and updated the state. We don't
                            // need to show the dialog here anymore.
                            return;
                        }

                        mDelegate.updateDialogType(DialogType.LOADING);
                        mDelegate.showDialog();

                        if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
                            // TODO(b/355186707): Temporary log to be removed after e2e validation.
                            Log.i(TAG, "Dialog shown while waiting for a backend response.");
                        }
                    },
                    silentlyPendingDurationMillis);
        }
        mIsDeviceChoiceRequiredSupplier.addObserver(mIsDeviceChoiceRequiredObserver);
        mLifecycleDispatcher.register(mActivityLifecycleObserver);
    }

    private void destroy() {
        if (mDelegate == null) return;

        // Prevent re-entry.
        var delegate = mDelegate;
        mDelegate = null;

        mLifecycleDispatcher.unregister(mActivityLifecycleObserver);
        mIsDeviceChoiceRequiredSupplier.removeObserver(mIsDeviceChoiceRequiredObserver);
        changeDialogType(DialogType.UNKNOWN);

        delegate.onMediatorDestroyed();
        if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
            // TODO(b/355186707): Temporary log to be removed after e2e validation.
            Log.i(TAG, "Mediator destroyed");
        }
    }

    /** Method to call when the primary action button of the dialog is tapped. */
    void onActionButtonClick() {
        assert mDelegate != null;

        switch (mDialogType) {
            case DialogType.CHOICE_LAUNCH -> recordLaunchChoiceScreenTapHandlingStatus(
                    maybeLaunchChoiceScreen());
            case DialogType.CHOICE_CONFIRM -> mDelegate.dismissDialog();
            case DialogType.LOADING, DialogType.UNKNOWN -> throw new IllegalStateException();
        }
    }

    /** Method to call when the dialog is actually shown. */
    void onDialogAdded() {
        assert mDialogAddedTimeMillis == null
                : "The dialog is not expected to have already been shown";
        assert mDialogType != DialogType.UNKNOWN;
        assert mObservationStartedTimeMillis != null;
        mDialogAddedTimeMillis = TimeUtils.currentTimeMillis();
        mSearchEngineChoiceService.notifyDeviceChoiceBlockShown();

        if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
            // TODO(b/355201070): Replace this after e2e testing with UMA recording.
            Log.i(
                    TAG,
                    "onDialogAdded(), time since observation start: %s millis",
                    mDialogAddedTimeMillis - mObservationStartedTimeMillis);
        }
        scheduleDismissOnDeviceChoiceRequiredUpdateTimeout();
    }

    void onDialogDismissed() {
        destroy();
    }

    @MainThread
    private void onIsDeviceChoiceRequiredChanged(@Nullable Boolean isDeviceChoiceRequired) {
        ThreadUtils.checkUiThread();
        assert mDelegate != null;
        boolean wasDialogShown = mDialogAddedTimeMillis != null;
        boolean wasDialogDismissed = wasDialogShown && mDialogType == DialogType.UNKNOWN;

        if (mFirstServiceEventTimeMillis == null) {
            mFirstServiceEventTimeMillis = TimeUtils.currentTimeMillis();
            if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
                // TODO(b/355201070): Replace this after e2e testing with UMA recording.
                Log.i(
                        TAG,
                        "onIsDeviceChoiceRequiredChanged(%s), time since dialog added: %s millis, "
                                + "time since observation started: %s millis",
                        isDeviceChoiceRequired,
                        wasDialogShown
                                ? mFirstServiceEventTimeMillis - mDialogAddedTimeMillis
                                : "<N/A>",
                        mObservationStartedTimeMillis != null
                                ? mFirstServiceEventTimeMillis - mObservationStartedTimeMillis
                                : "<N/A>");
            }
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Search.OsDefaultsChoice.DelayFromDialogShownToFirstStatus",
                    wasDialogShown ? mFirstServiceEventTimeMillis - mDialogAddedTimeMillis : 0);
            RecordHistogram.deprecatedRecordMediumTimesHistogram(
                    "Search.OsDefaultsChoice.DelayFromObservationToFirstStatus",
                    mObservationStartedTimeMillis == null
                            ? 0
                            : mFirstServiceEventTimeMillis - mObservationStartedTimeMillis);
        }

        if (Boolean.TRUE.equals(isDeviceChoiceRequired) && !wasDialogDismissed) {
            changeDialogType(DialogType.CHOICE_LAUNCH);
            mDelegate.updateDialogType(DialogType.CHOICE_LAUNCH);

            if (!wasDialogShown) {
                mDelegate.showDialog();

                if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
                    // TODO(b/355186707): Temporary log to be removed after e2e validation.
                    Log.i(TAG, "Dialog shown after a positive backend response.");
                }
            }
            return;
        }

        // `isDeviceChoiceRequired` being null indicates that the backend was disconnected, and
        // false indicates that blocking the user is not necessary anymore. In both cases we'll
        // want to unblock the user, but based on which state the UI is in, we may show some
        // confirmation message or not.

        if (wasDialogShown && !wasDialogDismissed) {
            if (Boolean.FALSE.equals(isDeviceChoiceRequired)
                    && (mDialogType == DialogType.LOADING
                            || mDialogType == DialogType.CHOICE_LAUNCH)) {
                // This is the normal flow, showing confirmation after the choice has been made.
                changeDialogType(DialogType.CHOICE_CONFIRM);
                mDelegate.updateDialogType(DialogType.CHOICE_CONFIRM);
                mSearchEngineChoiceService.notifyDeviceChoiceBlockCleared();
                return;
            }

            if (mDialogType == DialogType.CHOICE_CONFIRM) {
                // The backend is sending us some updates while we are showing the confirmation UI.
                // We are not blocking and the user can proceed, so don't do anything about it.
                return;
            }
        }

        // If we get here, this is some sort of error state. Shutdown everything.
        // Indicates that the backend was disconnected. This would make the dialog non-functional if
        // it is still shown, so let's dismiss it and let the user proceed to Chrome.
        // TODO(b/355201070): Add UMA recording.
        if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
            // TODO(b/355186707): Temporary log to be removed after e2e validation.
            Log.w(
                    TAG,
                    "Unexpected backend update received. State: "
                            + "{wasDialogShown=%b, wasDialogDismissed=%b, mDialogType=%s, "
                            + "isDeviceChoiceRequired=%s}",
                    wasDialogShown,
                    wasDialogDismissed,
                    mDialogType,
                    isDeviceChoiceRequired);
        }
        mDelegate.dismissDialog();
        destroy();
    }

    private void scheduleDismissOnDeviceChoiceRequiredUpdateTimeout() {
        if (mDialogType != DialogType.LOADING) {
            return;
        }

        int dialogTimeoutMillis = SearchEnginesFeatureUtils.clayBlockingDialogTimeoutMillis();
        if (dialogTimeoutMillis > 0) {
            ThreadUtils.postOnUiThreadDelayed(
                    () -> {
                        if (mDialogType != DialogType.LOADING) {
                            return; // No-op, we got an update.
                        }

                        assert mDelegate != null; // Unexpected if the type is still "loading".

                        Log.w(
                                TAG,
                                "Timeout waiting for backend block confirmation. Deadline: %s ms",
                                dialogTimeoutMillis);

                        mDelegate.dismissDialog();
                        destroy();
                        RecordHistogram.deprecatedRecordMediumTimesHistogram(
                                "Search.OsDefaultsChoice.DelayFromDialogShownToFirstStatus",
                                dialogTimeoutMillis);
                        RecordHistogram.deprecatedRecordMediumTimesHistogram(
                                "Search.OsDefaultsChoice.DelayFromObservationToFirstStatus",
                                dialogTimeoutMillis);
                    },
                    dialogTimeoutMillis);
        }
    }

    private void changeDialogType(@DialogType int type) {
        if (mDialogType == type) return;

        mDialogType = type;
        RecordHistogram.recordEnumeratedHistogram(
                "Search.OsDefaultsChoice.DialogStatusChange", type, DialogType.COUNT);

        // Reset the debounce logic, to avoid making the button unresponsive.
        mLaunchChoiceScreenTimeMillis = null;
        mLatestAcceptedTapTimeMillis = null;
    }

    /**
     * Processes an incoming tap on the button to launch the choice screen, and returns how it was
     * handled.
     */
    @LaunchChoiceScreenTapHandlingStatus
    private int maybeLaunchChoiceScreen() {
        // The nullability of the 2 timestamps is expected to always be in sync.
        assert (mLaunchChoiceScreenTimeMillis == null) == (mLatestAcceptedTapTimeMillis == null);

        if (mLatestAcceptedTapTimeMillis == null) {
            mLatestAcceptedTapTimeMillis = TimeUtils.currentTimeMillis();
            mLaunchChoiceScreenTimeMillis = TimeUtils.currentTimeMillis();
            mSearchEngineChoiceService.launchDeviceChoiceScreens();
            return LaunchChoiceScreenTapHandlingStatus.INITIAL_TAP;
        }

        if (TimeUtils.currentTimeMillis() - mLatestAcceptedTapTimeMillis <= DEBOUNCE_TIME_MILLIS) {
            // TODO(b/374288328): Consider disabling the button and indicate the "loading" status
            // instead of invisibly debouncing taps.
            return LaunchChoiceScreenTapHandlingStatus.SUPPRESSED_TAP;
        }

        // Accept some repeated taps after a while, it might be due to something going wrong on the
        // backend, and repeating the call might trigger a new attempt that could successfully show
        // the choice screen.
        mLatestAcceptedTapTimeMillis = TimeUtils.currentTimeMillis();
        mSearchEngineChoiceService.launchDeviceChoiceScreens();
        return LaunchChoiceScreenTapHandlingStatus.REPEATED_TAP;
    }

    private static void recordLaunchChoiceScreenDelay(long delayMillis) {
        RecordHistogram.recordTimesHistogram(
                "Search.OsDefaultsChoice.LaunchChoiceScreenDelay", delayMillis);
    }

    @IntDef({
        LaunchChoiceScreenTapHandlingStatus.INITIAL_TAP,
        LaunchChoiceScreenTapHandlingStatus.SUPPRESSED_TAP,
        LaunchChoiceScreenTapHandlingStatus.REPEATED_TAP
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface LaunchChoiceScreenTapHandlingStatus {
        // These values are persisted to logs. Entries should not be renumbered and numeric values
        // should never be reused.
        // LINT.IfChange
        /** No previously tracked tap. */
        int INITIAL_TAP = 0;

        /** The previously accepted tap happened too recently, so this one is ignored. */
        int SUPPRESSED_TAP = 1;

        /**
         * The previously accepted tap doesn't seem to have had a result yet, we let this one
         * through in case the request was dropped and this can allow unblocking the dialog.
         */
        int REPEATED_TAP = 2;

        int COUNT = 3;
        // LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:LaunchOsChoiceScreenTapHandlingStatus)
    }

    private static void recordLaunchChoiceScreenTapHandlingStatus(
            @LaunchChoiceScreenTapHandlingStatus int tapHandlingStatus) {
        RecordHistogram.recordEnumeratedHistogram(
                "Search.OsDefaultsChoice.LaunchChoiceScreenTapHandlingStatus",
                tapHandlingStatus,
                LaunchChoiceScreenTapHandlingStatus.COUNT);
    }
}
