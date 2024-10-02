// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.background_task_scheduler.internal;

import android.app.job.JobParameters;
import android.app.job.JobService;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;

import java.util.HashMap;
import java.util.Map;

/** Delegates calls out to various tasks that need to run in the background. */
public class BackgroundTaskJobService extends JobService {
    private static final String TAG = "BkgrdTaskJS";

    private BackgroundTaskSchedulerJobService.Clock mClock = System::currentTimeMillis;

    @VisibleForTesting
    void setClockForTesting(BackgroundTaskSchedulerJobService.Clock clock) {
        mClock = clock;
    }

    private static class TaskFinishedCallbackJobService
            implements BackgroundTask.TaskFinishedCallback {
        private final BackgroundTaskJobService mJobService;
        private final BackgroundTask mBackgroundTask;
        private final JobParameters mParams;
        private final long mTaskStartTimeMs;

        TaskFinishedCallbackJobService(BackgroundTaskJobService jobService,
                BackgroundTask backgroundTask, JobParameters params) {
            mJobService = jobService;
            mBackgroundTask = backgroundTask;
            mParams = params;

            // We are using uptimeMillis here to record the exact amount of time needed for the task
            // to run that excludes the time spent during deep sleep.
            mTaskStartTimeMs = SystemClock.uptimeMillis();
        }

        @Override
        public void taskFinished(final boolean needsReschedule) {
            // Need to remove the current task from the currently running tasks. All other access
            // happens on the main thread, so do this removal also on the main thread.
            // To ensure that a new job is not immediately scheduled in between removing the task
            // from being a current task and before calling jobFinished, leading to us finishing
            // something with the same ID, call
            // {@link JobService#jobFinished(JobParameters, boolean} also on the main thread.
            ThreadUtils.runOnUiThreadBlocking(new Runnable() {
                @Override
                public void run() {
                    if (!isCurrentBackgroundTaskForJobId()) {
                        Log.e(TAG, "Tried finishing non-current BackgroundTask.");
                        return;
                    }

                    mJobService.mCurrentTasks.remove(mParams.getJobId());
                    mJobService.jobFinished(mParams, needsReschedule);
                    BackgroundTaskSchedulerUma.getInstance().reportTaskFinished(
                            mParams.getJobId(), SystemClock.uptimeMillis() - mTaskStartTimeMs);
                }
            });
        }

        private boolean isCurrentBackgroundTaskForJobId() {
            return mJobService.mCurrentTasks.get(mParams.getJobId()) == mBackgroundTask;
        }
    }

    private final Map<Integer, BackgroundTask> mCurrentTasks = new HashMap<>();

    @Override
    public boolean onStartJob(JobParameters params) {
        ThreadUtils.assertOnUiThread();
        BackgroundTask backgroundTask =
                BackgroundTaskSchedulerFactoryInternal.getBackgroundTaskFromTaskId(
                        params.getJobId());
        if (backgroundTask == null) {
            Log.w(TAG, "Failed to start task. Could not instantiate BackgroundTask class.");
            // Cancel task if the BackgroundTask class is not found anymore. We assume this means
            // that the task has been deprecated.
            BackgroundTaskSchedulerFactoryInternal.getScheduler().cancel(
                    ContextUtils.getApplicationContext(), params.getJobId());
            return false;
        }

        if (BackgroundTaskSchedulerJobService.didTaskExpire(params, mClock.currentTimeMillis())) {
            BackgroundTaskSchedulerUma.getInstance().reportTaskExpired(params.getJobId());
            return false;
        }

        mCurrentTasks.put(params.getJobId(), backgroundTask);

        TaskParameters taskParams =
                BackgroundTaskSchedulerJobService.getTaskParametersFromJobParameters(params);

        BackgroundTaskSchedulerUma.getInstance().reportTaskStarted(taskParams.getTaskId());
        boolean taskNeedsBackgroundProcessing =
                backgroundTask.onStartTask(ContextUtils.getApplicationContext(), taskParams,
                        new TaskFinishedCallbackJobService(this, backgroundTask, params));

        if (!taskNeedsBackgroundProcessing) mCurrentTasks.remove(params.getJobId());
        return taskNeedsBackgroundProcessing;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        ThreadUtils.assertOnUiThread();
        if (!mCurrentTasks.containsKey(params.getJobId())) {
            Log.w(TAG,
                    "Failed to stop job, because job with job id " + params.getJobId()
                            + " does not exist.");
            return false;
        }

        BackgroundTask backgroundTask = mCurrentTasks.get(params.getJobId());

        TaskParameters taskParams =
                BackgroundTaskSchedulerJobService.getTaskParametersFromJobParameters(params);
        BackgroundTaskSchedulerUma.getInstance().reportTaskStopped(taskParams.getTaskId());
        boolean taskNeedsReschedule =
                backgroundTask.onStopTask(ContextUtils.getApplicationContext(), taskParams);
        mCurrentTasks.remove(params.getJobId());
        return taskNeedsReschedule;
    }
}
