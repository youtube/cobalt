// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;
import org.chromium.components.background_task_scheduler.TaskInfo;

/** Unit tests for NotificationTriggerScheduler. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NotificationTriggerSchedulerTest {

    @Mock private NotificationTriggerScheduler.Natives mNativeMock;
    @Mock private BackgroundTaskScheduler mTaskScheduler;
    @Captor private ArgumentCaptor<TaskInfo> mTaskInfoCaptor;

    private NotificationTriggerScheduler mTriggerScheduler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mTaskScheduler);
        NotificationTriggerSchedulerJni.setInstanceForTesting(mNativeMock);
        doReturn(true).when(mTaskScheduler).schedule(any(), mTaskInfoCaptor.capture());

        mTriggerScheduler = new NotificationTriggerScheduler();
    }

    @Test
    public void testTriggerNotifications_CallsNative() {
        mTriggerScheduler.triggerNotifications();
        verify(mNativeMock).triggerNotifications();
    }
}
