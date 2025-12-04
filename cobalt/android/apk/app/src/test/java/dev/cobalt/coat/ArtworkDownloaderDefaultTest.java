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
  private ArtworkDownloader mMockArtworkDownloader; // Needed for ArtworkLoader constructor

  @Before
  public void setUp() {
    mDownloader = new ArtworkDownloaderDefault();
    mMockArtworkDownloader = mock(ArtworkDownloader.class);
  }

  @Test
  public void testDownloadArtwork_CallsLoader() throws InterruptedException {
    final CountDownLatch latch = new CountDownLatch(1);

    ArtworkLoader artworkLoader =
        new ArtworkLoader(
            new ArtworkLoader.Callback() {
              @Override
              public void onArtworkLoaded(Bitmap bitmap) {
                // This callback is posted to the main looper by ArtworkLoader.
                // We will manually run the looper to trigger it.
              }
            },
            mMockArtworkDownloader) {
          @Override
          public synchronized void onDownloadFinished(Pair<String, Bitmap> urlBitmapPair) {
            super.onDownloadFinished(urlBitmapPair);
            latch.countDown();
          }
        };

    artworkLoader.mRequestedArtworkUrl = "http://example.com/image.png";

    String url = "http://example.com/image.png";
    mDownloader.downloadArtwork(url, artworkLoader);

    ShadowLooper.runUiThreadTasks();

    assertThat(latch.await(1, TimeUnit.SECONDS)).isTrue();

    // Verify interactions on the real ArtworkLoader (now that it has completed its work).
    // This implicitly verifies consumeBitmapAndCropTo16x9 was called, and that
    // onDownloadFinished was called.
    // Note: We cannot directly verify calls on the 'artworkLoader' instance using Mockito
    // 'verify' if it's a real object. If specific internal calls need verification,
    // ArtworkLoader itself might need to be a spy or provide testable hooks.
    // For this test, verifying the latch countdown is sufficient proof of the flow.
  }
}
