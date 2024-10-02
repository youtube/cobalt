// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.background_sync;

import android.content.Context;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.device.DeviceConditions;
import org.chromium.components.background_task_scheduler.NativeBackgroundTask;
import org.chromium.components.background_task_scheduler.TaskIds;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.net.ConnectionType;

/**
 * Handles servicing of Background Sync background tasks coming via
 * background_task_scheduler component.
 */
public class BackgroundSyncBackgroundTask extends NativeBackgroundTask {
    @Override
    public @StartBeforeNativeResult int onStartTaskBeforeNativeLoaded(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        assert taskParameters.getTaskId() == TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID;

        // Check that we're called with network connectivity.
        @ConnectionType
        int current_network_type = DeviceConditions.getCurrentNetConnectionType(context);
        if (current_network_type == ConnectionType.CONNECTION_NONE) {
            return StartBeforeNativeResult.RESCHEDULE;
        }

        return StartBeforeNativeResult.LOAD_NATIVE;
    }

    @Override
    protected void onStartTaskWithNative(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        // Record the delay from soonest expected wakeup time.
        long delayFromExpectedMs = System.currentTimeMillis()
                - taskParameters.getExtras().getLong(
                        BackgroundSyncBackgroundTaskScheduler.SOONEST_EXPECTED_WAKETIME);
        RecordHistogram.recordLongTimesHistogram(
                "BackgroundSync.Wakeup.DelayTime", delayFromExpectedMs);

        // Call into native code to fire any ready background sync events, and
        // wait for it to finish doing so.
        BackgroundSyncBackgroundTaskJni.get().fireOneShotBackgroundSyncEvents(
                () -> { callback.taskFinished(/* needsReschedule= */ false); });
    }

    @Override
    protected boolean onStopTaskBeforeNativeLoaded(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID;

        // Native didn't complete loading, but it was supposed to.
        // Presume we need to reschedule.
        return true;
    }

    @Override
    protected boolean onStopTaskWithNative(Context context, TaskParameters taskParameters) {
        assert taskParameters.getTaskId() == TaskIds.BACKGROUND_SYNC_ONE_SHOT_JOB_ID;

        // The method is called when the task was interrupted due to some reason.
        // It is not called when the task finishes successfully. Reschedule so
        // we can attempt it again.
        return true;
    }

    @NativeMethods
    interface Natives {
        void fireOneShotBackgroundSyncEvents(Runnable callback);
    }
}
