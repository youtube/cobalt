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

  private StartupGuard startupGuard;

  @Before
  public void setUp() throws Exception {
    UmaRecorderHolder.resetForTesting();
    startupGuard = StartupGuard.getInstance();
    startupGuard.resetForTesting();
  }

  @After
  public void tearDown() {
    startupGuard.resetForTesting();
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
    startupGuard.scheduleCrash(delaySeconds);

    // Assert
    assertTrue("Handler should have the crash runnable pending", startupGuard.isArmed());
  }

  @Test
  public void scheduleCrash_usesCorrectDelay() {
    // Act
    startupGuard.scheduleCrash(10);

    // Robolectric verification: Check the next task on the Looper
    ShadowLooper shadowLooper = ShadowLooper.shadowMainLooper();
    Duration nextTaskDelay = shadowLooper.getNextScheduledTaskTime();

    // Advance time by 9.9 seconds -> Should NOT have run yet (still in queue)
    shadowLooper.idleFor(Duration.ofSeconds(9).plusMillis(900));
    assertTrue(startupGuard.isArmed());
  }

  @Test
  public void disarm_removesRunnableFromHandler() {
    // Arrange
    startupGuard.scheduleCrash(5);
    assertTrue(startupGuard.isArmed());

    // Act
    startupGuard.disarm();

    // Assert
    assertFalse("Handler should NOT have the crash runnable after disarm", startupGuard.isArmed());
  }

  @Test(expected = RuntimeException.class)
  public void crashRunnable_throwsRuntimeException_whenExecuted() {
    // Act
    startupGuard.getCrashRunnable().run();
  }

  @Test(expected = RuntimeException.class)
  public void scheduledCrash_actuallyCrashes_whenTimeElapses() {
    // Arrange
    startupGuard.scheduleCrash(1);

    // Act
    ShadowLooper.idleMainLooper(2000, java.util.concurrent.TimeUnit.MILLISECONDS);
  }

  @Test
  public void setStartupMilestone_recordsUmaForMilestones5To37() {
    for (int milestone = 5; milestone <= 37; milestone++) {
      startupGuard.setStartupMilestone(milestone);
      assertEquals(
          "Milestone " + milestone + " should be recorded to UMA",
          1,
          RecordHistogram.getHistogramValueCountForTesting(
              StartupGuard.METRIC_MILESTONE_REACHED, milestone));
      assertEquals(
          "Milestone duration for " + milestone + " should be recorded",
          1,
          RecordHistogram.getHistogramTotalCountForTesting(
              StartupGuard.METRIC_MILESTONE_DURATION_PREFIX + milestone));
    }
  }

  @Test
  public void setStartupMilestone_ignoresMilestonesBelow5AndAbove37ForUma() {
    // Milestones 1 to 4 should not be logged to StartupGuard milestone UMA
    for (int milestone = 1; milestone <= 4; milestone++) {
      startupGuard.setStartupMilestone(milestone);
      assertEquals(
          "Milestone " + milestone + " should not be recorded to UMA",
          0,
          RecordHistogram.getHistogramValueCountForTesting(
              StartupGuard.METRIC_MILESTONE_REACHED, milestone));
    }

    // Milestones > 37 should not be logged to StartupGuard milestone UMA
    startupGuard.setStartupMilestone(38);
    assertEquals(
        "Milestone 38 should not be recorded to UMA",
        0,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED, 38));
  }

  @Test
  public void setStartupMilestone_handlesInvalidMilestonesSafely() {
    // Should not throw or crash on invalid index
    startupGuard.setStartupMilestone(-1);
    startupGuard.setStartupMilestone(64);
    startupGuard.setStartupMilestone(100);

    // Verify that invalid milestones did not corrupt or pre-set valid milestones (e.g. 100 % 64 =
    // 36)
    startupGuard.setStartupMilestone(36);
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(
            StartupGuard.METRIC_MILESTONE_REACHED, 36));
  }

  @Test
  public void setStartupMilestone_isIdempotentForDuplicateCalls() {
    startupGuard.setStartupMilestone(5);
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(StartupGuard.METRIC_MILESTONE_REACHED, 5));
    assertEquals(
        1,
        RecordHistogram.getHistogramTotalCountForTesting(
            StartupGuard.METRIC_MILESTONE_DURATION_PREFIX + 5));
    assertEquals(
        1,
        RecordHistogram.getHistogramTotalCountForTesting(StartupGuard.METRIC_MILESTONE_DURATION));

    // Second call with same milestone should be ignored
    startupGuard.setStartupMilestone(5);
    assertEquals(
        1,
        RecordHistogram.getHistogramValueCountForTesting(StartupGuard.METRIC_MILESTONE_REACHED, 5));
    assertEquals(
        1,
        RecordHistogram.getHistogramTotalCountForTesting(
            StartupGuard.METRIC_MILESTONE_DURATION_PREFIX + 5));
    assertEquals(
        1,
        RecordHistogram.getHistogramTotalCountForTesting(StartupGuard.METRIC_MILESTONE_DURATION));
  }
}
