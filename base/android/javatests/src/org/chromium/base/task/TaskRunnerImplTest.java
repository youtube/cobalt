// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.task.SchedulerTestHelpers;

/**
 * Test class for {@link TaskRunner}.
 *
 * Note due to layering concerns we can't test post native functionality in a
 * base javatest. Instead see:
 * content/public/android/javatests/src/org/chromium/content/browser/scheduler/
 * NativePostTaskTest.java
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class TaskRunnerImplTest {
    @Test
    @SmallTest
    public void testPreNativePostTask() {
        TaskRunner taskQueue = new TaskRunnerImpl(TaskTraits.USER_BLOCKING);

        // This should not time out.
        SchedulerTestHelpers.postTaskAndBlockUntilRun(taskQueue);
    }
}
