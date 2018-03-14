// Copyright 2017 Google Inc. All Rights Reserved.
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

import android.util.Log;
import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.URL;

/** Helper class that implements an HTTP POST function used by DRM one-time provisioning. */
public class CobaltHttpHelper {
  private static final int RETRY_SLEEP_MILLIS = 250;
  private static final int MAX_ATTEMPTS = 3;

  /** Exception representing a transient HTTP failure (eg, 500). */
  private static class TransientFailure extends Exception {}

  private static void sleepBeforeRetry() {
    try {
      Thread.sleep(RETRY_SLEEP_MILLIS);
    } catch (InterruptedException ex) {
      // should never happen
    }
  }

  /**
   * Performs an HTTP POST, sending postData to url and returning the response contents on 200.
   *
   * <p>Note that this function retries temporary failures (network, HTTP 500) a few times.
   *
   * <p>Note also that this sets a few DRM-specific headers.
   *
   * @return response contents on success, null for permanent failure or exception.
   */
  public byte[] performDrmHttpPost(String url) {
    for (int attempts = 0; attempts < MAX_ATTEMPTS; attempts++) {
      try {
        return internalPerformHttpPost(url);
      } catch (IOException ex) {
        Log.w(TAG, "performHttpPost IOException: ", ex);
        // continue below
      } catch (TransientFailure ex) {
        // continue below
      } catch (Throwable tr) {
        // All other exceptions are caught because the caller is expected
        // to be a JNI function, where exception handling is inconvenient.
        Log.e(TAG, "performHttpPost exception: ", tr);
        return null;
      }
      sleepBeforeRetry();
    }
    Log.w(TAG, "performHttpPost: Max attempts attempted");
    return null;
  }

  private byte[] internalPerformHttpPost(String url) throws IOException, TransientFailure {
    HttpURLConnection conn = (HttpURLConnection) new URL(url).openConnection();
    try {
      conn.setRequestMethod("POST");
      conn.setDoOutput(false);
      conn.setDoInput(true);

      int statusCode = conn.getResponseCode();

      if (statusCode >= 500 && statusCode <= 599) {
        // We retry on 5xx failures.
        Log.i(TAG, "performHttpPost transient failure: " + conn.getResponseMessage());
        throw new TransientFailure();
      }

      if (statusCode != 200) {
        Log.i(TAG, "performHttpPost permanent failure: " + conn.getResponseMessage());
        return null;
      }

      ByteArrayOutputStream output = new ByteArrayOutputStream();
      BufferedInputStream input = new BufferedInputStream(conn.getInputStream());
      for (int b = input.read(); b != -1; b = input.read()) {
        output.write(b);
      }

      return output.toByteArray();
    } finally {
      conn.disconnect();
    }
  }
}
