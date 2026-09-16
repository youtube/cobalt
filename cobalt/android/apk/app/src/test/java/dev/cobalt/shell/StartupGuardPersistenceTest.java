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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import androidx.test.core.app.ApplicationProvider;
import java.io.File;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class StartupGuardPersistenceTest {
  private Context mContext;
  private File mStateFile;
  private File mPrevStateFile;

  @Before
  public void setUp() {
    mContext = ApplicationProvider.getApplicationContext();
    mStateFile = new File(mContext.getFilesDir(), StartupGuard.STARTUP_STATE_FILE_NAME);
    mPrevStateFile =
        new File(mContext.getFilesDir(), StartupGuard.STARTUP_STATE_PREVIOUS_FILE_NAME);

    // Clean up previous files if any
    mStateFile.delete();
    mPrevStateFile.delete();

    StartupGuard.getInstance().resetForTesting();
  }

  @Test
  public void testInitializeRenamesPreviousFile() throws Exception {
    mStateFile.createNewFile();

    StartupGuard guard = StartupGuard.getInstance();
    guard.initializePersistenceInternal(mContext, mContext.getFilesDir());

    // Assert the prior file was moved to _previous
    assertTrue(mPrevStateFile.exists());
    assertTrue(mStateFile.exists()); // The new session file
  }

  @Test
  public void testSetStartupMilestoneWritesToBuffer() throws Exception {
    StartupGuard guard = StartupGuard.getInstance();
    guard.initializePersistenceInternal(mContext, mContext.getFilesDir());

    // Set bits after initialization
    guard.setStartupMilestone(1);
    guard.setStartupMilestone(3);

    assertTrue(mStateFile.exists());

    try (RandomAccessFile raf = new RandomAccessFile(mStateFile, "r")) {
      byte[] data = new byte[8];
      raf.readFully(data);
      ByteBuffer buffer = ByteBuffer.wrap(data);
      buffer.order(ByteOrder.LITTLE_ENDIAN);

      long storedValue = buffer.getLong();
      long expected = (1L << 1) | (1L << 3);
      assertEquals("Milestone bits not properly set in little-endian order", expected, storedValue);
    }
  }

  @Test
  public void testMilestonesPreInitializationAreFlushedToDisk() throws Exception {
    StartupGuard guard = StartupGuard.getInstance();

    // Set a milestone BEFORE initialization (e.g. extremely early Application onCreate)
    guard.setStartupMilestone(2);

    // Now initialize persistence and ensure it doesn't overwrite it with 0
    guard.initializePersistenceInternal(mContext, mContext.getFilesDir());

    try (RandomAccessFile raf = new RandomAccessFile(mStateFile, "r")) {
      byte[] data = new byte[8];
      raf.readFully(data);
      ByteBuffer buffer = ByteBuffer.wrap(data);
      buffer.order(ByteOrder.LITTLE_ENDIAN);

      long storedValue = buffer.getLong();
      long expected = (1L << 2);
      assertEquals(
          "Pre-initialization milestones were lost during buffer init", expected, storedValue);
    }
  }

  @Test
  public void testDisarmDeletesDiskFileOnHealthyExit() throws Exception {
    StartupGuard guard = StartupGuard.getInstance();
    guard.initializePersistenceInternal(mContext, mContext.getFilesDir());
    guard.setStartupMilestone(4);
    guard.scheduleCrash(10);

    // Unarm the watchdog (representing a successful initialization)
    guard.disarm();

    // The state file MUST be deleted on a clean exit, because C++ records these
    // milestones natively during a healthy session. We only want previous.bin generated
    // on a real watchdog hang crash!
    assertTrue("Disk file was not wiped on clean exit", !mStateFile.exists());
  }
}
