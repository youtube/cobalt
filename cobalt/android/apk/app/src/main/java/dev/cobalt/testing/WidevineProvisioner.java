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

package dev.cobalt.testing;

import android.media.MediaDrm;
import android.media.NotProvisionedException;
import android.os.Handler;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import dev.cobalt.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Pre-provisions Widevine MediaDrm for Android native test packages.
 *
 * <p>Production Cobalt delegates DRM provisioning to the web application via EME individualization
 * requests, which standalone native test suites such as {@code nplb} do not answer.
 *
 * <p>Lifetime/Ownership: Stateless utility class whose process-scoped completion latch spans the
 * test process lifetime.
 *
 * <p>Threading: Thread-safe. {@link #ensureProvisionedAsync} may be called from any thread
 * (including the UI thread); {@link #ensureProvisioned} blocks up to {@link #PROVISION_TIMEOUT_MS}
 * and must be called off the UI thread.
 */
public final class WidevineProvisioner {
  private static final String TAG = "WidevineProvisioner";
  private static final UUID WIDEVINE_UUID = UUID.fromString("edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");
  private static final int MAX_PROVISION_ROUNDS = 3;
  private static final int HTTP_TIMEOUT_MS = 3000;
  private static final long PROVISION_TIMEOUT_MS = 10000;
  private static final String SECURITY_LEVEL_L3 = "L3";

  private static final AtomicBoolean sProvisioningStarted = new AtomicBoolean(false);
  private static final CountDownLatch sProvisioningCompleteLatch = new CountDownLatch(1);

  private WidevineProvisioner() {}

  /** Runs provisioning on a daemon thread and posts {@code onComplete} to {@code handler} once. */
  public static void ensureProvisionedAsync(Handler handler, Runnable onComplete) {
    Thread thread =
        new Thread(
            () -> {
              ensureProvisioned();
              handler.post(onComplete);
            },
            "WidevineProvisionerAsync");
    thread.setDaemon(true);
    thread.start();
  }

  /** Ensures Widevine is provisioned for the calling package, bounded by a wall-clock timeout. */
  public static void ensureProvisioned() {
    if (!sProvisioningStarted.compareAndSet(false, true)) {
      try {
        sProvisioningCompleteLatch.await(PROVISION_TIMEOUT_MS, TimeUnit.MILLISECONDS);
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
      }
      return;
    }

    Thread thread = new Thread(WidevineProvisioner::runProvisioningOnce, "WidevineProvisioner");
    thread.setDaemon(true);
    thread.start();
    try {
      thread.join(PROVISION_TIMEOUT_MS);
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
    }
    if (thread.isAlive()) {
      Log.w(TAG, "Widevine provisioning timed out, continuing without it.");
      thread.interrupt();
    }
  }

  private static void runProvisioningOnce() {
    try {
      if (!MediaDrm.isCryptoSchemeSupported(WIDEVINE_UUID)) {
        return;
      }
      String defaultLevel = provisionSecurityLevel(null);
      if (defaultLevel != null
          && !defaultLevel.isEmpty()
          && !SECURITY_LEVEL_L3.equals(defaultLevel)
          && !Thread.currentThread().isInterrupted()) {
        provisionSecurityLevel(SECURITY_LEVEL_L3);
      }
    } catch (Throwable t) {
      Log.w(TAG, "Widevine pre-provisioning failed", t);
    } finally {
      sProvisioningCompleteLatch.countDown();
    }
  }

  @Nullable
  private static String provisionSecurityLevel(@Nullable String targetLevel) {
    MediaDrm drm = null;
    String level = targetLevel;
    try {
      drm = new MediaDrm(WIDEVINE_UUID);
      setPropertyQuietly(drm, "privacyMode", "disable");
      setPropertyQuietly(drm, "sessionSharing", "enable");
      if (targetLevel != null) {
        drm.setPropertyString("securityLevel", targetLevel);
      } else {
        level = getPropertyQuietly(drm, "securityLevel");
      }

      for (int round = 0; round < MAX_PROVISION_ROUNDS; round++) {
        if (Thread.currentThread().isInterrupted()) {
          return level;
        }
        try {
          byte[] sessionId = drm.openSession();
          drm.closeSession(sessionId);
          return level;
        } catch (NotProvisionedException e) {
          Log.i(TAG, "Provisioning Widevine (" + level + ", round " + (round + 1) + ")");
          MediaDrm.ProvisionRequest req = drm.getProvisionRequest();
          String defaultUrl = req != null ? req.getDefaultUrl() : null;
          byte[] requestData = req != null ? req.getData() : null;
          if (defaultUrl == null || defaultUrl.isEmpty() || requestData == null) {
            Log.w(TAG, "Empty provision request returned by MediaDrm");
            return level;
          }
          byte[] response =
              performHttpPost(buildProvisionUrl(defaultUrl, requestData), HTTP_TIMEOUT_MS);
          if (response == null || response.length == 0) {
            return level;
          }
          drm.provideProvisionResponse(response);
        }
      }
      return level;
    } catch (Throwable t) {
      Log.w(TAG, "Widevine provisioning failed for securityLevel=" + level, t);
      return level;
    } finally {
      closeQuietly(drm);
    }
  }

  @VisibleForTesting
  static String buildProvisionUrl(String defaultUrl, byte[] requestData) {
    String separator = defaultUrl.indexOf('?') >= 0 ? "&" : "?";
    return defaultUrl
        + separator
        + "signedRequest="
        + new String(requestData, StandardCharsets.UTF_8);
  }

  @VisibleForTesting
  @Nullable
  static byte[] performHttpPost(String requestUrl, int timeoutMs) {
    HttpURLConnection conn = null;
    try {
      conn = (HttpURLConnection) new URL(requestUrl).openConnection();
      conn.setConnectTimeout(timeoutMs);
      conn.setReadTimeout(timeoutMs);
      conn.setRequestMethod("POST");
      conn.setRequestProperty("User-Agent", "Widevine CDM v1.0");
      conn.setRequestProperty("Content-Type", "application/json");
      conn.setDoOutput(false);
      conn.setDoInput(true);
      int statusCode = conn.getResponseCode();
      if (statusCode != HttpURLConnection.HTTP_OK) {
        Log.w(TAG, "Provisioning HTTP POST failed with status " + statusCode);
        return null;
      }
      try (InputStream input = conn.getInputStream()) {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[4096];
        int read;
        while ((read = input.read(buffer)) != -1) {
          output.write(buffer, 0, read);
        }
        return output.toByteArray();
      }
    } catch (IOException | RuntimeException e) {
      Log.w(TAG, "Provisioning HTTP POST failed", e);
      return null;
    } finally {
      if (conn != null) {
        conn.disconnect();
      }
    }
  }

  private static void setPropertyQuietly(MediaDrm drm, String name, String value) {
    try {
      drm.setPropertyString(name, value);
    } catch (Throwable ignored) {
    }
  }

  @Nullable
  private static String getPropertyQuietly(MediaDrm drm, String name) {
    try {
      return drm.getPropertyString(name);
    } catch (Throwable ignored) {
      return null;
    }
  }

  @SuppressWarnings("deprecation")
  private static void closeQuietly(@Nullable MediaDrm drm) {
    if (drm == null) {
      return;
    }
    try {
      drm.release();
    } catch (Throwable ignored) {
    }
  }
}
