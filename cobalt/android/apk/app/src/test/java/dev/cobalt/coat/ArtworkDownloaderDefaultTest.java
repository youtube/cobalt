// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.coat;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.mock;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Pair;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

/** Tests for {@link ArtworkDownloaderDefault}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArtworkDownloaderDefaultTest {

  private static final String TEST_URL = "http://example.com/image.png";
  private static final int TIMEOUT_SECONDS = 1;

  private ArtworkDownloaderDefault mDownloader;

  @Before
  public void setUp() {
    mDownloader = new ArtworkDownloaderDefault();
  }

  // Helper to execute the download, flush the looper, and assert the latch.
  private void executeAndVerify(ArtworkLoader loader, CountDownLatch latch)
      throws InterruptedException {
    loader.mRequestedArtworkUrl = TEST_URL;

    mDownloader.downloadArtwork(TEST_URL, loader);

    // The download runs on the calling thread (in this test context), calls onDownloadFinished,
    // which posts to the main looper. Run the looper to execute the callback.
    ShadowLooper.runUiThreadTasks();

    assertThat(latch.await(TIMEOUT_SECONDS, TimeUnit.SECONDS)).isTrue();
  }

  @Test
  public void testDownloadArtwork_Failure_CallsOnDownloadFinishedWithNull()
      throws InterruptedException {
    final CountDownLatch latch = new CountDownLatch(1);

    ArtworkLoader artworkLoader =
        new ArtworkLoader(mock(ArtworkLoader.Callback.class), mDownloader) {
          @Override
          public void onDownloadFinished(Pair<String, Bitmap> urlBitmapPair) {
            super.onDownloadFinished(urlBitmapPair);
            // In this test, the download is expected to fail, so the bitmap should be null.
            // In this test environment, the download fails because the URL is not accessible,
            // triggering the IOException catch block in ArtworkDownloaderDefault.
            assertThat(urlBitmapPair.second).isNull();
            latch.countDown();
          }
        };

    executeAndVerify(artworkLoader, latch);
  }

  @Test
  public void testDownloadArtwork_TriggersCallback() throws InterruptedException {
    final CountDownLatch latch = new CountDownLatch(1);

    ArtworkLoader artworkLoader =
        new ArtworkLoader(
            new ArtworkLoader.Callback() {
              @Override
              public void onArtworkLoaded(Bitmap bitmap) {
                latch.countDown();
              }
            },
            mDownloader) {
          @Override
          public Bitmap cropTo16x9(Bitmap bitmap) {
            // Return a dummy bitmap to simulate success and trigger the callback,
            // even if the download (simulated to fail here) produces a null bitmap.
            return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
          }
        };

    executeAndVerify(artworkLoader, latch);
  }

  @Test
  public void testCalculateInSampleSize_capsDecodeTowards640x360() {
    BitmapFactory.Options opts = new BitmapFactory.Options();

    opts.outWidth = 640;
    opts.outHeight = 360;
    assertThat(
            ArtworkDownloaderDefault.calculateInSampleSize(
                opts,
                ArtworkLoader.OPTIMIZED_MAX_ARTWORK_WIDTH,
                ArtworkLoader.OPTIMIZED_MAX_ARTWORK_HEIGHT))
        .isEqualTo(1);

    opts.outWidth = 1920;
    opts.outHeight = 1080;
    assertThat(
            ArtworkDownloaderDefault.calculateInSampleSize(
                opts,
                ArtworkLoader.OPTIMIZED_MAX_ARTWORK_WIDTH,
                ArtworkLoader.OPTIMIZED_MAX_ARTWORK_HEIGHT))
        .isEqualTo(2);

    opts.outWidth = 2560;
    opts.outHeight = 1440;
    assertThat(
            ArtworkDownloaderDefault.calculateInSampleSize(
                opts,
                ArtworkLoader.OPTIMIZED_MAX_ARTWORK_WIDTH,
                ArtworkLoader.OPTIMIZED_MAX_ARTWORK_HEIGHT))
        .isEqualTo(4);
  }

  @Test
  public void testReadAllBytesBounded_enforcesMaxBytesLimit() throws Exception {
    byte[] payload = new byte[] {1, 2, 3, 4, 5};
    byte[] read =
        ArtworkDownloaderDefault.readAllBytesBounded(
            new java.io.ByteArrayInputStream(payload), /* maxBytes= */ 5);
    assertThat(read).isEqualTo(payload);

    boolean threw = false;
    try {
      ArtworkDownloaderDefault.readAllBytesBounded(
          new java.io.ByteArrayInputStream(new byte[1024]), /* maxBytes= */ 512);
    } catch (java.io.IOException expected) {
      threw = true;
    }
    assertThat(threw).isTrue();
  }
}
