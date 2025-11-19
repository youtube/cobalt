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
import dev.cobalt.util.Log;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/**
 * Default implementation of ArtworkDownloader using java.net.HttpURLConnection to fetch artwork.
 */
public class ArtworkDownloaderDefault implements ArtworkDownloader {

  public ArtworkDownloaderDefault() {}

  @Override
  public void downloadArtwork(String url, ArtworkLoader artworkLoader) {
    Bitmap bitmap = null;
    HttpURLConnection conn = null;
    InputStream is = null;
    try {
      conn = (HttpURLConnection) new URL(url).openConnection();
      is = conn.getInputStream();
      bitmap = BitmapFactory.decodeStream(is);
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

    bitmap = artworkLoader.consumeBitmapAndCropTo16x9(bitmap);
    Log.i(TAG, "Artwork downloaded %s.", url);
    artworkLoader.onDownloadFinished(Pair.create(url, bitmap));
  }
}
