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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import android.util.Pair;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link ArtworkDownloaderDefault}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArtworkDownloaderDefaultTest {

  private ArtworkLoader mMockLoader;
  private ArtworkDownloaderDefault mDownloader;

  @Before
  public void setUp() {
    mMockLoader = mock(ArtworkLoader.class);
    mDownloader = new ArtworkDownloaderDefault();
  }

  @Test
  public void testDownloadArtwork_CallsLoader() {
      // This test verifies that even if the download fails (which is expected here as we
      // aren't mocking the network layer fully), the downloader still reports back to the loader.

      String url = "http://example.com/image.png";
      mDownloader.downloadArtwork(url, mMockLoader);

      // Even if download fails, bitmap will be null, but it should still be processed.
      verify(mMockLoader, timeout(1000)).consumeBitmapAndCropTo16x9(any());
      verify(mMockLoader, timeout(1000)).onDownloadFinished(any(Pair.class));
  }
}
