package dev.cobalt.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import dev.cobalt.shell.StartupGuard;
import java.time.Duration;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

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
    for (int milestone = 1; milestone <= 37; milestone++) {
      mStartupGuard.setStartupMilestone(milestone);
      assertEquals(
          "Milestone " + milestone + " should be recorded to UMA",
          1,
          RecordHistogram.getHistogramValueCountForTesting(
              StartupGuard.METRIC_MILESTONE_REACHED, milestone));
      assertEquals(
          "Milestone duration for "
              + StartupGuard.MILESTONE_NAMES[milestone]
              + " should be recorded",
          1,
          RecordHistogram.getHistogramTotalCountForTesting(
              StartupGuard.METRIC_MILESTONE_DURATION_PREFIX
                  + StartupGuard.MILESTONE_NAMES[milestone]));
    }
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
            StartupGuard.METRIC_MILESTONE_DURATION_PREFIX
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
            StartupGuard.METRIC_MILESTONE_DURATION_PREFIX
                + StartupGuard.MILESTONE_NAMES[StartupGuard.STARBOARD_BRIDGE_NATIVE_INIT]));
  }

  @Test
  public void crashRunnable_executesPreCrashHook_whenExecuted() {
    boolean[] hookExecuted = new boolean[] {false};
    mStartupGuard.setPreCrashHook(() -> hookExecuted[0] = true);

    try {
      mStartupGuard.getCrashRunnable().run();
    } catch (RuntimeException e) {
      // Expected
    }

    assertTrue("preCrashHook should have executed prior to crash", hookExecuted[0]);
  }

  @Test
  public void crashRunnable_hookThrows_stillCrashes() {
    mStartupGuard.setPreCrashHook(
        () -> {
          throw new RuntimeException("Simulated failure in preCrashHook");
        });

    boolean crashed = false;
    try {
      mStartupGuard.getCrashRunnable().run();
    } catch (RuntimeException e) {
      if (e.getMessage() != null && e.getMessage().contains("crash triggered by StartupGuard")) {
        crashed = true;
      }
    }

    assertTrue("StartupGuard must still crash even if preCrashHook throws", crashed);
  }
}
