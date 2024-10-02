// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.os.Build;
import android.os.PersistableBundle;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.VariationsFastFetchModeUtils;
import org.chromium.android_webview.common.variations.VariationsServiceMetricsHelper;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedFetchInfo;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

import java.net.HttpURLConnection;
import java.util.Date;
import java.util.Random;
import java.util.concurrent.Executor;
import java.util.concurrent.TimeUnit;

/**
 * AwVariationsSeedFetcher is a JobService which periodically downloads the variations seed.
 * The job is scheduled whenever an app requests the seed, and it's been at least 1 day
 * since the last fetch. If WebView is never used, the job will never run. The 1-day minimum fetch
 * period is chosen as a trade-off between seed freshness (and prompt delivery of feature
 * killswitches) and data and battery usage. Various Android versions may enforce longer periods,
 * depending on WebView usage and battery-saving features. AwVariationsSeedFetcher is not meant to
 * be used outside the variations service. For the equivalent fetch in Chrome, see
 * AsyncInitTaskRunner$FetchSeedTask.
 */
// TODO(https://crbug.com/1328637): consider using BackgroundTaskScheduler instead of JobService
public class AwVariationsSeedFetcher extends JobService {
    @VisibleForTesting
    public static final String JOB_REQUEST_COUNT_KEY = "RequestCount";
    @VisibleForTesting
    public static final int JOB_MAX_REQUEST_COUNT = 5;
    // Represents whether the currently scheduled job is Fast Mode seed fetches or a normal seed
    // fetch. This also enables the seed fetcher to check the status of whether a SafeMode seed
    // fetch has already been requested, preventing unnecessary repeated requests and enabling the
    // seed fetcher to determine if a regularly shceduled seed fetch request should be cancelled.
    @VisibleForTesting
    public static final String JOB_REQUEST_FAST_MODE = "RequestFastMode";

    private static final String TAG = "AwVariationsSeedFet-";
    private static final int JOB_ID = TaskIds.WEBVIEW_VARIATIONS_SEED_FETCH_JOB_ID;
    private static final long MIN_JOB_PERIOD_MILLIS = TimeUnit.HOURS.toMillis(12);
    private static final int JOB_BACKOFF_POLICY = JobInfo.BACKOFF_POLICY_EXPONENTIAL;
    private static final long JOB_INITIAL_BACKOFF_TIME_IN_MS = TimeUnit.MINUTES.toMillis(5);

    /** Clock used to fake time in tests. */
    public interface Clock { long currentTimeMillis(); }

    private static JobScheduler sMockJobScheduler;
    private static VariationsSeedFetcher sMockDownloader;
    private static Clock sTestClock;
    private static Date sDate;
    private static boolean sUseZeroJitter;

    private FetchTask mFetchTask;
    // Since fast mode seed fetches are scheduled on a delay, this prevents scheduling
    // multiple fast seed fetch jobs.
    private static boolean sPendingFastFetchModeScheduling;

    private static long currentTimeMillis() {
        if (sTestClock != null) {
            return sTestClock.currentTimeMillis();
        }
        return System.currentTimeMillis();
    }

    private static String getChannelStr() {
        switch (VersionConstants.CHANNEL) {
            case Channel.STABLE: return "stable";
            case Channel.BETA:   return "beta";
            case Channel.DEV:    return "dev";
            case Channel.CANARY: return "canary";
            default: return null;
        }
    }

    private static JobInfo getPendingJob(JobScheduler scheduler) {
        return scheduler.getPendingJob(JOB_ID);
    }

    private static JobScheduler getScheduler() {
        if (sMockJobScheduler != null) return sMockJobScheduler;

        // This may be null due to vendor framework bugs. https://crbug.com/968636
        return (JobScheduler) ContextUtils.getApplicationContext().getSystemService(
                Context.JOB_SCHEDULER_SERVICE);
    }

    /**
     * Determines whether the currently scheduled job is in Fast Mode.
     */
    private static Boolean isFastModeJob(@Nullable PersistableBundle bundle) {
        if (bundle == null) return false;
        // Default to assume WebView is not in Fast Mode
        return bundle.getBoolean(JOB_REQUEST_FAST_MODE);
    }

    /**
     * Cancels SafeMode seed fetch jobs on the scheduler.
     * Assumes that scheduleIfNeeded(true) has been previously called during activation
     * of FastFetch mode, causing periodic seed fetch requests. This prevents unnecessary
     * periodic seed fetches, instead of the usual single-fire seed fetch requests for
     * non-SafeMode seed fetches.
     */
    public static void cancelSafeModeSeedFetchSchedulerJob() {
        JobScheduler scheduler = getScheduler();
        if (scheduler == null) return;
        if (getPendingJob(scheduler) != null) {
            VariationsUtils.debugLog("Cancelling SafeMode seed download job.");
            scheduler.cancel(JOB_ID);
        }
    }

    /**
     * This method returns whether the incoming job request should be scheduled or not based on
     * three conditions in descending order of priority:
     * 1. No job is currently scheduled
     * 2. Whether the command line switch - finch-seed-ignore-pending-download is set
     * 3. The incoming job request is for Fast Mode and there is a regular seed fetch job already
     * scheduled
     *
     * It also cancels currently scheduled jobs as need based on two conditions:
     * 1. The command line switch - finch-seed-ignore-pending-download is set and the currently
     * scheduled job is for Fast Mode. This prevents multiple periodic seed fetch jobs, since the
     * Fast Mode jobs execute periodically.
     * 2. If no Fast Mode job is scheduled and the incoming job request is for a Fast Mode job.
     * This occurs when a regular seed fetch job has been scheduled, but should be canceled to free
     * up resources for the higher priority Fast Mode job. Note: These conditions are dependent on a
     * job being scheduled. Otherwise, no jobs can be canceled.
     *
     * @param scheduler Reference to the job scheduler
     * @param requireFastMode Indicates if the incoming job request is for Fast Mode
     *
     * @return A return value of true indicates the scheduler will NOT schedule a new job. A return
     *         value of false means that the scheduler will schedule the new job as requested.
     */
    private static boolean handlePreviouslyScheduledJob(
            JobScheduler scheduler, boolean requireFastMode) {
        // Check if a job is already scheduled.
        JobInfo jobInfo = getPendingJob(scheduler);
        if (jobInfo == null) return false;

        PersistableBundle bundle = jobInfo.getExtras();
        Boolean inFastMode = isFastModeJob(bundle);
        boolean ignorePendingDownload =
                CommandLine.getInstance().hasSwitch(AwSwitches.FINCH_SEED_IGNORE_PENDING_DOWNLOAD);

        if (ignorePendingDownload) {
            if (inFastMode) {
                // Prevent multiple periodic Fast Mode seed fetches
                scheduler.cancel(JOB_ID);
            }
            return false;
        }
        if (!inFastMode && requireFastMode) {
            // This is when a regular seed fetch job is scheduled, no SafeMode seed fetch is
            // scheduled. There is a need to cancel this seed fetch and reschedule the
            // higher priority, unrestricted seed fetch.

            // The situation where scheduleIfNeeded(true) is called is
            // only expected to ever occur once before a deactivate command is subsequently
            // made, cancelling all seed fetch jobs.
            scheduler.cancel(JOB_ID);
            VariationsUtils.debugLog("Regular seed download job already scheduled. "
                    + "Canceling for Fast Mode job.");
            return false;
        }

        // A job is already scheduled and meets our requirements
        VariationsUtils.debugLog("Seed download job already scheduled");
        return true;
    }

    /** Should only be called by {@link VariationsSeedServer} for non-SafeMode seed fetches. */
    public static void scheduleIfNeeded() {
        scheduleIfNeeded(false);
    }

    /**
     * There are two types of calls made to this scheduler - SafeMode and regular seed fetches.
     * Normal seed fetches are made via bound IPC through {@link VariationsSeedServer}.
     * SafeMode seed fetches are made through {@link NonEmbeddedFastVariationsSeedSafeModeAction}.
     *
     * @param fastVariationsSeed Indicates whether the request is for a SafeMode seed fetch,
     * which requires a higher priority and less restricted seed fetch job, or a regular seed fetch
     * job.
     */
    public static void scheduleIfNeeded(boolean requireFastMode) {
        JobScheduler scheduler = getScheduler();
        if (scheduler == null) return;
        boolean alreadyScheduled = handlePreviouslyScheduledJob(scheduler, requireFastMode);
        if (alreadyScheduled) return;

        // Awaiting delayed fast variations seed request due to postDelayedTask
        if (sPendingFastFetchModeScheduling) {
            return;
        }

        // Note: we don't throttle fast mode, since we need to set up a periodic job,
        // and it already has random delay on the initial fetch.
        if (!requireFastMode && hasFetchTaskRunRecently()) {
            VariationsUtils.debugLog("Throttling seed download job");
            return;
        }

        VariationsUtils.debugLog("Scheduling seed download job");
        if (!requireFastMode || sUseZeroJitter) {
            scheduleJob(scheduler, requireFastMode);
            return;
        }
        // The jitter is used to create a more uniform distribution of seed fetch requests for
        // the population. Adding jitter to the initial request helps space them out more evenly
        // as the mitigation is deployed so the seed fetches are not requested all at once
        // even if SafeMode is enabled simultaneously on many devices.
        sPendingFastFetchModeScheduling = true;
        int jitter =
                new Random().nextInt((int) VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS);
        PostTask.postDelayedTask(TaskTraits.USER_VISIBLE,
                () -> { scheduleJob(scheduler, requireFastMode); }, /*delay=*/jitter);
    }

    private static boolean hasFetchTaskRunRecently() {
        // Check how long it's been since FetchTask last ran.
        long lastRequestTime = VariationsUtils.getStampTime();
        if (lastRequestTime != 0) {
            long now = currentTimeMillis();
            long minJobPeriodMillis = VariationsUtils.getDurationSwitchValueInMillis(
                    AwSwitches.FINCH_SEED_MIN_DOWNLOAD_PERIOD, MIN_JOB_PERIOD_MILLIS);
            // At this point when requireFastMode == true, we have likely recently
            // scheduled/completed a regular seed fetch
            // On top of that FastMode is expected to only ever schedule a periodic job once
            // Since we still want to schedule a periodic seed fetch, ignore the minimum
            // seed fetch request time frame (once) so that we can still schedule the periodic
            // seed fetch for Fast Mode.
            return now < lastRequestTime + minJobPeriodMillis;
        }
        return false;
    }

    private static void scheduleJob(JobScheduler scheduler, Boolean requireFastMode) {
        Context context = ContextUtils.getApplicationContext();
        ComponentName thisComponent = new ComponentName(context, AwVariationsSeedFetcher.class);
        PersistableBundle extras = new PersistableBundle(/*capacity=*/2);
        extras.putInt(JOB_REQUEST_COUNT_KEY, 0);
        extras.putBoolean(JOB_REQUEST_FAST_MODE, requireFastMode);
        JobInfo.Builder builder =
                new JobInfo.Builder(JOB_ID, thisComponent)
                        .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                        .setBackoffCriteria(JOB_INITIAL_BACKOFF_TIME_IN_MS, JOB_BACKOFF_POLICY)
                        .setExtras(extras);
        if (requireFastMode) {
            builder.setPeriodic(VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                builder = JobBuildHelperForTiramisu.expediteAndRaisePriority(builder);
            }
        } else {
            boolean requiresCharging = !CommandLine.getInstance().hasSwitch(
                    AwSwitches.FINCH_SEED_NO_CHARGING_REQUIREMENT);
            builder = builder.setRequiresCharging(requiresCharging);
        }
        if (scheduler.schedule(builder.build()) == JobScheduler.RESULT_SUCCESS) {
            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromVariationsSharedPreferences(context);
            metrics.setLastEnqueueTime(currentTimeMillis());
            if (!metrics.writeMetricsToVariationsSharedPreferences(context)) {
                Log.e(TAG, "Failed to write variations SharedPreferences to disk");
            }
        } else {
            Log.e(TAG, "Failed to schedule job");
        }
        sPendingFastFetchModeScheduling = false;
    }

    @RequiresApi(Build.VERSION_CODES.TIRAMISU)
    private static class JobBuildHelperForTiramisu {
        private JobBuildHelperForTiramisu() {}

        public static JobInfo.Builder expediteAndRaisePriority(JobInfo.Builder builder) {
            return builder.setExpedited(true).setPriority(JobInfo.PRIORITY_MAX);
        }
    }

    private class FetchTask extends BackgroundOnlyAsyncTask<Void> {
        private JobParameters mParams;

        FetchTask(JobParameters params) {
            mParams = params;
        }

        @Override
        protected Void doInBackground() {
            // Should we call onFinished at the end of this task?
            boolean shouldFinish = true;
            // Should we retry the job?
            boolean needsReschedule = false;
            long startTime = currentTimeMillis();
            PersistableBundle bundle = mParams != null ? mParams.getExtras() : null;
            Boolean fastMode = isFastModeJob(bundle);

            try {
                VariationsUtils.updateStampTime();
                SeedInfo info = VariationsUtils.readSeedFile(VariationsUtils.getSeedFile());
                VariationsUtils.debugLog(
                        String.format("Downloading new seed [fastMode=%B]", fastMode));

                VariationsSeedFetcher downloader =
                        sMockDownloader != null ? sMockDownloader : VariationsSeedFetcher.get();
                String milestone = String.valueOf(VersionConstants.PRODUCT_MAJOR_VERSION);

                final VariationsSeedFetcher.SeedFetchParameters params =
                        VariationsSeedFetcher.SeedFetchParameters.Builder.newBuilder()
                                .setPlatform(
                                        VariationsSeedFetcher.VariationsPlatform.ANDROID_WEBVIEW)
                                .setRestrictMode(null)
                                .setMilestone(milestone)
                                .setChannel(getChannelStr())
                                .setIsFastFetchMode(fastMode)
                                .build();
                SeedFetchInfo fetchInfo = downloader.downloadContent(params, info);

                saveMetrics(startTime, /*endTime=*/currentTimeMillis());

                if (isCancelled()) {
                    return null;
                }

                // VariationsSeedFetcher returns HttpURLConnection.HTTP_NOT_MODIFIED if seed did not
                // change server-side, or HttpURLConnection.HTTP_OK if a new seed was successfully
                // fetched
                if (HttpURLConnection.HTTP_OK != fetchInfo.seedFetchResult
                        && HttpURLConnection.HTTP_NOT_MODIFIED != fetchInfo.seedFetchResult) {
                    int requestCount = 0;
                    if (bundle != null) {
                        requestCount = bundle.getInt(JOB_REQUEST_COUNT_KEY) + 1;
                        bundle.putInt(JOB_REQUEST_COUNT_KEY, requestCount);
                    }
                    // Limit the retries to JOB_MAX_REQUEST_COUNT.
                    needsReschedule = (requestCount <= JOB_MAX_REQUEST_COUNT);
                }
                if (fetchInfo.seedInfo != null) {
                    if (fastMode) {
                        VariationsSeedHolder.getInstance().updateSeedFilesSynchronously(
                                fetchInfo.seedInfo);
                        onFinished(mParams, /*needsReschedule=*/false);
                    } else {
                        VariationsSeedHolder.getInstance().updateSeed(fetchInfo.seedInfo,
                                /*onFinished=*/
                                () -> onFinished(mParams, /*needsReschedule=*/false));
                    }
                    shouldFinish = false; // jobFinished will be deferred until updateSeed is done.
                }
            } finally {
                if (shouldFinish) onFinished(mParams, needsReschedule);
            }

            return null;
        }

        private void saveMetrics(long startTime, long endTime) {
            Context context = ContextUtils.getApplicationContext();
            VariationsServiceMetricsHelper metrics =
                    VariationsServiceMetricsHelper.fromVariationsSharedPreferences(context);
            if (metrics.hasLastEnqueueTime()) {
                metrics.setJobQueueTime(startTime - metrics.getLastEnqueueTime());
            }
            if (metrics.hasLastJobStartTime()) {
                metrics.setJobInterval(startTime - metrics.getLastJobStartTime());
            }
            metrics.clearLastEnqueueTime();
            metrics.setLastJobStartTime(startTime);
            if (!metrics.writeMetricsToVariationsSharedPreferences(context)) {
                Log.e(TAG, "Failed to write variations SharedPreferences to disk");
            }
        }
    }

    @Override
    public boolean onStartJob(JobParameters params) {
        // If this process has survived since the last run of this job, mFetchTask could still
        // exist. Either way, (re)create it with the new params.
        mFetchTask = new FetchTask(params);
        if (params != null && isFastModeJob(params.getExtras())) {
            final Executor userBlockingExecutor =
                    (Runnable r) -> PostTask.postTask(TaskTraits.USER_BLOCKING, r);
            mFetchTask.executeOnExecutor(userBlockingExecutor);
        } else {
            mFetchTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        }
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        if (mFetchTask != null) {
            mFetchTask.cancel(true);
            mFetchTask = null;
        }
        return false;
    }

    protected void onFinished(JobParameters params, boolean needsReschedule) {
        assert params.getJobId() == JOB_ID;
        jobFinished(params, needsReschedule);
    }

    public static void setMocks(JobScheduler scheduler, VariationsSeedFetcher fetcher) {
        sMockJobScheduler = scheduler;
        sMockDownloader = fetcher;
    }

    public static void setTestClock(Clock clock) {
        sTestClock = clock;
    }

    @VisibleForTesting
    public static void setUseZeroJitterForTesting(boolean useZeroJitter) {
        sUseZeroJitter = useZeroJitter;
    }

    @VisibleForTesting
    public static void setDateForTesting(Date date) {
        sDate = date;
    }

    private static long getCurrentTimestamp() {
        return sDate != null ? sDate.getTime() : new Date().getTime();
    }
}
