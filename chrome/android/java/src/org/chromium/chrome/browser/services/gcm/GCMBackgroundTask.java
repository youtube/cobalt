// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.content.Context;
import android.os.PersistableBundle;

import androidx.annotation.MainThread;

import org.chromium.base.Log;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.components.background_task_scheduler.TaskParameters;
import org.chromium.components.gcm_driver.GCMMessage;

/**
 * Processes jobs that have been scheduled for delivering GCM messages to the native GCM Driver,
 * processing for which may exceed the lifetime of the GcmListenerService.
 */
public class GCMBackgroundTask implements BackgroundTask {
    private static final String TAG = "GCMBackgroundTask";

    /**
     * Called when a GCM message is ready to be delivered to the GCM Driver consumer. Because we
     * don't yet know when a message has been fully processed, the task returns that processing has
     * been completed, and we hope that the system keeps us alive long enough to finish processing.
     *
     * @return Boolean indicating whether the WakeLock for this task must be maintained.
     */
    @MainThread
    @Override
    public boolean onStartTask(
            Context context, TaskParameters taskParameters, TaskFinishedCallback callback) {
        PersistableBundle extras = taskParameters.getExtras();
        GCMMessage message = GCMMessage.createFromPersistableBundle(extras);
        if (message == null) {
            Log.e(TAG, "The received bundle containing message data could not be validated.");
            return false;
        }

        ChromeGcmListenerServiceImpl.dispatchMessageToDriver(message);
        return false;
    }

    /**
     * Called when the system has determined that processing the GCM message must be stopped.
     *
     * @return Boolean indicating whether the task has to be rescheduled.
     */
    @MainThread
    @Override
    public boolean onStopTask(Context context, TaskParameters taskParameters) {
        // The GCM Driver has no mechanism for aborting previously dispatched messages.
        return false;
    }
}
