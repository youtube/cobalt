// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.test.transit.StatusStore.StatusRegion;
import org.chromium.base.test.transit.Transition.TransitionOptions;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Polls multiple {@link Condition}s in parallel. */
public class ConditionWaiter {

    /**
     * The process of waiting for a {@link Condition} to be fulfilled.
     *
     * <p>Tracks the {@link ConditionStatus}es returned over time, how long it took to be fulfilled
     * (or for long it was checked until it timed out).
     *
     * <p>Tracks and aggregates the ConditionStatues for user-friendly printing.
     */
    protected static class ConditionWait {

        private final Condition mCondition;
        private final @ConditionOrigin int mOrigin;
        private boolean mIsInitialWait = true;
        private long mTimeStarted;
        private long mTimeUnfulfilled;
        private long mTimeFulfilled;
        private StatusStore mStatusStore = new StatusStore();

        /**
         * Constructor.
         *
         * @param condition the {@link Condition} that this will wait for.
         * @param origin the origin of the |condition|.
         */
        ConditionWait(Condition condition, @ConditionOrigin int origin) {
            mCondition = condition;
            mOrigin = origin;
        }

        Condition getCondition() {
            return mCondition;
        }

        @ConditionOrigin
        int getOrigin() {
            return mOrigin;
        }

        void markAsDelayedWait() {
            mIsInitialWait = false;
        }

        boolean isInitialWait() {
            return mIsInitialWait;
        }

        private void ensureTimerStarted() {
            if (mTimeStarted > 0) {
                return;
            }

            mTimeStarted = getNow();
            mTimeUnfulfilled = mTimeStarted;
        }

        private boolean update() {
            ConditionStatus status;
            try {
                if (mCondition.isRunOnUiThread()) {
                    // TODO(crbug.com/40284026): Post multiple checks in parallel, the UI thread
                    // will
                    // run them sequentially.
                    status = ThreadUtils.runOnUiThreadBlocking(mCondition::check);
                } else {
                    status = mCondition.check();
                }
            } catch (Exception e) {
                StringWriter sw = new StringWriter();
                e.printStackTrace(new PrintWriter(sw));
                status = Condition.error(sw.toString());
            }

            mStatusStore.report(status);
            if (status.isError()) {
                return true;
            } else if (status.isFulfilled()) {
                reportFulfilledWait(status);
                return false;
            } else {
                reportUnfulfilledWait(status);
                return true;
            }
        }

        /** Report that the Condition being waited on is not fulfilled at this time. */
        private void reportUnfulfilledWait(ConditionStatus status) throws IllegalStateException {
            assert mTimeStarted > 0
                    : "\"" + getCondition().getDescription() + "\"'s wait timer never started";
            mTimeFulfilled = 0;
            mTimeUnfulfilled = status.getTimestamp();
        }

        /** Report that the Condition being waited on is fulfilled at this time. */
        private void reportFulfilledWait(ConditionStatus status) {
            assert mTimeStarted > 0
                    : "\"" + getCondition().getDescription() + "\"'s wait timer never started";
            if (!isFulfilled()) {
                // isFulfilled() will return true after setting a non-zero time.
                mTimeFulfilled = status.getTimestamp();
            }
        }

        /**
         * @return if the Condition is fulfilled.
         */
        private boolean isFulfilled() {
            return mTimeFulfilled > 0;
        }

        /**
         * @return how long the condition has been considered unfulfilled for.
         *     <p>The Condition must be unfulfilled, or an assertion will be raised.
         */
        private long getTimeUnfulfilled() {
            assert !isFulfilled();

            return mTimeUnfulfilled - mTimeStarted;
        }

        /**
         * @return how long the condition took to be fulfilled for the first time. The result is a
         *     pair (lowerBound, upperBound), where the time it took is between these two numbers.
         *     |lowerBound| is the last time at which the Condition was seen as unfulfilled and
         *     |upperBound| is the first time at which the Condition was seen as fulfilled.
         *     <p>The Condition must be fulfilled, or an assertion will be raised.
         */
        private Pair<Long, Long> getTimeToFulfill() {
            assert isFulfilled();

            long minTimeToFulfill = mTimeUnfulfilled - mTimeStarted;
            long maxTimeToFulfill = mTimeFulfilled - mTimeStarted;
            return Pair.create(minTimeToFulfill, maxTimeToFulfill);
        }

        private static long getNow() {
            long now = TimeUtils.currentTimeMillis();
            assert now > 0;
            return now;
        }

        /**
         * @return an aggregation of the statuses reported while checking a Condition.
         */
        public StatusStore getStatusStore() {
            return mStatusStore;
        }
    }

    /** The maximum time to wait for a criteria to become valid. */
    public static final long MAX_TIME_TO_POLL = 5000L;

    /** The polling interval to wait between checking for a satisfied criteria. */
    public static final long POLLING_INTERVAL = 50;

    private static final String TAG = "Transit";

    protected final Transition mTransition;
    protected List<ConditionWait> mWaits;
    protected Map<Condition, ElementFactory> mConditionsGuardingFactories;
    protected final Map<String, ConditionWait> mExitWaitsByElementId = new HashMap<>();

    ConditionWaiter(Transition transition) {
        mTransition = transition;
    }

    protected void onBeforeTransition(boolean failOnAlreadyFulfilled) {
        preCheck(failOnAlreadyFulfilled);
        for (ConditionWait wait : mWaits) {
            wait.getCondition().onStartMonitoring();
        }
    }

    protected void onAfterTransition() {
        for (ConditionWait wait : mWaits) {
            wait.getCondition().onStopMonitoring();
        }
    }

    /**
     * Start timers, perform the first Condition checks before running the Trigger.
     *
     * <p>Ensure at least one Condition is not fulfilled before running the Trigger.
     *
     * <p>This also makes supplied values available for Conditions that implement Supplier before
     * {@link Condition#onStartMonitoring()} is called.
     */
    void preCheck(boolean failOnAlreadyFulfilled) {
        mWaits = createWaits();
        mConditionsGuardingFactories = createFactories();

        if (mWaits.isEmpty()) {
            if (failOnAlreadyFulfilled) {
                throw new IllegalArgumentException(
                        "No conditions to fulfill. If this is expected, use a null Trigger.");
            } else {
                Log.i(TAG, "No conditions to fulfill.");
            }
        }

        for (ConditionWait wait : mWaits) {
            wait.ensureTimerStarted();
        }
        boolean anyCriteriaMissing = processWaits(/* startMonitoringNewWaits= */ false);

        if (!anyCriteriaMissing && failOnAlreadyFulfilled) {
            throw new CriteriaNotSatisfiedException(
                    "All Conditions already fulfilled before running Trigger. If this is expected,"
                        + " use a null Trigger. If this is possible but not necessarily expected,"
                        + " use TransitionOptions.withPossiblyAlreadyFulfilled().\n"
                            + createWaitConditionsSummary(
                                    mWaits, /* generateMainMessage= */ false));
        }
    }

    /**
     * Blocks waiting for multiple {@link Condition}s, polling them and reporting their status to he
     * {@link ConditionWait}es.
     *
     * @throws TravelException if not all {@link Condition}s are fulfilled before timing out.
     */
    void waitFor() throws TravelException {
        TransitionOptions options = mTransition.getOptions();
        long timeoutMs = options.mTimeoutMs != 0 ? options.mTimeoutMs : MAX_TIME_TO_POLL;
        try {
            CriteriaHelper.pollInstrumentationThread(
                    new CheckConditionsOnce(), timeoutMs, POLLING_INTERVAL);
        } catch (CriteriaHelper.TimeoutException timeoutException) {
            // Unwrap the TimeoutException and CriteriaNotSatisfiedException parts of the stack to
            // reduce the error message.
            if (timeoutException.getCause()
                    instanceof CriteriaNotSatisfiedException criteriaNotSatisfiedException) {
                throw TravelException.newTravelException(
                        criteriaNotSatisfiedException.getMessage());
            } else {
                throw timeoutException;
            }
        }

        // Check that all element factories were used.
        if (!mConditionsGuardingFactories.isEmpty()) {
            StringBuilder failureMessage =
                    new StringBuilder("Some Conditions of element factories were not declared:\n");
            for (Condition condition : mConditionsGuardingFactories.keySet()) {
                failureMessage.append("  * ").append(condition.getDescription()).append("\n");
            }
            throw TravelException.newTravelException(failureMessage.toString());
        }
    }

    private List<ConditionWait> createWaits() {
        List<ConditionWait> allWaits = new ArrayList<>();

        Set<String> destinationElementIds = new HashSet<>();
        for (ConditionalState conditionalState : mTransition.getEnteredStates()) {
            final Elements destinationElements = conditionalState.getElements();
            List<ConditionWait> newWaits = createEnterConditionWaits(destinationElements);
            allWaits.addAll(newWaits);
            destinationElementIds.addAll(destinationElements.getElementIds());
        }

        // Create EXIT Conditions for Views that should disappear and LogicalElements that should
        // be false.
        for (ConditionalState conditionalState : mTransition.getExitedStates()) {
            final Elements originElements = conditionalState.getElements();
            for (Element<?> element : originElements.getElements()) {
                Condition exitCondition = element.getExitCondition(destinationElementIds);
                if (exitCondition != null) {
                    ConditionWait conditionWait =
                            new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT);
                    // Keep track of exit waits by element id so that any new
                    // elements added by an element factory can remove matching
                    // previously created exit wait (since the element is now a shared element).
                    mExitWaitsByElementId.put(element.getId(), conditionWait);
                    allWaits.add(conditionWait);
                }
            }

            // Add extra EXIT Conditions.
            for (Condition exitCondition : originElements.getOtherExitConditions()) {
                allWaits.add(
                        new ConditionWait(exitCondition, ConditionWaiter.ConditionOrigin.EXIT));
            }
        }

        // Add transition (TRSTN) conditions
        for (Condition condition : mTransition.getTransitionConditions()) {
            allWaits.add(new ConditionWait(condition, ConditionWaiter.ConditionOrigin.TRANSITION));
        }

        return allWaits;
    }

    private Map<Condition, ElementFactory> createFactories() {
        Map<Condition, ElementFactory> allConditionsGuardingFactories = new HashMap<>();

        for (ConditionalState conditionalState : mTransition.getEnteredStates()) {
            final Elements destinationElements = conditionalState.getElements();
            allConditionsGuardingFactories.putAll(destinationElements.getElementFactories());
        }

        return allConditionsGuardingFactories;
    }

    /**
     * Processes destination elements to get a list of waits for their enter conditions, also called
     * when processing new elements from ElementFactory.
     *
     * @param elements The elements to process (i.e. create ConditionWaits for).
     * @return the created {@link ConditionWait}s.
     */
    private List<ConditionWait> createEnterConditionWaits(Elements elements) {
        final List<ConditionWait> newWaits = new ArrayList<>();
        for (Element<?> element : elements.getElements()) {
            @Nullable Condition enterCondition = element.getEnterCondition();
            if (enterCondition != null) {
                newWaits.add(
                        new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
            }
        }

        // Add extra ENTER Conditions.
        for (Condition enterCondition : elements.getOtherEnterConditions()) {
            newWaits.add(new ConditionWait(enterCondition, ConditionWaiter.ConditionOrigin.ENTER));
        }

        return newWaits;
    }

    private boolean processWaits(boolean startMonitoringNewWaits) {
        boolean anyCriteriaMissing = false;
        Set<String> newElementIds = new HashSet<>();

        // We process waits in batches because if a wait that guards a factory is
        // fulfilled, the new elements fabricated by the factory create new
        // waits that also need to be processed. nextBatch starts with the
        // original list of waits |mWaits| and at the end of the loop, nextBatch
        // contains any new waits created while processing the current batch.
        // We stop when no new Waits are created (i.e. nextBatch is empty).
        List<ConditionWait> nextBatch = mWaits;
        mWaits = new ArrayList<>();
        while (!nextBatch.isEmpty()) {
            List<ElementFactory> newFactories = new ArrayList<>();
            for (ConditionWait wait : nextBatch) {
                boolean stillNeedsWait = wait.update();
                anyCriteriaMissing |= stillNeedsWait;
                ElementFactory generator = mConditionsGuardingFactories.get(wait.mCondition);
                if (!stillNeedsWait && generator != null) {
                    // Remove from the map so that next time we check this wait
                    // we dont rerun the factory.
                    mConditionsGuardingFactories.remove(wait.mCondition);
                    newFactories.add(generator);
                }
            }

            mWaits.addAll(nextBatch);

            Elements newElements = fabricateElements(newFactories);
            nextBatch = createEnterConditionWaits(newElements);

            for (ConditionWait wait : nextBatch) {
                wait.markAsDelayedWait();
                wait.ensureTimerStarted();
                // We do not want to start monitoring conditions (even newly
                // created ones) during the first update cycle (aka preCheck)
                // since we already do that after.
                if (startMonitoringNewWaits) {
                    wait.getCondition().onStartMonitoring();
                }
            }

            mConditionsGuardingFactories.putAll(newElements.getElementFactories());
            newElementIds.addAll(newElements.getElementIds());
        }

        for (String elementId : newElementIds) {
            // Check if an exit condition matching the newly added element
            // exists and remove it since it is now a shared element.
            ConditionWait removedExitWait = mExitWaitsByElementId.remove(elementId);
            if (removedExitWait != null) {
                mWaits.remove(removedExitWait);
            }
        }

        return anyCriteriaMissing;
    }

    private Elements fabricateElements(List<ElementFactory> factories) {
        Elements newElements = new Elements();
        for (ElementFactory factory : factories) {
            newElements.addAll(factory.processDelayedDeclarations());
        }
        return newElements;
    }

    private static String createWaitConditionsSummary(
            List<ConditionWait> conditionStatuses, boolean generateMainMessage) {
        String firstUnfulfilledConditionString = null;
        int unfulfilledConditionCount = 0;
        StringBuilder detailsString = new StringBuilder();
        int i = 1;
        for (ConditionWait conditionStatus : conditionStatuses) {
            String conditionDescription = conditionStatus.mCondition.getDescription();

            String indexString = "[" + i + "]";

            String marker = "  ";
            String originString = "";
            switch (conditionStatus.mOrigin) {
                case ConditionOrigin.ENTER:
                    if (conditionStatus.isInitialWait()) {
                        originString = "[ENTER ]";
                    } else {
                        originString = "[+ENTER]";
                    }
                    break;
                case ConditionOrigin.EXIT:
                    originString = "[EXIT  ]";
                    break;
                case ConditionOrigin.TRANSITION:
                    originString = "[TRSTN ]";
                    break;
            }

            String verdictString;
            if (conditionStatus.isFulfilled()) {
                if (conditionStatus.getStatusStore().anyErrorsReported()) {
                    verdictString = "[OK* ]";
                } else {
                    verdictString = "[OK  ]";
                }
            } else {
                if (conditionStatus.getStatusStore().anyErrorsReported()) {
                    verdictString = "[ERR*]";
                } else {
                    verdictString = "[FAIL]";
                }
                marker = "->";
                if (firstUnfulfilledConditionString == null) {
                    firstUnfulfilledConditionString = indexString + " " + conditionDescription;
                }
                unfulfilledConditionCount++;
            }

            StringBuilder historyString = new StringBuilder();
            if (conditionStatus.getStatusStore().shouldPrintRegions()) {
                List<StatusRegion> statusRegions =
                        conditionStatus.getStatusStore().getStatusRegions();
                for (StatusRegion r : statusRegions) {
                    historyString.append("\n        ");
                    historyString.append(r.getLogString(conditionStatus.mTimeStarted));
                }
            }

            String fulfilledString;
            if (conditionStatus.isFulfilled()) {
                Pair<Long, Long> timeToFulfill = conditionStatus.getTimeToFulfill();
                fulfilledString =
                        String.format(
                                "{fulfilled after %d~%d ms}",
                                timeToFulfill.first, timeToFulfill.second);
            } else {
                fulfilledString =
                        String.format(
                                "{unfulfilled after %d ms}", conditionStatus.getTimeUnfulfilled());
            }

            detailsString
                    .append(marker)
                    .append("  ")
                    .append(indexString)
                    .append(" ")
                    .append(originString)
                    .append(" ")
                    .append(verdictString)
                    .append(" ")
                    .append(conditionDescription)
                    .append(" ")
                    .append(fulfilledString);
            if (historyString.length() > 0) {
                detailsString.append(historyString);
            }
            detailsString.append('\n');
            i++;
        }

        if (generateMainMessage) {
            if (unfulfilledConditionCount == 0) {
                return String.format("all Conditions fulfilled:\n%s", detailsString);
            } else if (unfulfilledConditionCount == 1) {
                return String.format(
                        "missing 1 Condition :%s\n%s",
                        firstUnfulfilledConditionString, detailsString);
            } else {
                return String.format(
                        "missing %d Conditions: %s (+%d more)\n%s",
                        unfulfilledConditionCount,
                        firstUnfulfilledConditionString,
                        unfulfilledConditionCount - 1,
                        detailsString);
            }
        } else {
            return detailsString.toString();
        }
    }

    /** The origin of a {@link Condition} (enter, exit, transition). */
    @IntDef({
        ConditionWaiter.ConditionOrigin.ENTER,
        ConditionWaiter.ConditionOrigin.EXIT,
        ConditionWaiter.ConditionOrigin.TRANSITION
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface ConditionOrigin {
        int ENTER = 0;
        int EXIT = 1;
        int TRANSITION = 2;
    }

    private class CheckConditionsOnce implements Runnable {
        @Override
        public void run() {
            boolean anyCriteriaMissing =
                    ConditionWaiter.this.processWaits(/* startMonitoringNewWaits= */ true);

            if (anyCriteriaMissing) {
                throw new CriteriaNotSatisfiedException(
                        "Did not complete "
                                + mTransition.toDebugString()
                                + ", "
                                + createWaitConditionsSummary(
                                        mWaits, /* generateMainMessage= */ true));
            } else {
                Log.i(
                        TAG,
                        "%s: Conditions fulfilled:\n%s",
                        mTransition.toDebugString(),
                        createWaitConditionsSummary(mWaits, /* generateMainMessage= */ false));
            }
        }
    }
}
