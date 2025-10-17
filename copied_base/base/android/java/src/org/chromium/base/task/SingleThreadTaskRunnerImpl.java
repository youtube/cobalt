// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.task;

import android.os.Handler;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.JNINamespace;

/**
 * Implementation of the abstract class {@link SingleThreadTaskRunner}. Before native initialization
 * tasks are posted to the {@link java android.os.Handler}, after native initialization they're
 * posted to a base::SingleThreadTaskRunner which runs on the same thread.
 */
@JNINamespace("base")
public class SingleThreadTaskRunnerImpl extends TaskRunnerImpl implements SingleThreadTaskRunner {
    @Nullable
    private final Handler mHandler;

    /**
     * @param handler                The backing Handler if any. Note this must run tasks on the
     *                               same thread that the native code runs a task with |traits|.
     *                               If handler is null then tasks won't run until native has
     *                               initialized.
     * @param traits                 The TaskTraits associated with this SingleThreadTaskRunnerImpl.
     */
    public SingleThreadTaskRunnerImpl(Handler handler, @TaskTraits int traits) {
        super(traits, "SingleThreadTaskRunnerImpl", TaskRunnerType.SINGLE_THREAD);
        mHandler = handler;
    }

    @Override
    public boolean belongsToCurrentThread() {
        Boolean belongs = belongsToCurrentThreadInternal();
        if (belongs != null) return belongs.booleanValue();
        assert mHandler != null;
        return mHandler.getLooper().getThread() == Thread.currentThread();
    }

    @Override
    protected void schedulePreNativeTask() {
        // if |mHandler| is null then pre-native task execution is not supported.
        if (mHandler == null) {
            return;
        } else {
            mHandler.post(mRunPreNativeTaskClosure);
        }
    }

    @Override
    protected boolean schedulePreNativeDelayedTask(Runnable task, long delay) {
        if (mHandler == null) return false;
        // In theory it would be fine to delay these tasks until native is initialized and post them
        // to the native task runner, but in practice some tests don't initialize native and still
        // expect delayed tasks to eventually run. There's no good reason not to support them here,
        // there are so few of these tasks that they're very unlikely to cause performance problems.
        mHandler.postDelayed(task, delay);
        return true;
    }
}
