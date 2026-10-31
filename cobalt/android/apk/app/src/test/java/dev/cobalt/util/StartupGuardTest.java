package dev.cobalt.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.SystemClock;
import dev.cobalt.shell.StartupGuard;
import java.time.Duration;
import java.util.concurrent.TimeUnit;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;

/** Tests for the StartupGuard. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StartupGuardTest {

  private StartupGuard mStartupGuard;

  @Before
  public void setUp() throws Exception {
    UmaRecorderHolder.resetForTesting();
    mStartupGuard = StartupGuard.getInstance();

    // Ensure a clean slate before each test
    mStartupGuard.resetForTesting();
  }

  @After
  public void tearDown() {
    // clean up after tests to prevent static state leaking
    mStartupGuard.resetForTesting();
    UmaRecorderHolder.resetForTesting();
  }

  @Test
  public void getInstance_returnsSameInstance() {
    StartupGuard instance1 = StartupGuard.getInstance();
    StartupGuard instance2 = StartupGuard.getInstance();
    assertTrue("Singleton should return the same instance", instance1 == instance2);
  }

  @Test
  public void scheduleCrash_postsRunnableToHandler() {
    // Act
    long delaySeconds = 5;
    mStartupGuard.scheduleCrash(delaySeconds);

    // Assert
    assertTrue("Handler should have the crash runnable pending", mStartupGuard.isArmed());
  }

  @Test
  public void scheduleCrash_usesCorrectDelay() {
    // Act
    mStartupGuard.scheduleCrash(10);

    // Robolectric verification: Check the next task on the Looper
    ShadowLooper shadowLooper = ShadowLooper.shadowMainLooper();
    Duration nextTaskDelay = shadowLooper.getNextScheduledTaskTime();

    // Note: ShadowLooper timing can be tricky; simpler check is looking at the queue
    // But simply asserting the callback exists (previous test) is usually sufficient.
    // A more strict check is advancing time:

    // Advance time by 9.9 seconds -> Should NOT have run yet (still in queue)
    shadowLooper.idleFor(Duration.ofSeconds(9).plusMillis(900));
    assertTrue(mStartupGuard.isArmed());
  }

  @Test
  public void disarm_removesRunnableFromHandler() {
    // Arrange
    mStartupGuard.scheduleCrash(5);
    assertTrue(mStartupGuard.isArmed());

    // Act
    mStartupGuard.disarm();

    // Assert
    assertFalse("Handler should NOT have the crash runnable after disarm", mStartupGuard.isArmed());
  }

  @Test(expected = RuntimeException.class)
  public void crashRunnable_throwsRuntimeException_whenExecuted() {
    // Act
    // We run the runnable directly to verify it actually throws the exception
    // intended to crash the app.
    mStartupGuard.getCrashRunnable().run();
  }

  @Test(expected = RuntimeException.class)
  public void scheduledCrash_actuallyCrashes_whenTimeElapses() {
    // Arrange
    mStartupGuard.scheduleCrash(1);

    // Act
    // Fast forward the Main Looper by 2 seconds
    ShadowLooper.idleMainLooper(2000, java.util.concurrent.TimeUnit.MILLISECONDS);

    // Assert
    // The test expects a RuntimeException (defined in @Test annotation)
  }

  @Test
  public void setStartupMilestone_recordsUmaForMilestones1To37() {
    // ActivityOnDestroy ends startup timing, so reach it last.
    for (int milestone = 1; milestone <= 37; milestone++) {
      if (milestone != StartupGuard.ACTIVITY_ON_DESTROY) {
        assertMilestoneRecordedToUma(milestone);
      }
    }
    assertMilestoneRecordedToUma(StartupGuard.ACTIVITY_ON_DESTROY);
  }

  private void assertMilestoneRecordedToUma(int milestone) {
    mStartupGuard.setStartupMilestone(milestone);
    assertEquals(
        "Milestone " + milestone + " should be recorded to UMA",
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED, milestone));
    assertEquals(
        "Time to milestone for " + StartupGuard.MILESTONE_NAMES[milestone] + " should be recorded",
        1,
        RecordHistogram.getHistogramTotalCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[milestone]));
  }

  @Test
  public void setStartupMilestone_recordsTimeSinceStartupOriginNotSincePreviousMilestone() {
    mStartupGuard.setStartupOriginForTesting(SystemClock.elapsedRealtime() - 1500);

    mStartupGuard.setStartupMilestone(StartupGuard.PRE_LIBRARY_LOADER_INIT);
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.PRE_LIBRARY_LOADER_INIT],
            1500));

    ShadowSystemClock.advanceBy(250, TimeUnit.MILLISECONDS);
    mStartupGuard.setStartupMilestone(StartupGuard.POST_LIBRARY_LOADER_INIT);
    // Measured from the shared origin (1750 ms), not from the previous milestone (250 ms).
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.POST_LIBRARY_LOADER_INIT],
            1750));
    assertEquals(
        0,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.POST_LIBRARY_LOADER_INIT],
            250));
  }

  @Test
  public void setStartupMilestone_recordsMilestoneTimestamp() {
    assertEquals(0L, mStartupGuard.getMilestoneTimestampMs(StartupGuard.DID_FINISH_NAVIGATION));

    long before = SystemClock.elapsedRealtime();
    mStartupGuard.setStartupMilestone(StartupGuard.DID_FINISH_NAVIGATION);
    assertEquals(before, mStartupGuard.getMilestoneTimestampMs(StartupGuard.DID_FINISH_NAVIGATION));

    // Out-of-range milestones never have a timestamp.
    assertEquals(0L, mStartupGuard.getMilestoneTimestampMs(-1));
    assertEquals(0L, mStartupGuard.getMilestoneTimestampMs(64));
  }

  @Test
  public void setStartupMilestone_stopsTimingAfterActivityDestroyed() {
    mStartupGuard.setStartupMilestone(StartupGuard.ACTIVITY_ON_DESTROY);
    assertEquals(
        "ActivityOnDestroy itself should be timed",
        1,
        RecordHistogram.getHistogramTotalCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.ACTIVITY_ON_DESTROY]));

    mStartupGuard.setStartupMilestone(StartupGuard.PLATFORM_ERROR_RAISED);
    assertEquals(
        "Milestones first reached after the Activity is destroyed are not startup timings",
        0,
        RecordHistogram.getHistogramTotalCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.PLATFORM_ERROR_RAISED]));
    assertEquals(
        "MilestoneReached is still recorded",
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED, StartupGuard.PLATFORM_ERROR_RAISED));
  }

  @Test
  public void setStartupMilestone_stopsTimingWhenSecondActivityIsCreated() {
    mStartupGuard.setStartupMilestone(StartupGuard.JAVA_ACTIVITY_CREATED);
    // A second Activity created in the same process reports JAVA_ACTIVITY_CREATED again.
    mStartupGuard.setStartupMilestone(StartupGuard.JAVA_ACTIVITY_CREATED);

    mStartupGuard.setStartupMilestone(StartupGuard.ACTIVITY_ON_RESUME);
    assertEquals(
        "Milestones first reached by a later Activity are not startup timings",
        0,
        RecordHistogram.getHistogramTotalCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.ACTIVITY_ON_RESUME]));
    assertEquals(
        "MilestoneReached is still recorded",
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED, StartupGuard.ACTIVITY_ON_RESUME));
  }

  @Test
  public void setStartupMilestone_ignoresMilestonesBelow1AndAbove37ForUma() {
    // Milestone 0 should not be logged to StartupGuard milestone UMA
    mStartupGuard.setStartupMilestone(0);
    assertEquals(
        "Milestone 0 should not be recorded to UMA",
        0,
        RecordHistogram.getHistogramValueCountForTesting(StartupGuard.METRIC_MILESTONE_REACHED, 0));

    // Milestones > 37 should not be logged to StartupGuard milestone UMA
    mStartupGuard.setStartupMilestone(38);
    assertEquals(
        "Milestone 38 should not be recorded to UMA",
        0,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED, 38));
  }

  @Test
  public void setStartupMilestone_handlesInvalidMilestonesSafely() {
    // Should not throw or crash on invalid index
    mStartupGuard.setStartupMilestone(-1);
    mStartupGuard.setStartupMilestone(64);
    mStartupGuard.setStartupMilestone(100);

    // Verify that invalid milestones did not corrupt or pre-set valid milestones (e.g. 100 % 64 =
    // 36)
    mStartupGuard.setStartupMilestone(StartupGuard.STARTUP_GUARD_OBSERVER_DID_REDIRECT_NAVIGATION);
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED,
            StartupGuard.STARTUP_GUARD_OBSERVER_DID_REDIRECT_NAVIGATION));
  }

  @Test
  public void setStartupMilestone_isIdempotentForDuplicateCalls() {
    mStartupGuard.setStartupMilestone(StartupGuard.STARBOARD_BRIDGE_NATIVE_INIT);
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED, StartupGuard.STARBOARD_BRIDGE_NATIVE_INIT));
    assertEquals(
        1,
        RecordHistogram.getHistogramTotalCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.STARBOARD_BRIDGE_NATIVE_INIT]));

    // Second call with same milestone should be ignored
    mStartupGuard.setStartupMilestone(StartupGuard.STARBOARD_BRIDGE_NATIVE_INIT);
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED, StartupGuard.STARBOARD_BRIDGE_NATIVE_INIT));
    assertEquals(
        1,
        RecordHistogram.getHistogramTotalCountForTesting(
            StartupGuard.METRIC_TIME_TO_MILESTONE_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.STARBOARD_BRIDGE_NATIVE_INIT]));
  }

  @Test
  public void testGetStartupStatusAndHighestMilestone() {
    assertEquals(0L, mStartupGuard.getStartupStatus());
    assertEquals(0, mStartupGuard.getHighestMilestone());

    mStartupGuard.setStartupMilestone(1);
    assertEquals(1L << 1, mStartupGuard.getStartupStatus());
    assertEquals(1, mStartupGuard.getHighestMilestone());

    mStartupGuard.setStartupMilestone(10);
    assertEquals((1L << 1) | (1L << 10), mStartupGuard.getStartupStatus());
    assertEquals(10, mStartupGuard.getHighestMilestone());

    mStartupGuard.setStartupMilestone(37);
    assertEquals((1L << 1) | (1L << 10) | (1L << 37), mStartupGuard.getStartupStatus());
    assertEquals(37, mStartupGuard.getHighestMilestone());
  }

  @Test
  public void testPreCrashHook_executedBeforeForcedCrash() {
    boolean[] hookExecuted = new boolean[] {false};
    mStartupGuard.setPreCrashHook(() -> hookExecuted[0] = true);

    try {
      mStartupGuard.getCrashRunnable().run();
      fail("Expected RuntimeException to be thrown");
    } catch (RuntimeException expected) {
      // Expected exception
    }
    assertTrue("Pre-crash hook should have executed before exception", hookExecuted[0]);
  }

  @Test
  public void testPreCrashHook_exceptionHandledGracefully() {
    mStartupGuard.setPreCrashHook(
        () -> {
          throw new RuntimeException("Hook failed");
        });

    try {
      mStartupGuard.getCrashRunnable().run();
      fail("Expected RuntimeException to be thrown");
    } catch (RuntimeException expected) {
      assertTrue(expected.getMessage().contains("Application startup may not have succeeded"));
    }
  }
}
