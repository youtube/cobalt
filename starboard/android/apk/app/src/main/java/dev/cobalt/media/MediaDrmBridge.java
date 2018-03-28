// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Modifications Copyright 2017 Google Inc. All Rights Reserved.
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

import static dev.cobalt.media.Log.TAG;

import android.annotation.TargetApi;
import android.media.DeniedByServerException;
import android.media.MediaCrypto;
import android.media.MediaCryptoException;
import android.media.MediaDrm;
import android.media.MediaDrm.OnEventListener;
import android.media.MediaDrmException;
import android.media.NotProvisionedException;
import android.media.UnsupportedSchemeException;
import android.os.Build;
import android.util.Log;
import dev.cobalt.coat.CobaltHttpHelper;
import dev.cobalt.util.UsedByNative;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;

/** A wrapper of the android MediaDrm class. */
@UsedByNative
public class MediaDrmBridge {
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

  /**
   * Create a new MediaDrmBridge with the Widevine crypto scheme.
   *
   * @param nativeMediaDrmBridge The native owner of this class.
   */
  @UsedByNative
  static MediaDrmBridge create(long nativeMediaDrmBridge) {
    UUID cryptoScheme = WIDEVINE_UUID;
    if (!MediaDrm.isCryptoSchemeSupported(cryptoScheme)) {
      return null;
    }

    MediaDrmBridge mediaDrmBridge = null;
    try {
      mediaDrmBridge = new MediaDrmBridge(cryptoScheme, nativeMediaDrmBridge);
      Log.d(TAG, "MediaDrmBridge successfully created.");
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
   * Check whether the Widevine crypto scheme is supported for the given container. If
   * |containerMimeType| is an empty string, we just return whether the crypto scheme is supported.
   *
   * @return true if the container and the crypto scheme is supported, or false otherwise.
   */
  @UsedByNative
  static boolean isWidevineCryptoSchemeSupported(String containerMimeType) {
    if (containerMimeType.isEmpty()) {
      return isWidevineCryptoSchemeSupported();
    }
    return MediaDrm.isCryptoSchemeSupported(WIDEVINE_UUID, containerMimeType);
  }

  /** Destroy the MediaDrmBridge object. */
  @UsedByNative
  void destroy() {
    mNativeMediaDrmBridge = INVALID_NATIVE_MEDIA_DRM_BRIDGE;
    if (mMediaDrm != null) {
      release();
    }
  }

  @UsedByNative
  void createSession(int ticket, byte[] initData, String mime) {
    Log.d(TAG, "createSession()");

    if (mMediaDrm == null) {
      Log.e(TAG, "createSession() called when MediaDrm is null.");
      return;
    }

    boolean newSessionOpened = false;
    byte[] sessionId = null;
    try {
      sessionId = openSession();
      if (sessionId == null) {
        Log.e(TAG, "Open session failed.");
        return;
      }
      newSessionOpened = true;
      if (sessionExists(sessionId)) {
        Log.e(TAG, "Opened session that already exists.");
        return;
      }

      MediaDrm.KeyRequest request = null;
      request = getKeyRequest(sessionId, initData, mime);
      if (request == null) {
        try {
          // Some implementations let this method throw exceptions.
          mMediaDrm.closeSession(sessionId);
        } catch (Exception e) {
          Log.e(TAG, "closeSession failed", e);
        }
        Log.e(TAG, "Generate request failed.");
        return;
      }

      // Success!
      Log.d(
          TAG,
          String.format("createSession(): Session (%s) created.", bytesToHexString(sessionId)));
      mSessionIds.put(ByteBuffer.wrap(sessionId), mime);
      onSessionMessage(ticket, sessionId, request);
    } catch (NotProvisionedException e) {
      Log.e(TAG, "Device not provisioned", e);
      if (newSessionOpened) {
        try {
          // Some implementations let this method throw exceptions.
          mMediaDrm.closeSession(sessionId);
        } catch (Exception ex) {
          Log.e(TAG, "closeSession failed", ex);
        }
      }
      attemptProvisioning();
    }
  }

  /**
   * Update a session with response.
   *
   * @param sessionId Reference ID of session to be updated.
   * @param response Response data from the server.
   */
  @UsedByNative
  boolean updateSession(byte[] sessionId, byte[] response) {
    Log.d(TAG, "updateSession()");
    if (mMediaDrm == null) {
      Log.e(TAG, "updateSession() called when MediaDrm is null.");
      return false;
    }

    if (!sessionExists(sessionId)) {
      Log.e(TAG, "updateSession tried to update a session that does not exist.");
      return false;
    }

    try {
      try {
        mMediaDrm.provideKeyResponse(sessionId, response);
      } catch (IllegalStateException e) {
        // This is not really an exception. Some error codes are incorrectly
        // reported as an exception.
        Log.e(TAG, "Exception intentionally caught when calling provideKeyResponse()", e);
      }
      Log.d(
          TAG, String.format("Key successfully added for session %s", bytesToHexString(sessionId)));
      if (Build.VERSION.SDK_INT < 23) {
        // Pass null to indicate that KeyStatus isn't supported.
        nativeOnKeyStatusChange(mNativeMediaDrmBridge, sessionId, null);
      }
      return true;
    } catch (NotProvisionedException e) {
      // TODO: Should we handle this?
      Log.e(TAG, "failed to provide key response", e);
    } catch (DeniedByServerException e) {
      Log.e(TAG, "failed to provide key response", e);
    }
    Log.e(TAG, "Update session failed.");
    release();
    return false;
  }

  /**
   * Close a session that was previously created by createSession().
   *
   * @param sessionId ID of session to be closed.
   */
  @UsedByNative
  void closeSession(byte[] sessionId) {
    Log.d(TAG, "closeSession()");
    if (mMediaDrm == null) {
      Log.e(TAG, "closeSession() called when MediaDrm is null.");
      return;
    }

    if (!sessionExists(sessionId)) {
      Log.e(TAG, "Invalid sessionId in closeSession(): " + bytesToHexString(sessionId));
      return;
    }

    try {
      // Some implementations don't have removeKeys.
      // https://bugs.chromium.org/p/chromium/issues/detail?id=475632
      mMediaDrm.removeKeys(sessionId);
    } catch (Exception e) {
      Log.e(TAG, "removeKeys failed: ", e);
    }
    try {
      // Some implementations let this method throw exceptions.
      mMediaDrm.closeSession(sessionId);
    } catch (Exception e) {
      Log.e(TAG, "closeSession failed: ", e);
    }
    mSessionIds.remove(ByteBuffer.wrap(sessionId));
    Log.d(TAG, String.format("Session %s closed", bytesToHexString(sessionId)));
  }

  @UsedByNative
  MediaCrypto getMediaCrypto() {
    return mMediaCrypto;
  }

  private MediaDrmBridge(UUID schemeUUID, long nativeMediaDrmBridge)
      throws android.media.UnsupportedSchemeException {
    mSchemeUUID = schemeUUID;
    mMediaDrm = new MediaDrm(schemeUUID);

    mNativeMediaDrmBridge = nativeMediaDrmBridge;
    if (!isNativeMediaDrmBridgeValid()) {
      throw new IllegalArgumentException(
          String.format("Invalid nativeMediaDrmBridge value: |%d|.", nativeMediaDrmBridge));
    }

    mMediaDrm.setOnEventListener(
        new OnEventListener() {
          @Override
          public void onEvent(MediaDrm md, byte[] sessionId, int event, int extra, byte[] data) {
            if (sessionId == null) {
              Log.e(TAG, "EventListener: Null session.");
              return;
            }
            if (!sessionExists(sessionId)) {
              Log.e(
                  TAG,
                  String.format("EventListener: Invalid session %s", bytesToHexString(sessionId)));
              return;
            }
            switch (event) {
              case MediaDrm.EVENT_KEY_REQUIRED:
                Log.d(TAG, "MediaDrm.EVENT_KEY_REQUIRED");
                String mime = mSessionIds.get(ByteBuffer.wrap(sessionId));
                MediaDrm.KeyRequest request = null;
                try {
                  request = getKeyRequest(sessionId, data, mime);
                } catch (NotProvisionedException e) {
                  Log.e(TAG, "Device not provisioned", e);
                  if (!attemptProvisioning()) {
                    Log.e(TAG, "Failed to provision device when responding to EVENT_KEY_REQUIRED");
                    return;
                  }
                  // If we supposedly successfully provisioned ourselves, then try to create a
                  // request again.
                  try {
                    request = getKeyRequest(sessionId, data, mime);
                  } catch (NotProvisionedException e2) {
                    Log.e(
                        TAG,
                        "Device still not provisioned after supposedly successful provisioning",
                        e2);
                    return;
                  }
                }
                if (request != null) {
                  onSessionMessage(SB_DRM_TICKET_INVALID, sessionId, request);
                } else {
                  Log.e(TAG, "EventListener: getKeyRequest failed.");
                  return;
                }
                break;
              case MEDIA_DRM_EVENT_KEY_EXPIRED:
                Log.d(TAG, "MediaDrm.EVENT_KEY_EXPIRED");
                break;
              case MediaDrm.EVENT_VENDOR_DEFINED:
                Log.d(TAG, "MediaDrm.EVENT_VENDOR_DEFINED");
                break;
              default:
                Log.e(TAG, "Invalid DRM event " + event);
                return;
            }
          }
        });

    if (Build.VERSION.SDK_INT >= 23) {
      setOnKeyStatusChangeListenerV23();
    }

    mMediaDrm.setPropertyString("privacyMode", "enable");
    mMediaDrm.setPropertyString("sessionSharing", "enable");
  }

  @TargetApi(23)
  private void setOnKeyStatusChangeListenerV23() {
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
  }

  /** Convert byte array to hex string for logging. */
  private static String bytesToHexString(byte[] bytes) {
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

    int requestType = MediaDrm.KeyRequest.REQUEST_TYPE_INITIAL;
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      requestType = request.getRequestType();
    } else {
      // Prior to M, getRequestType() is not supported. Do our best guess here: Assume
      // requests with a URL are renewals and all others are initial requests.
      requestType =
          request.getDefaultUrl().isEmpty()
              ? MediaDrm.KeyRequest.REQUEST_TYPE_INITIAL
              : MediaDrm.KeyRequest.REQUEST_TYPE_RENEWAL;
    }

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
      request =
          mMediaDrm.getKeyRequest(
              sessionId, data, mime, MediaDrm.KEY_TYPE_STREAMING, optionalParameters);
    } catch (IllegalStateException e) {
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP
          && e instanceof android.media.MediaDrm.MediaDrmStateException) {
        Log.e(TAG, "MediaDrmStateException fired during getKeyRequest().", e);
      }
    }

    String result = (request != null) ? "succeeded" : "failed";
    Log.d(TAG, String.format("getKeyRequest %s!", result));

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
    if (mMediaCryptoSession != null) {
      throw new IllegalStateException(
          "Cannot create media crypto with non-null mMediaCryptoSession.");
    }
    // TODO: Cannot do this during provisioning pending.

    // Open media crypto session.
    try {
      mMediaCryptoSession = openSession();
    } catch (NotProvisionedException e) {
      Log.d(TAG, "Device not provisioned", e);
      if (!attemptProvisioning()) {
        Log.e(TAG, "Failed to provision device during MediaCrypto creation.");
        return false;
      }
      return true;
    }

    if (mMediaCryptoSession == null) {
      Log.e(TAG, "Cannot create MediaCrypto Session.");
      return false;
    }
    Log.d(
        TAG,
        String.format("MediaCrypto Session created: %s", bytesToHexString(mMediaCryptoSession)));

    // Create MediaCrypto object.
    try {
      if (MediaCrypto.isCryptoSchemeSupported(mSchemeUUID)) {
        MediaCrypto mediaCrypto = new MediaCrypto(mSchemeUUID, mMediaCryptoSession);
        Log.d(TAG, "MediaCrypto successfully created!");
        mMediaCrypto = mediaCrypto;
        return true;
      } else {
        Log.e(TAG, "Cannot create MediaCrypto for unsupported scheme.");
      }
    } catch (MediaCryptoException e) {
      Log.e(TAG, "Cannot create MediaCrypto", e);
    }

    try {
      // Some implementations let this method throw exceptions.
      mMediaDrm.closeSession(mMediaCryptoSession);
    } catch (Exception e) {
      Log.e(TAG, "closeSession failed: ", e);
    }
    mMediaCryptoSession = null;

    return false;
  }

  /**
   * Open a new session.
   *
   * @return ID of the session opened. Returns null if unexpected error happened.
   */
  private byte[] openSession() throws android.media.NotProvisionedException {
    Log.d(TAG, "openSession()");
    if (mMediaDrm == null) {
      throw new IllegalStateException("mMediaDrm cannot be null in openSession");
    }
    try {
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

  /**
   * Attempt to get the device that we are currently running on provisioned.
   *
   * @return whether provisioning was successful or not.
   */
  private boolean attemptProvisioning() {
    Log.d(TAG, "attemptProvisioning()");
    MediaDrm.ProvisionRequest request = mMediaDrm.getProvisionRequest();
    String url = request.getDefaultUrl() + "&signedRequest=" + new String(request.getData());
    byte[] response = new CobaltHttpHelper().performDrmHttpPost(url);
    if (response == null) {
      return false;
    }
    try {
      mMediaDrm.provideProvisionResponse(response);
      return true;
    } catch (android.media.DeniedByServerException e) {
      Log.e(TAG, "failed to provide provision response", e);
    } catch (java.lang.IllegalStateException e) {
      Log.e(TAG, "failed to provide provision response", e);
    }
    return false;
  }

  /**
   * Check whether |sessionId| is an existing session ID, excluding the media crypto session.
   *
   * @param sessionId Crypto session Id.
   * @return true if |sessionId| exists, false otherwise.
   */
  private boolean sessionExists(byte[] sessionId) {
    if (mMediaCryptoSession == null) {
      if (!mSessionIds.isEmpty()) {
        throw new IllegalStateException(
            "mSessionIds must be empty if crypto session does not exist.");
      }
      Log.e(TAG, "Session doesn't exist because media crypto session is not created.");
      return false;
    }
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
        mMediaDrm.removeKeys(sessionId.array());
      } catch (Exception e) {
        Log.e(TAG, "removeKeys failed: ", e);
      }

      try {
        // Some implementations let this method throw exceptions.
        mMediaDrm.closeSession(sessionId.array());
      } catch (Exception e) {
        Log.e(TAG, "closeSession failed: ", e);
      }
      Log.d(
          TAG,
          String.format("Successfully closed session (%s)", bytesToHexString(sessionId.array())));
    }
    mSessionIds.clear();
    mSessionIds = null;

    // Close mMediaCryptoSession if it's open.
    if (mMediaCryptoSession != null) {
      try {
        // Some implementations let this method throw exceptions.
        mMediaDrm.closeSession(mMediaCryptoSession);
      } catch (Exception e) {
        Log.e(TAG, "closeSession failed: ", e);
      }
      mMediaCryptoSession = null;
    }

    if (mMediaDrm != null) {
      mMediaDrm.release();
      mMediaDrm = null;
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
