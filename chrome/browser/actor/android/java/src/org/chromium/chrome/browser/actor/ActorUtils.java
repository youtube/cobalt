// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.build.annotations.NullMarked;

/** Utility methods for Actor tasks. */
@NullMarked
public class ActorUtils {
    /**
     * @param state The {@link ActorTaskState} to check.
     * @return True if the state is completed (finished, failed, or cancelled).
     */
    public static boolean isCompletedState(@ActorTaskState int state) {
        return state == ActorTaskState.FINISHED
                || state == ActorTaskState.FAILED
                || state == ActorTaskState.CANCELLED;
    }

    /**
     * @param state The {@link ActorTaskState} to check.
     * @return True if the state is a running/working state (created, acting, or reflecting).
     */
    public static boolean isRunningState(@ActorTaskState int state) {
        return state == ActorTaskState.CREATED
                || state == ActorTaskState.ACTING
                || state == ActorTaskState.REFLECTING;
    }

    /**
     * @param state The {@link ActorTaskState} to check.
     * @return True if the state is a paused state (paused by actor, paused by user).
     */
    public static boolean isPausedState(@ActorTaskState int state) {
        return state == ActorTaskState.PAUSED_BY_ACTOR || state == ActorTaskState.PAUSED_BY_USER;
    }

    /**
     * @param prevTaskState The first {@link ActorTaskState} to compare.
     * @param newTaskState The second {@link ActorTaskState} to compare.
     * @return True if both states belong to the same logical group (Running, Paused, Waiting on
     *     User, or Completed).
     */
    public static boolean isSameLogicalGroup(
            @ActorTaskState int prevTaskState, @ActorTaskState int newTaskState) {
        if (prevTaskState == newTaskState) return true;
        if (isRunningState(prevTaskState) && isRunningState(newTaskState)) return true;
        if (isPausedState(prevTaskState) && isPausedState(newTaskState)) return true;
        if (isCompletedState(prevTaskState) && isCompletedState(newTaskState)) return true;
        return prevTaskState == ActorTaskState.WAITING_ON_USER
                && newTaskState == ActorTaskState.WAITING_ON_USER;
    }
}
