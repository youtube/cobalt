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

package dev.cobalt.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.os.HandlerThread;
import android.os.Process;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowMediaCodec;
import android.media.MediaCodec;
import java.io.IOException;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

/** Unit Tests for MediaCodecBridge class. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MediaCodecBridgeTest {

  @Before
  public void setUp() {
    ShadowMediaCodec.clearCodecs();
    // Register a mock decoder for testing.
    ShadowMediaCodec.addDecoder(
        "mock-h264-decoder",
        new ShadowMediaCodec.CodecConfig(
            /* inputBufferSize= */ 1024,
            /* outputBufferSize= */ 1024,
            (input, output) -> output.put(input)));
  }

  @Test
  public void testConstructor_SuccessfulCreationSpawnsThread() {
    MediaCodecBridge bridge = null;
    try {
      bridge =
          new MediaCodecBridge(
              12345L,
              "mock-h264-decoder",
              TunnelModeAudioSessionId.NONE,
              /* enableFrameRendererListener= */ false,
              /* enableIgnoreCallbacksDuringFlushing= */ false);

      HandlerThread thread = bridge.getMediaCodecThreadForTesting();
      assertNotNull("HandlerThread should be non-null", thread);
      assertTrue("HandlerThread should be started and alive", thread.isAlive());
      assertEquals(
          "Thread priority should be THREAD_PRIORITY_VIDEO",
          Process.THREAD_PRIORITY_VIDEO,
          Process.getThreadPriority(thread.getThreadId()));
    } finally {
      if (bridge != null) {
        HandlerThread thread = bridge.getMediaCodecThreadForTesting();
        bridge.release();
        if (thread != null) {
          try {
            thread.join(1000);
          } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
          }
          assertFalse(
              "Thread should be stopped after release",
              thread.isAlive());
        }
      }
    }
  }

  @Test
  public void testConstructor_InvalidDecoderNameThrowsIllegalArgumentException() {
    try {
      new MediaCodecBridge(
          12345L,
          null, // Null decoder name
          TunnelModeAudioSessionId.NONE,
          /* enableFrameRendererListener= */ false,
          /* enableIgnoreCallbacksDuringFlushing= */ false);
      fail("Should have thrown IllegalArgumentException for null decoderName");
    } catch (IllegalArgumentException e) {
      assertEquals("Decoder name cannot be null or empty", e.getMessage());
    }

    try {
      new MediaCodecBridge(
          12345L,
          "", // Empty decoder name
          TunnelModeAudioSessionId.NONE,
          /* enableFrameRendererListener= */ false,
          /* enableIgnoreCallbacksDuringFlushing= */ false);
      fail("Should have thrown IllegalArgumentException for empty decoderName");
    } catch (IllegalArgumentException e) {
      assertEquals("Decoder name cannot be null or empty", e.getMessage());
    }
  }

  @Test
  @Config(shadows = {ShadowFailingMediaCodec.class})
  public void testConstructor_FailureToCreateCodecCleansUpThread() {
    try {
      new MediaCodecBridge(
          12345L,
          "unregistered-decoder-name",
          TunnelModeAudioSessionId.NONE,
          /* enableFrameRendererListener= */ false,
          /* enableIgnoreCallbacksDuringFlushing= */ false);
      fail("Should have thrown RuntimeException due to codec creation failure");
    } catch (RuntimeException e) {
      assertTrue(
          "Exception should contain cause of failure",
          e.getMessage().contains("Failed to create MediaCodec on HandlerThread"));
    }
    // Join the thread if found to wait for its asynchronous shutdown
    for (Thread t : Thread.getAllStackTraces().keySet()) {
      if (t.getName().startsWith("MediaCodecBridgeThread_unregistered-decoder-name")) {
        try {
          t.join(1000);
        } catch (InterruptedException e) {
          Thread.currentThread().interrupt();
        }
      }
    }
    // Now verify no alive thread remains
    boolean foundAliveThread = false;
    for (Thread t : Thread.getAllStackTraces().keySet()) {
      if (t.getName().startsWith("MediaCodecBridgeThread_unregistered-decoder-name")) {
        if (t.isAlive()) {
          foundAliveThread = true;
        }
      }
    }
    assertFalse(
        "Spun up thread should have been stopped during failure cleanup",
        foundAliveThread);
  }

  @Implements(MediaCodec.class)
  public static class ShadowFailingMediaCodec {
    @Implementation
    public static MediaCodec createByCodecName(String name) throws IOException {
      throw new IOException("Simulated codec creation failure for: " + name);
    }
  }
}
