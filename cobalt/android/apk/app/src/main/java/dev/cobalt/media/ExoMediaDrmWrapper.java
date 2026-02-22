// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.media;

import android.media.DeniedByServerException;
import android.media.MediaCryptoException;
import android.media.MediaDrmException;
import android.media.NotProvisionedException;
import android.os.PersistableBundle;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.media3.common.C;
import androidx.media3.common.DrmInitData;
import androidx.media3.common.util.UnstableApi;
import androidx.media3.exoplayer.analytics.PlayerId;
import androidx.media3.exoplayer.drm.ExoMediaDrm;
import androidx.media3.exoplayer.drm.FrameworkMediaDrm;
import androidx.media3.exoplayer.drm.FrameworkCryptoConfig;
import androidx.media3.exoplayer.drm.UnsupportedDrmException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * A wrapper around {@link FrameworkMediaDrm} that captures the session ID when a session is opened.
 */
@UnstableApi
public final class ExoMediaDrmWrapper implements ExoMediaDrm {

  private final FrameworkMediaDrm mFrameworkMediaDrm;
  private byte[] mSessionId;

  public ExoMediaDrmWrapper(UUID uuid) throws UnsupportedDrmException {
    mFrameworkMediaDrm = FrameworkMediaDrm.newInstance(uuid);
  }

  public byte[] getSessionId() {
    return mSessionId;
  }

  @Override
  public void setOnEventListener(@Nullable OnEventListener listener) {
    mFrameworkMediaDrm.setOnEventListener(listener);
  }

  @Override
  public void setOnKeyStatusChangeListener(@Nullable OnKeyStatusChangeListener listener) {
    mFrameworkMediaDrm.setOnKeyStatusChangeListener(listener);
  }

  @Override
  public void setOnExpirationUpdateListener(@Nullable OnExpirationUpdateListener listener) {
    mFrameworkMediaDrm.setOnExpirationUpdateListener(listener);
  }

  @NonNull
  @Override
  public byte[] openSession() throws MediaDrmException {
    mSessionId = mFrameworkMediaDrm.openSession();
    return mSessionId;
  }

  @Override
  public void closeSession(@NonNull byte[] sessionId) {
    mFrameworkMediaDrm.closeSession(sessionId);
  }

  @Override
  public void setPlayerIdForSession(@NonNull byte[] sessionId, @NonNull PlayerId playerId) {
    mFrameworkMediaDrm.setPlayerIdForSession(sessionId, playerId);
  }

  @NonNull
  @Override
  public KeyRequest getKeyRequest(
      @NonNull byte[] scope,
      @Nullable List<DrmInitData.SchemeData> schemeDatas,
      int keyType,
      @Nullable HashMap<String, String> optionalParameters)
      throws NotProvisionedException {
    return mFrameworkMediaDrm.getKeyRequest(scope, schemeDatas, keyType, optionalParameters);
  }

  @Override
  @Nullable
  public byte[] provideKeyResponse(@NonNull byte[] scope, @NonNull byte[] response)
      throws NotProvisionedException, DeniedByServerException {
    return mFrameworkMediaDrm.provideKeyResponse(scope, response);
  }

  @NonNull
  @Override
  public ProvisionRequest getProvisionRequest() {
    return mFrameworkMediaDrm.getProvisionRequest();
  }

  @Override
  public void provideProvisionResponse(@NonNull byte[] response) throws DeniedByServerException {
    mFrameworkMediaDrm.provideProvisionResponse(response);
  }

  @NonNull
  @Override
  public Map<String, String> queryKeyStatus(@NonNull byte[] sessionId) {
    return mFrameworkMediaDrm.queryKeyStatus(sessionId);
  }

  @Override
  public boolean requiresSecureDecoder(@NonNull byte[] sessionId, @NonNull String mimeType) {
    return mFrameworkMediaDrm.requiresSecureDecoder(sessionId, mimeType);
  }

  @Override
  public void acquire() {
    mFrameworkMediaDrm.acquire();
  }

  @Override
  public void release() {
    mFrameworkMediaDrm.release();
  }

  @Override
  public void restoreKeys(@NonNull byte[] sessionId, @NonNull byte[] keySetId) {
    mFrameworkMediaDrm.restoreKeys(sessionId, keySetId);
  }

  @Override
  @RequiresApi(29)
  public void removeOfflineLicense(@NonNull byte[] keySetId) {
    mFrameworkMediaDrm.removeOfflineLicense(keySetId);
  }

  @NonNull
  @Override
  @RequiresApi(29)
  public List<byte[]> getOfflineLicenseKeySetIds() {
    return mFrameworkMediaDrm.getOfflineLicenseKeySetIds();
  }

  @Override
  @Nullable
  public PersistableBundle getMetrics() {
    return mFrameworkMediaDrm.getMetrics();
  }

  @NonNull
  @Override
  public String getPropertyString(@NonNull String propertyName) {
    return mFrameworkMediaDrm.getPropertyString(propertyName);
  }

  @NonNull
  @Override
  public byte[] getPropertyByteArray(@NonNull String propertyName) {
    return mFrameworkMediaDrm.getPropertyByteArray(propertyName);
  }

  @Override
  public void setPropertyString(@NonNull String propertyName, @NonNull String value) {
    mFrameworkMediaDrm.setPropertyString(propertyName, value);
  }

  @Override
  public void setPropertyByteArray(@NonNull String propertyName, @NonNull byte[] value) {
    mFrameworkMediaDrm.setPropertyByteArray(propertyName, value);
  }

  @NonNull
  @Override
  public FrameworkCryptoConfig createCryptoConfig(byte[] sessionId) throws MediaCryptoException {
    return mFrameworkMediaDrm.createCryptoConfig(sessionId);
  }

  @Override
  public @C.CryptoType int getCryptoType() {
    return mFrameworkMediaDrm.getCryptoType();
  }
}
