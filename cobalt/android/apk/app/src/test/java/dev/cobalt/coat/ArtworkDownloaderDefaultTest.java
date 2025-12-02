package dev.cobalt.coat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
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

  private ArtworkLoader mockLoader;
  private ArtworkDownloaderDefault downloader;

  @Before
  public void setUp() {
    mockLoader = mock(ArtworkLoader.class);
    downloader = new ArtworkDownloaderDefault();
  }

  @Test
  public void testDownloadArtwork_CallsLoader() {
      // This test verifies that even if the download fails (which is expected here as we
      // aren't mocking the network layer fully), the downloader still reports back to the loader.

      String url = "http://example.com/image.png";
      downloader.downloadArtwork(url, mockLoader);

      // Even if download fails, bitmap will be null, but it should still be processed.
      verify(mockLoader).consumeBitmapAndCropTo16x9(any());
      verify(mockLoader).onDownloadFinished(any(Pair.class));
  }
}
