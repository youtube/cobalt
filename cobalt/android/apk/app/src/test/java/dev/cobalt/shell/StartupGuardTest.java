// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.shell;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Handler;
import java.lang.reflect.Field;
import java.time.Duration;
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
    private Handler internalHandler;
    private Runnable internalRunnable;

    @Before
    public void setUp() throws Exception {
        startupGuard = StartupGuard.getInstance();

        // Use reflection to get access to the private Handler and Runnable
        // This allows us to verify the internal state without changing the source code.
        internalHandler = getPrivateField(startupGuard, "handler");
        internalRunnable = getPrivateField(startupGuard, "crashRunnable");

        // Ensure a clean slate before each test
        startupGuard.disarm();
    }

    @After
    public void tearDown() {
        // clean up after tests to prevent static state leaking
        startupGuard.disarm();
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
        assertTrue("Handler should have the crash runnable pending",
                internalHandler.hasCallbacks(internalRunnable));
    }

    @Test
    public void scheduleCrash_usesCorrectDelay() {
        // Act
        startupGuard.scheduleCrash(10);

        // Robolectric verification: Check the next task on the Looper
        ShadowLooper shadowLooper = ShadowLooper.shadowMainLooper();
        Duration nextTaskDelay = shadowLooper.getNextScheduledTaskTime();

        // Note: ShadowLooper timing can be tricky; simpler check is looking at the queue
        // But simply asserting the callback exists (previous test) is usually sufficient.
        // A more strict check is advancing time:

        // Advance time by 9.9 seconds -> Should NOT have run yet (still in queue)
        shadowLooper.idleFor(Duration.ofSeconds(9).plusMillis(900));
        assertTrue(internalHandler.hasCallbacks(internalRunnable));
    }

    @Test
    public void disarm_removesRunnableFromHandler() {
        // Arrange
        startupGuard.scheduleCrash(5);
        assertTrue(internalHandler.hasCallbacks(internalRunnable));

        // Act
        startupGuard.disarm();

        // Assert
        assertFalse("Handler should NOT have the crash runnable after disarm",
                internalHandler.hasCallbacks(internalRunnable));
    }

    @Test(expected = RuntimeException.class)
    public void crashRunnable_throwsRuntimeException_whenExecuted() {
        // Act
        // We run the runnable directly to verify it actually throws the exception
        // intended to crash the app.
        internalRunnable.run();
    }

    @Test(expected = RuntimeException.class)
    public void scheduledCrash_actuallyCrashes_whenTimeElapses() {
        // Arrange
        startupGuard.scheduleCrash(1);

        // Act
        // Fast forward the Main Looper by 2 seconds
        ShadowLooper.idleMainLooper(2000, java.util.concurrent.TimeUnit.MILLISECONDS);

        // Assert
        // The test expects a RuntimeException (defined in @Test annotation)
    }

    // --- Helper for Reflection ---
    @SuppressWarnings("unchecked")
    private <T> T getPrivateField(Object target, String fieldName) throws Exception {
        Field field = target.getClass().getDeclaredField(fieldName);
        field.setAccessible(true);
        return (T) field.get(target);
    }
}
