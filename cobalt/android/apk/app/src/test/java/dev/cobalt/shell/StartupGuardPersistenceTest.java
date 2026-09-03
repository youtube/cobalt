package dev.cobalt.shell;

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
  private Context context;

  @Before
  public void setUp() {
    context = ApplicationProvider.getApplicationContext();
    // Clean up previous files if any
    new File(context.getFilesDir(), "java_startup_state.bin").delete();
    new File(context.getFilesDir(), "java_startup_state_previous.bin").delete();
  }

  @Test
  public void testInitializeRenamesPreviousFile() throws Exception {
    File priorFile = new File(context.getFilesDir(), "java_startup_state.bin");
    priorFile.createNewFile();

    StartupGuard guard = StartupGuard.getInstance();
    guard.initializePersistence(context);

    // Assert the prior file was moved to _previous
    File prevFile = new File(context.getFilesDir(), "java_startup_state_previous.bin");
    assertTrue(prevFile.exists());
    assertTrue(new File(context.getFilesDir(), "java_startup_state.bin").exists());
  }

  @Test
  public void testSetStartupMilestoneWritesToBuffer() throws Exception {
    StartupGuard guard = StartupGuard.getInstance();
    guard.initializePersistence(context);

    // Flip bits 1 and 3 (0x0A)
    guard.setStartupMilestone(1);
    guard.setStartupMilestone(3);

    File currentStateFile = new File(context.getFilesDir(), "java_startup_state.bin");
    assertTrue(currentStateFile.exists());

    try (RandomAccessFile raf = new RandomAccessFile(currentStateFile, "r")) {
      byte[] data = new byte[8];
      raf.readFully(data);
      ByteBuffer buffer = ByteBuffer.wrap(data);
      buffer.order(ByteOrder.LITTLE_ENDIAN);

      long storedValue = buffer.getLong();
      // bit 1 (2) + bit 3 (8) = 10 (0x0A)
      long expected = (1L << 1) | (1L << 3);

      // Wait, there might be other bits flipped if it's singleton across tests.
      // But we just verify the exact bit mask has the bits we set.
      assertTrue(
          "Milestone bits not properly set in little-endian order",
          (storedValue & expected) == expected);
    }
  }
}
