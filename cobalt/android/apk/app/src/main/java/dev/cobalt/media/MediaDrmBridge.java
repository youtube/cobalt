// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.media;

import android.media.DeniedByServerException;
import android.media.MediaCrypto;
import android.media.MediaCryptoException;
import android.media.MediaDrm;
import android.media.MediaDrm.OnEventListener;
import android.media.MediaDrmException;
import android.media.NotProvisionedException;
import android.media.UnsupportedSchemeException;
import android.os.Build;
import android.util.Base64;
import androidx.annotation.RequiresApi;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import java.nio.charset.StandardCharsets;

/** A wrapper of the android MediaDrm class. */
@UsedByNative
public class MediaDrmBridge {
  private static final String TAG = "CobaltDrm";
  // Implementation Notes:
  // - A media crypto session (mMediaCryptoSession) is opened after MediaDrm
  //   is created. This session will NOT be added to mSessionIds and will only
  //   be used to create the MediaCrypto object.
  // - Each createSession() call creates a new session. All created sessions
  //   are managed in mSessionIds.
  // - Whenever NotProvisionedException is thrown, we will clean up the
  //   current state and start the provisioning process.
  // - When provisioning is finished, we will try to resume suspended
  //   operations:
  //   a) Create the media crypto session if it's not created.
  //   b) Finish createSession() if previous createSession() was interrupted
  //      by a NotProvisionedException.
  // - Whenever an unexpected error occurred, we'll call release() to release
  //   all resources immediately, clear all states and fail all pending
  //   operations. After that all calls to this object will fail (e.g. return
  //   null or reject the promise). All public APIs and callbacks should check
  //   mMediaBridge to make sure release() hasn't been called.

  private static final char[] HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray();
  private static final long INVALID_NATIVE_MEDIA_DRM_BRIDGE = 0;

  // The value of this must stay in sync with kSbDrmTicketInvalid in "starboard/drm.h"
  private static final int SB_DRM_TICKET_INVALID = Integer.MIN_VALUE;

  // Scheme UUID for Widevine. See http://dashif.org/identifiers/protection/
  private static final UUID WIDEVINE_UUID = UUID.fromString("edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");

  // Deprecated in API 26, but we still log it on earlier devices.
  // We do handle STATUS_EXPIRED in nativeOnKeyStatusChange() for API 23+ devices.
  @SuppressWarnings("deprecation")
  private static final int MEDIA_DRM_EVENT_KEY_EXPIRED = MediaDrm.EVENT_KEY_EXPIRED;

  // Deprecated in API 23, but we still log it on earlier devices.
  @SuppressWarnings("deprecation")
  private static final int MEDIA_DRM_EVENT_PROVISION_REQUIRED = MediaDrm.EVENT_PROVISION_REQUIRED;

  // Added in API 23.
  private static final int MEDIA_DRM_EVENT_SESSION_RECLAIMED = MediaDrm.EVENT_SESSION_RECLAIMED;

  private MediaDrm mMediaDrm;
  private long mNativeMediaDrmBridge;
  private UUID mSchemeUUID;

  // A session only for the purpose of creating a MediaCrypto object. Created
  // after construction, or after the provisioning process is successfully
  // completed. No getKeyRequest() should be called on |mMediaCryptoSession|.
  private byte[] mMediaCryptoSession;

  // The map of all opened sessions (excluding mMediaCryptoSession) to their
  // mime types.
  private HashMap<ByteBuffer, String> mSessionIds = new HashMap<>();

  private MediaCrypto mMediaCrypto;

  // Return value type for calls to updateSession(), which contains whether or not the call
  // succeeded, and optionally an error message (that is empty on success).
  @UsedByNative
  private static class UpdateSessionResult {
    public enum Status {
      SUCCESS,
      FAILURE
    }

    // Whether or not the update session attempt succeeded or failed.
    private boolean mIsSuccess;

    // Descriptive error message or details, in the scenario where the update session call failed.
    private String mErrorMessage;

    public UpdateSessionResult(Status status, String errorMessage) {
      this.mIsSuccess = status == Status.SUCCESS;
      this.mErrorMessage = errorMessage;
    }

    @UsedByNative
    public boolean isSuccess() {
      return mIsSuccess;
    }

    @UsedByNative
    public String getErrorMessage() {
      return mErrorMessage;
    }
  }

  /**
   * Create a new MediaDrmBridge with the Widevine crypto scheme.
   *
   * @param nativeMediaDrmBridge The native owner of this class.
   */
  @UsedByNative
  static MediaDrmBridge create(String keySystem, long nativeMediaDrmBridge) {
    UUID cryptoScheme = WIDEVINE_UUID;
    if (!MediaDrm.isCryptoSchemeSupported(cryptoScheme)) {
      return null;
    }

    MediaDrmBridge mediaDrmBridge = null;
    try {
      mediaDrmBridge = new MediaDrmBridge(keySystem, cryptoScheme, nativeMediaDrmBridge);
      Log.i(TAG, "MediaDrmBridge successfully created.");
    } catch (UnsupportedSchemeException e) {
      Log.e(TAG, "Unsupported DRM scheme", e);
      return null;
    } catch (IllegalArgumentException e) {
      Log.e(TAG, "Failed to create MediaDrmBridge", e);
      return null;
    } catch (IllegalStateException e) {
      Log.e(TAG, "Failed to create MediaDrmBridge", e);
      return null;
    }

    if (!mediaDrmBridge.createMediaCrypto()) {
      return null;
    }

    return mediaDrmBridge;
  }

  /**
   * Check whether the Widevine crypto scheme is supported.
   *
   * @return true if the container and the crypto scheme is supported, or false otherwise.
   */
  @UsedByNative
  static boolean isWidevineCryptoSchemeSupported() {
    return MediaDrm.isCryptoSchemeSupported(WIDEVINE_UUID);
  }

  /**
   * Check whether `cbcs` scheme is supported.
   *
   * @return true if the `cbcs` encryption is supported, or false otherwise.
   */
  @UsedByNative
  static boolean isCbcsSchemeSupported() {
    // While 'cbcs' scheme was originally implemented in N, there was a bug (in the
    // DRM code) which means that it didn't really work properly until N-MR1).
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N_MR1;
  }

  /** Destroy the MediaDrmBridge object. */
  @UsedByNative
  void destroy() {
    mNativeMediaDrmBridge = INVALID_NATIVE_MEDIA_DRM_BRIDGE;
    if (mMediaDrm != null) {
      release();
    }
  }

  class LicenseRequestArgs {
    public final int ticket;
    public final byte[] initData;
    public final String mime;

    public LicenseRequestArgs(int ticket, byte[] initData, String mime) {
      this.ticket = ticket;
      this.initData = initData;
      this.mime = mime;
    }
  }
  LicenseRequestArgs pendingLicenseRequest;

  @UsedByNative
  void runPendingTasks() {
    if (pendingLicenseRequest != null) {
      Log.i(TAG, "Wait for pending license request.");
      try {
        Thread.sleep(1_000);
      } catch(Exception e) {
        // Do nothing.
      }
      Log.i(TAG, "Handle pending license request.");
      try {
        createSessionInternal(pendingLicenseRequest);
      } catch (NotProvisionedException e) {
        Log.e(TAG, "Met Provisioning error while handling pending license request.", e);
        return;
      }
      pendingLicenseRequest = null;

      return;
    }

    Log.i(TAG, "There is no pending task.");
  }

  @UsedByNative
  void createSession(int ticket, byte[] initData, String mime) {
    Log.i(TAG, "createSession()");
    assert mMediaDrm != null;
    LicenseRequestArgs args = new LicenseRequestArgs(ticket, initData, mime);

    try {
      createSessionInternal(args);
    } catch(NotProvisionedException e) {
      Log.i(TAG, "Device not provisioned. Start provisioning.");
      pendingLicenseRequest = new LicenseRequestArgs(
        /*ticket=*/Integer.MIN_VALUE, initData, mime);
      startProvisioning(ticket);
      return;
    } catch(Exception e) {
      Log.e(TAG, "Device not provisioned", e);
      return;
    }
  }

  private void createSessionInternal(LicenseRequestArgs args) throws android.media.NotProvisionedException {
    boolean newSessionOpened = false;
    byte[] sessionId;
    MediaDrm.KeyRequest request;

    createMediaCryptoSession();

    try {
      sessionId = openSession();
      if (sessionId == null) {
        Log.e(TAG, "Open session failed.");
        return;
      }

      if (usingFirstSessionId) {
        assert mediaDrmFirstSessionId == null;
        mediaDrmFirstSessionId = sessionId;
      }

      newSessionOpened = true;
      if (sessionExists(sessionId)) {
        Log.e(TAG, "Opened session that already exists.");
        return;
      }

      request = getKeyRequest(sessionId, args.initData, args.mime);
      if (request == null) {
        try {
          // Some implementations let this method throw exceptions.
          Log.i(TAG, "Calling mMediaDrm.closeSession(...) in createSession failure path.");
          mMediaDrm.closeSession(sessionId);
        } catch (Exception e) {
          Log.e(TAG, "closeSession failed", e);
        }
        Log.e(TAG, "Generate request failed.");
        return;
      }

      // Success!
      Log.i(TAG, "createSession(): Session (%s) created.", bytesToHexString(sessionId));
    } catch (NotProvisionedException e) {
      Log.e(TAG, "Device not provisioned", e);
      return;
    }

    mSessionIds.put(ByteBuffer.wrap(sessionId), args.mime);

    if (Arrays.equals(sessionId, mediaDrmFirstSessionId)) {
      sessionId = FIRST_DRM_SESSION_ID;
    }
    onSessionMessage(args.ticket, sessionId, request);
  }

  /**
   * Update a session with response.
   *
   * @param sessionId Reference ID of session to be updated.
   * @param response Response data from the server.
   */
  @UsedByNative
  UpdateSessionResult updateSession(int ticket, byte[] sessionId, byte[] response) {
    assert mMediaDrm != null;

    if (Arrays.equals(sessionId, FIRST_DRM_SESSION_ID)) {
      Log.i(TAG, "Handle provision response.");
      return handleProvisionResponse(response);
    }

    Log.i(TAG, "updateSession()");
    if (!sessionExists(sessionId)) {
      Log.e(TAG, "updateSession tried to update a session that does not exist.");
      return new UpdateSessionResult(
          UpdateSessionResult.Status.FAILURE,
          "Failed to update session because it does not exist. StackTrace: "
              + android.util.Log.getStackTraceString(new Throwable()));
    }

    try {
      try {
        Log.i(TAG, "Calling mMediaDrm.provideKeyResponse: sessionId=" + bytesToHexString(sessionId)
              + ", response(bytes)=" + response.length);
        mMediaDrm.provideKeyResponse(sessionId, response);
      } catch (IllegalStateException e) {
        // This is not really an exception. Some error codes are incorrectly
        // reported as an exception.
        Log.e(TAG, "Exception intentionally caught when calling provideKeyResponse()", e);
      }
      Log.i(TAG, String.format("Key successfully added for session %s", bytesToHexString(sessionId)));
    } catch (NotProvisionedException e) {
      // TODO: Should we handle this?
      Log.e(TAG, "Failed to provide key response", e);
      release();
      return new UpdateSessionResult(
          UpdateSessionResult.Status.FAILURE,
          "Update session failed due to lack of provisioning. StackTrace: "
              + android.util.Log.getStackTraceString(e));
    } catch (DeniedByServerException e) {
      Log.e(TAG, "Failed to provide key response.", e);
      release();
      return new UpdateSessionResult(
          UpdateSessionResult.Status.FAILURE,
          "Update session failed because we were denied by server. StackTrace: "
              + android.util.Log.getStackTraceString(e));
    } catch (Exception e) {
      Log.e(TAG, "", e);
      release();
      return new UpdateSessionResult(
          UpdateSessionResult.Status.FAILURE,
          "Update session failed. Caught exception: "
              + e.getMessage()
              + " StackTrace: "
              + android.util.Log.getStackTraceString(e));
    }

    isKeyUpdated = true;
    return new UpdateSessionResult(UpdateSessionResult.Status.SUCCESS, "");
  }

  private boolean isKeyUpdated = false;
  @UsedByNative
  boolean isKeyLoaded() {
    return isKeyUpdated;
  }

  /**
   * Close a session that was previously created by createSession().
   *
   * @param sessionId ID of session to be closed.
   */
  @UsedByNative
  void closeSession(byte[] sessionId) {
    Log.i(TAG, "closeSession()");
    if (mMediaDrm == null) {
      Log.e(TAG, "closeSession() called when MediaDrm is null.");
      return;
    }

    if (Arrays.equals(sessionId, FIRST_DRM_SESSION_ID)) {
      if (mediaDrmFirstSessionId == null) {
        Log.i(TAG, "No MediaDrm session is created for " + bytesToHexString(FIRST_DRM_SESSION_ID));
        return;
      }

      sessionId = mediaDrmFirstSessionId;
      Log.i(TAG, "Close first session: " + bytesToHexString(sessionId));
    }

    if (!sessionExists(sessionId)) {
      Log.e(TAG, "Invalid sessionId in closeSession(): " + bytesToHexString(sessionId));
      return;
    }

    try {
      // Some implementations don't have removeKeys.
      // https://bugs.chromium.org/p/chromium/issues/detail?id=475632
      Log.i(TAG, "Calling mMediaDrm.removeKeys(...)");
      mMediaDrm.removeKeys(sessionId);
    } catch (Exception e) {
      Log.e(TAG, "removeKeys failed: ", e);
    }

    try {
      // Some implementations let this method throw exceptions.
      Log.i(TAG, "Calling mMediaDrm.closeSession(...)");
      mMediaDrm.closeSession(sessionId);
    } catch (Exception e) {
      Log.e(TAG, "closeSession failed: ", e);
    }
    mSessionIds.remove(ByteBuffer.wrap(sessionId));
    Log.i(TAG, String.format("Session %s closed", bytesToHexString(sessionId)));
  }

  @UsedByNative
  byte[] getMetricsInBase64() {
    if (Build.VERSION.SDK_INT < 28) {
      return null;
    }
    byte[] metrics;
    try {
      Log.i(TAG, "Calling mMediaDrm.getPropertyByteArray(\"metrics\")");
      metrics = mMediaDrm.getPropertyByteArray("metrics");
    } catch (Exception e) {
      Log.e(TAG, "Failed to retrieve DRM Metrics.");
      return null;
    }
    return Base64.encode(metrics, Base64.NO_PADDING | Base64.NO_WRAP | Base64.URL_SAFE);
  }

  @UsedByNative
  MediaCrypto getMediaCrypto() {
    return mMediaCrypto;
  }

  private MediaDrmBridge(String keySystem, UUID schemeUUID, long nativeMediaDrmBridge)
      throws android.media.UnsupportedSchemeException {
    mSchemeUUID = schemeUUID;
    Log.i(TAG, "Calling mMediaDrm Ctor.");
    mMediaDrm = new MediaDrm(schemeUUID);

    // Get info of hdcp connection
    if (Build.VERSION.SDK_INT >= 29) {
      getConnectedHdcpLevelInfoV29(mMediaDrm);
    }

    mNativeMediaDrmBridge = nativeMediaDrmBridge;
    if (!isNativeMediaDrmBridgeValid()) {
      throw new IllegalArgumentException(
          String.format("Invalid nativeMediaDrmBridge value: |%d|.", nativeMediaDrmBridge));
    }

    Log.i(TAG, "Calling mMediaDrm.setOnEventListener(...)");
    mMediaDrm.setOnEventListener(
        new OnEventListener() {
          @Override
          public void onEvent(MediaDrm md, byte[] sessionId, int event, int extra, byte[] data) {
            if (event == MediaDrm.EVENT_KEY_REQUIRED) {
              Log.i(TAG, "MediaDrm.EVENT_KEY_REQUIRED");
              String mime = mSessionIds.get(ByteBuffer.wrap(sessionId));
              MediaDrm.KeyRequest request = null;
              try {
                request = getKeyRequest(sessionId, data, mime);
              } catch (NotProvisionedException e) {
                Log.e(TAG, "EventListener: getKeyRequest failed.", e);
                return;
              }
              assert request != null;

              onSessionMessage(SB_DRM_TICKET_INVALID, sessionId, request);
            } else if (event == MEDIA_DRM_EVENT_KEY_EXPIRED) {
              Log.i(TAG, "MediaDrm.EVENT_KEY_EXPIRED");
            } else if (event == MediaDrm.EVENT_VENDOR_DEFINED) {
              Log.i(TAG, "MediaDrm.EVENT_VENDOR_DEFINED");
            } else if (event == MEDIA_DRM_EVENT_PROVISION_REQUIRED) {
              Log.i(TAG, "MediaDrm.EVENT_PROVISION_REQUIRED");
            } else if (event == MEDIA_DRM_EVENT_SESSION_RECLAIMED) {
              Log.i(TAG, "MediaDrm.EVENT_SESSION_RECLAIMED");
            } else {
              Log.e(TAG, "Invalid DRM event " + event);
              return;
            }
          }
        });

    Log.i(TAG, "Calling mMediaDrm.setOnKeyStatusChangeListener(...)");
    mMediaDrm.setOnKeyStatusChangeListener(
        new MediaDrm.OnKeyStatusChangeListener() {
          @Override
          public void onKeyStatusChange(
              MediaDrm md,
              byte[] sessionId,
              List<MediaDrm.KeyStatus> keyInformation,
              boolean hasNewUsableKey) {
            nativeOnKeyStatusChange(
                mNativeMediaDrmBridge,
                sessionId,
                keyInformation.toArray(new MediaDrm.KeyStatus[keyInformation.size()]));
          }
        },
        null);

    Log.i(TAG, "Calling mMediaDrm.setPropertyString(\"privacyMode\", \"disable\")");
    mMediaDrm.setPropertyString("privacyMode", "disable");
    Log.i(TAG, "Calling mMediaDrm.setPropertyString(\"sessionSharing\", \"enable\")");
    mMediaDrm.setPropertyString("sessionSharing", "enable");
    if (keySystem.equals("com.youtube.widevine.l3")) {
      Log.i(TAG, "Calling mMediaDrm.getPropertyString(\"securityLevel\")");
      if (!mMediaDrm.getPropertyString("securityLevel").equals("L3")) {
        Log.i(TAG, "Calling mMediaDrm.setPropertyString(\"securityLevel\", \"L3\")");
        mMediaDrm.setPropertyString("securityLevel", "L3");
      }
    }
  }

  /** Convert byte array to hex string for logging. */
  private static String bytesToHexString(byte[] bytes) {
    try {
      return StandardCharsets.UTF_8.newDecoder().decode(ByteBuffer.wrap(bytes)).toString();
    } catch (Exception e) {
      // Go to the fallback.
    }
    StringBuilder hexString = new StringBuilder();
    for (int i = 0; i < bytes.length; ++i) {
      hexString.append(HEX_CHAR_LOOKUP[bytes[i] >>> 4]);
      hexString.append(HEX_CHAR_LOOKUP[bytes[i] & 0xf]);
    }
    return hexString.toString();
  }

  private void onSessionMessage(
      int ticket, final byte[] sessionId, final MediaDrm.KeyRequest request) {
    if (!isNativeMediaDrmBridgeValid()) {
      return;
    }

    int requestType = request.getRequestType();

    nativeOnSessionMessage(
        mNativeMediaDrmBridge, ticket, sessionId, requestType, request.getData());
  }

  /**
   * Get a key request.
   *
   * @param sessionId ID of session on which we need to get the key request.
   * @param data Data needed to get the key request.
   * @param mime Mime type to get the key request.
   * @return the key request.
   */
  private MediaDrm.KeyRequest getKeyRequest(byte[] sessionId, byte[] data, String mime)
      throws android.media.NotProvisionedException {
    if (mMediaDrm == null) {
      throw new IllegalStateException("mMediaDrm cannot be null in getKeyRequest");
    }
    if (mMediaCryptoSession == null) {
      throw new IllegalStateException("mMediaCryptoSession cannot be null in getKeyRequest.");
    }
    // TODO: Cannot do this during provisioning pending.

    HashMap<String, String> optionalParameters = new HashMap<>();
    MediaDrm.KeyRequest request = null;
    try {
      Log.i(TAG, "Calling mMediaDrm.getKeyRequest: sessionId=" + bytesToHexString(sessionId)
          + ", mime = " + mime + ", data(bytes)=" + data.length + ", optionalParameters=(null)");
      request =
          mMediaDrm.getKeyRequest(
              sessionId, data, mime, MediaDrm.KEY_TYPE_STREAMING, null);
    } catch (IllegalStateException e) {
      if (e instanceof android.media.MediaDrm.MediaDrmStateException) {
        Log.e(TAG, "MediaDrmStateException fired during getKeyRequest().", e);
      }
    }

    String result = (request != null) ? "succeeded" : "failed";
    Log.i(TAG, String.format("getKeyRequest %s!", result));

    return request;
  }

  /**
   * Create a MediaCrypto object.
   *
   * @return false upon fatal error in creating MediaCrypto. Returns true otherwise, including the
   *     following two cases: 1. MediaCrypto is successfully created and notified. 2. Device is not
   *     provisioned and MediaCrypto creation will be tried again after the provisioning process is
   *     completed.
   *     <p>When false is returned, the caller should call release(), which will notify the native
   *     code with a null MediaCrypto, if needed.
   */
  private boolean createMediaCrypto() {
    if (mMediaDrm == null) {
      throw new IllegalStateException("Cannot create media crypto with null mMediaDrm.");
    }
    // Create MediaCrypto object.
    try {
      if (MediaCrypto.isCryptoSchemeSupported(mSchemeUUID)) {
        Log.i(TAG, "Calling MediaCrypto Ctor. mSchemeUUID = " + mSchemeUUID.toString());
        MediaCrypto mediaCrypto = new MediaCrypto(mSchemeUUID, new byte[0]);
        Log.i(TAG, "MediaCrypto successfully created!");
        mMediaCrypto = mediaCrypto;
        return true;
      } else {
        Log.e(TAG, "Cannot create MediaCrypto for unsupported scheme.");
      }
    } catch (MediaCryptoException e) {
      Log.e(TAG, "Cannot create MediaCrypto", e);
    }

    return false;
  }

  /**
   * Open a new session.
   *
   * @return ID of the session opened. Returns null if unexpected error happened.
   */
  private byte[] openSession() throws android.media.NotProvisionedException {
    Log.i(TAG, "openSession()");
    if (mMediaDrm == null) {
      throw new IllegalStateException("mMediaDrm cannot be null in openSession");
    }

    try {
      Log.i(TAG, "Calling mMediaDrm.openSession()");
      byte[] sessionId = mMediaDrm.openSession();
      // Make a clone here in case the underlying byte[] is modified.
      return sessionId.clone();
    } catch (RuntimeException e) { // TODO: Drop this?
      Log.e(TAG, "Cannot open a new session", e);
      release();
      return null;
    } catch (NotProvisionedException e) {
      // Throw NotProvisionedException so that we can attemptProvisioning().
      throw e;
    } catch (MediaDrmException e) {
      // Other MediaDrmExceptions (e.g. ResourceBusyException) are not
      // recoverable.
      Log.e(TAG, "Cannot open a new session", e);
      release();
      return null;
    }
  }

  void createMediaCryptoSession() throws android.media.NotProvisionedException {
    if (mMediaCryptoSession != null) {
      Log.i(TAG, "MediaCryptoSession is already created");
      return;
    }

    assert mMediaCrypto != null;

    try {
      mMediaCryptoSession = openSession();
    } catch (NotProvisionedException e) {
      Log.w(TAG, "Device not provisioned. Re-throw NotProvisionedException");
      throw e;
    }
    assert mMediaCryptoSession != null;

    try {
      Log.i(TAG, "Calling MediaCrypto.setMediaDrmSession(mMediaCryptoSession)");
      mMediaCrypto.setMediaDrmSession(mMediaCryptoSession);
    } catch (MediaCryptoException e3) {
      Log.e(TAG, "Unable to set media drm session", e3);
      try {
        // Some implementations let this method throw exceptions.
        Log.i(TAG, "Calling mMediaDrm.closeSession(...).");
        mMediaDrm.closeSession(mMediaCryptoSession);
      } catch (Exception e) {
        Log.e(TAG, "closeSession failed: ", e);
      }
      mMediaCryptoSession = null;
      return;
    }

    Log.i(TAG, "MediaCrypto Session created: sessionId=" + bytesToHexString(mMediaCryptoSession));
  }

  private static final byte[] FIRST_DRM_SESSION_ID = "initialdrmsessionid".getBytes();
  private static final int INDIVIDUALIZATION_REQUEST_TYPE = 3;

  private boolean usingFirstSessionId = false;
  private byte[] mediaDrmFirstSessionId;

  /**
   * Attempt to get the device that we are currently running on provisioned.
   *
   * @return whether provisioning was successful or not.
   */
  // Provisioning should be singletone activity.
  private void startProvisioning(int ticket) {
    Log.i(TAG, "start provisioning()");
    Log.i(TAG, "Calling mMediaDrm.getProvisionRequest()");

    MediaDrm.ProvisionRequest request = mMediaDrm.getProvisionRequest();

    usingFirstSessionId = true;

    nativeOnSessionMessage(
        mNativeMediaDrmBridge,
        ticket, FIRST_DRM_SESSION_ID, INDIVIDUALIZATION_REQUEST_TYPE, request.getData());
  }

  private UpdateSessionResult handleProvisionResponse(byte[] response) {
    Log.i(TAG, "handleProvisionResponse()");

     try {
      Log.i(TAG, "Calling mMediaDrm.provideProvisionResponse()");
      mMediaDrm.provideProvisionResponse(response);
    } catch (android.media.DeniedByServerException e) {
      Log.e(TAG, "Failed to provide provision response.", e);
      return new UpdateSessionResult(
          UpdateSessionResult.Status.FAILURE,
          e.getMessage());
    }
    Log.i(TAG, "provideProvisionResponse succeeded");

    return new UpdateSessionResult(UpdateSessionResult.Status.SUCCESS, "");
  }

  /**
   * Check whether |sessionId| is an existing session ID, excluding the media crypto session.
   *
   * @param sessionId Crypto session Id.
   * @return true if |sessionId| exists, false otherwise.
   */
  private boolean sessionExists(byte[] sessionId) {
    return !Arrays.equals(sessionId, mMediaCryptoSession)
        && mSessionIds.containsKey(ByteBuffer.wrap(sessionId));
  }

  /** Release all allocated resources and finish all pending operations. */
  private void release() {
    // Note that mNativeMediaDrmBridge may have already been reset (see destroy()).
    if (mMediaDrm == null) {
      throw new IllegalStateException("Called release with null mMediaDrm.");
    }

    // Close all open sessions.
    for (ByteBuffer sessionId : mSessionIds.keySet()) {
      try {
        // Some implementations don't have removeKeys.
        // https://bugs.chromium.org/p/chromium/issues/detail?id=475632
        Log.i(TAG, "Calling mMediaDrm.removeKeys(...) in release loop.");
        mMediaDrm.removeKeys(sessionId.array());
      } catch (Exception e) {
        Log.e(TAG, "removeKeys failed: ", e);
      }

      try {
        Log.i(TAG, "Calling mMediaDrm.closeSession(...) in release loop.");
        // Some implementations let this method throw exceptions.
        mMediaDrm.closeSession(sessionId.array());
      } catch (Exception e) {
        Log.e(TAG, "closeSession failed: ", e);
      }
      Log.i(
          TAG,
          String.format("Successfully closed session (%s)", bytesToHexString(sessionId.array())));
    }
    mSessionIds.clear();

    // Close mMediaCryptoSession if it's open.
    if (mMediaCryptoSession != null) {
      try {
        Log.i(
            TAG, "Calling mMediaDrm.closeSession(...) for mMediaCryptoSession in release.");
        // Some implementations let this method throw exceptions.
        mMediaDrm.closeSession(mMediaCryptoSession);
      } catch (Exception e) {
        Log.e(TAG, "closeSession failed: ", e);
      }
      mMediaCryptoSession = null;
    }

    if (mMediaDrm != null) {
      if (Build.VERSION.SDK_INT >= 28) {
        closeMediaDrmV28(mMediaDrm);
      } else {
        releaseMediaDrmDeprecated(mMediaDrm);
      }
      mMediaDrm = null;
    }
  }

  @SuppressWarnings("deprecation")
  private void releaseMediaDrmDeprecated(MediaDrm mediaDrm) {
    Log.i(TAG, "Calling mMediaDrm.release() (deprecated)");
    mediaDrm.release();
  }

  @RequiresApi(28)
  private void closeMediaDrmV28(MediaDrm mediaDrm) {
    Log.i(TAG, "Calling mMediaDrm.close()");
    mediaDrm.close();
  }

  @RequiresApi(29)
  private void getConnectedHdcpLevelInfoV29(MediaDrm mediaDrm) {
    Log.i(TAG, "Calling mMediaDrm.getConnectedHdcpLevel()");
    int hdcpLevel = mediaDrm.getConnectedHdcpLevel();
    switch (hdcpLevel) {
      case MediaDrm.HDCP_V1:
        Log.i(TAG, "MediaDrm HDCP Level is HDCP_V1.");
        break;
      case MediaDrm.HDCP_V2:
        Log.i(TAG, "MediaDrm HDCP Level is HDCP_V2.");
        break;
      case MediaDrm.HDCP_V2_1:
        Log.i(TAG, "MediaDrm HDCP Level is HDCP_V2_1.");
        break;
      case MediaDrm.HDCP_V2_2:
        Log.i(TAG, "MediaDrm HDCP Level is HDCP_V2_2.");
        break;
      case MediaDrm.HDCP_V2_3:
        Log.i(TAG, "MediaDrm HDCP Level is HDCP_V2_3.");
        break;
      case MediaDrm.HDCP_NONE:
        Log.i(TAG, "MediaDrm HDCP Level is HDCP_NONE.");
        break;
      case MediaDrm.HDCP_NO_DIGITAL_OUTPUT:
        Log.i(TAG, "MediaDrm HDCP Level is HDCP_NO_DIGITAL_OUTPUT.");
        break;
      case MediaDrm.HDCP_LEVEL_UNKNOWN:
        Log.i(TAG, "MediaDrm HDCP Level is HDCP_LEVEL_UNKNOWN.");
        break;
      default:
        Log.i(TAG, String.format(Locale.US, "Unknown MediaDrm HDCP level %d.", hdcpLevel));
        break;
    }
  }

  private boolean isNativeMediaDrmBridgeValid() {
    return mNativeMediaDrmBridge != INVALID_NATIVE_MEDIA_DRM_BRIDGE;
  }

  private native void nativeOnSessionMessage(
      long nativeMediaDrmBridge, int ticket, byte[] sessionId, int requestType, byte[] message);

  private native void nativeOnKeyStatusChange(
      long nativeMediaDrmBridge, byte[] sessionId, MediaDrm.KeyStatus[] keyInformation);
}
