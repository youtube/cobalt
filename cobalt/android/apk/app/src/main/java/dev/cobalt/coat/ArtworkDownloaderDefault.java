// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

import static dev.cobalt.util.Log.TAG;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.util.Pair;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Default implementation of ArtworkDownloader using java.net.HttpURLConnection to fetch artwork.
 */
public class ArtworkDownloaderDefault implements ArtworkDownloader {
  @VisibleForTesting static final int CONNECT_TIMEOUT_MS = 10_000;
  @VisibleForTesting static final int READ_TIMEOUT_MS = 10_000;
  @VisibleForTesting static final int MAX_ARTWORK_BYTES = 8 * 1024 * 1024;

  public ArtworkDownloaderDefault() {}

  @VisibleForTesting
  static byte[] readAllBytesBounded(InputStream is, int maxBytes) throws IOException {
    ByteArrayOutputStream buffer = new ByteArrayOutputStream();
    byte[] chunk = new byte[8192];
    int total = 0;
    int n;
    while ((n = is.read(chunk)) != -1) {
      total += n;
      if (total > maxBytes) {
        throw new IOException("Artwork response exceeds max size: " + maxBytes);
      }
      buffer.write(chunk, 0, n);
    }
    return buffer.toByteArray();
  }

  @VisibleForTesting
  static int calculateInSampleSize(BitmapFactory.Options options, int reqW, int reqH) {
    int height = options.outHeight;
    int width = options.outWidth;
    int inSampleSize = 1;
    if (height > reqH || width > reqW) {
      int halfHeight = height / 2;
      int halfWidth = width / 2;
      while ((halfHeight / inSampleSize) >= reqH && (halfWidth / inSampleSize) >= reqW) {
        inSampleSize *= 2;
      }
    }
    return inSampleSize;
  }

  @Override
  public void downloadArtwork(String url, ArtworkLoader artworkLoader) {
    Bitmap bitmap = null;
    HttpURLConnection conn = null;
    InputStream is = null;
    try {
      boolean optimize = artworkLoader.isMediaSessionOptimizationEnabled();
      conn = (HttpURLConnection) new URL(url).openConnection();
      if (optimize) {
        conn.setConnectTimeout(CONNECT_TIMEOUT_MS);
        conn.setReadTimeout(READ_TIMEOUT_MS);
      }
      is = conn.getInputStream();
      if (optimize) {
        byte[] data = readAllBytesBounded(is, MAX_ARTWORK_BYTES);
        BitmapFactory.Options opts = new BitmapFactory.Options();
        opts.inJustDecodeBounds = true;
        BitmapFactory.decodeByteArray(data, 0, data.length, opts);
        opts.inSampleSize =
            calculateInSampleSize(
                opts,
                ArtworkLoader.OPTIMIZED_MAX_ARTWORK_WIDTH,
                ArtworkLoader.OPTIMIZED_MAX_ARTWORK_HEIGHT);
        opts.inJustDecodeBounds = false;
        bitmap = BitmapFactory.decodeByteArray(data, 0, data.length, opts);
      } else {
        bitmap = BitmapFactory.decodeStream(is);
      }
    } catch (IOException e) {
      Log.e(TAG, "Could not download artwork %s", url, e);
    } finally {
      try {
        if (conn != null) {
          conn.disconnect();
          conn = null;
        }
        if (is != null) {
          is.close();
          is = null;
        }
      } catch (Exception e) {
        Log.e(TAG, "Error closing connection for artwork", e);
      }
    }

    bitmap = artworkLoader.cropTo16x9(bitmap);
    Log.i(TAG, "Artwork downloaded %s.", url);
    artworkLoader.onDownloadFinished(Pair.create(url, bitmap));
  }
}
