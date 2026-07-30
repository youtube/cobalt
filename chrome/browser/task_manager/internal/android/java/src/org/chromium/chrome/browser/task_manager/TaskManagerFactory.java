// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.task_manager;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Provides a static method to create a TaskManager instance. */
@NullMarked
public class TaskManagerFactory {
    private static @Nullable TaskManager sInstanceForTesting;

    private TaskManagerFactory() {}

    /** Set the TaskManager instance for testing. */
    public static void setInstanceForTesting(TaskManager taskManager) {
        sInstanceForTesting = taskManager;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    /**
     * @return a TaskManager instance to launch the task manager.
     */
    public static TaskManager createTaskManager() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return new TaskManagerImpl();
    }
}
