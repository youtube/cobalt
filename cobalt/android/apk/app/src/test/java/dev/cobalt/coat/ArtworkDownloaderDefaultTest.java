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

  private ArtworkDownloaderDefault mDownloader;

  @Before
  public void setUp() {
    mDownloader = new ArtworkDownloaderDefault();
  }

  @Test
  public void testDownloadArtwork_CallsOnDownloadFinished() throws InterruptedException {
    final CountDownLatch latch = new CountDownLatch(1);

    ArtworkLoader artworkLoader =
        new ArtworkLoader(
            mock(ArtworkLoader.Callback.class),
            mock(ArtworkDownloader.class)) {
          @Override
          public synchronized void onDownloadFinished(Pair<String, Bitmap> urlBitmapPair) {
            super.onDownloadFinished(urlBitmapPair);
            latch.countDown();
          }
        };

    artworkLoader.mRequestedArtworkUrl = "http://example.com/image.png";

    String url = "http://example.com/image.png";
    mDownloader.downloadArtwork(url, artworkLoader);

    assertThat(latch.await(1, TimeUnit.SECONDS)).isTrue();
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
            mock(ArtworkDownloader.class)) {
          @Override
          public Bitmap consumeBitmapAndCropTo16x9(Bitmap bitmap) {
            // Return a dummy bitmap to simulate success and trigger the callback,
            // even if the download (simulated to fail here) produces a null bitmap.
            return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
          }
        };

    artworkLoader.mRequestedArtworkUrl = "http://example.com/image.png";

    String url = "http://example.com/image.png";
    mDownloader.downloadArtwork(url, artworkLoader);

    // The download runs on the calling thread (in this test context), calls onDownloadFinished,
    // which posts to the main looper. Run the looper to execute the callback.
    ShadowLooper.runUiThreadTasks();

    assertThat(latch.await(1, TimeUnit.SECONDS)).isTrue();
  }
}
