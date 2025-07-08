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
import dev.cobalt.media.MediaDrmSessionManager.Session;
import dev.cobalt.util.Log;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;


/** A wrapper of the android MediaDrm class. */
@JNINamespace("starboard::android::shared")
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

  private static final long INVALID_NATIVE_MEDIA_DRM_BRIDGE = 0;

  // The value of this must stay in sync with kSbDrmTicketInvalid in "starboard/drm.h"
  private static final int SB_DRM_TICKET_INVALID = Integer.MIN_VALUE;

  // Scheme UUID for Widevine. See http://dashif.org/identifiers/protection/
  private static final UUID WIDEVINE_UUID = UUID.fromString("edef8ba9-79d6-4ace-a3c8-27dcd51d21ed");

  private static final boolean HANDLE_PENDING_LICENSE_REQUESTS = true;

  // Deprecated in API 26, but we still log it on earlier devices.
  // We do handle STATUS_EXPIRED in onKeyStatusChange() for API 23+ devices.
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
  private final MediaDrmSessionManager mSessionManager = new MediaDrmSessionManager();

  private MediaCrypto mMediaCrypto;

  // Return value type for calls to updateSession(), which contains whether or not the call
  // succeeded, and optionally an error message (that is empty on success).
  private static class UpdateSessionResult {
    public enum Status {
      SUCCESS,
      FAILURE
    }

    // Whether or not the update session attempt succeeded or failed.
    private boolean mIsSuccess;

    // Descriptive error message or details, in the scenario where the update session call failed.
    private String mErrorMessage;

    private UpdateSessionResult(Status status, String errorMessage) {
      this.mIsSuccess = status == Status.SUCCESS;
      this.mErrorMessage = errorMessage;
    }

    public static UpdateSessionResult success() {
      return new UpdateSessionResult(Status.SUCCESS, "");
    }

    public static UpdateSessionResult failure(String errorMessage, Throwable e) {
      return new UpdateSessionResult(
          Status.FAILURE,
          errorMessage + " StackTrace: " + android.util.Log.getStackTraceString(e));
    }

    @CalledByNative("UpdateSessionResult")
    public boolean isSuccess() {
      return mIsSuccess;
    }

    @CalledByNative("UpdateSessionResult")
    public String getErrorMessage() {
      return mErrorMessage;
    }
  }

  /**
   * Create a new MediaDrmBridge with the Widevine crypto scheme.
   *
   * @param nativeMediaDrmBridge The native owner of this class.
   */
  @CalledByNative
  static MediaDrmBridge create(String keySystem, long nativeMediaDrmBridge) {
    UUID cryptoScheme = WIDEVINE_UUID;
    if (!MediaDrm.isCryptoSchemeSupported(cryptoScheme)) {
      return null;
    }

    MediaDrmBridge mediaDrmBridge = null;
    try {
      mediaDrmBridge = new MediaDrmBridge(keySystem, cryptoScheme, nativeMediaDrmBridge);
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
  @CalledByNative
  static boolean isWidevineCryptoSchemeSupported() {
    return MediaDrm.isCryptoSchemeSupported(WIDEVINE_UUID);
  }

  /**
   * Check whether `cbcs` scheme is supported.
   *
   * @return true if the `cbcs` encryption is supported, or false otherwise.
   */
  @CalledByNative
  static boolean isCbcsSchemeSupported() {
    // While 'cbcs' scheme was originally implemented in N, there was a bug (in the
    // DRM code) which means that it didn't really work properly until N-MR1).
    return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N_MR1;
  }

  /** Destroy the MediaDrmBridge object. */
  @CalledByNative
  void destroy() {
    mNativeMediaDrmBridge = INVALID_NATIVE_MEDIA_DRM_BRIDGE;
    if (mMediaDrm != null) {
      release();
    }
  }

  class LicenseRequestArgs {
    public final Session session;
    public final int ticket;
    public final byte[] initData;

    public LicenseRequestArgs(Session session, int ticket, byte[] initData) {
      this.session = session;
      this.ticket = ticket;
      this.initData = initData;
    }
  }

  class PendingLicenseRequestArgs extends LicenseRequestArgs {
    public PendingLicenseRequestArgs(LicenseRequestArgs args) {
      super(args.session, SB_DRM_TICKET_INVALID, args.initData);
    }
  }

  ArrayList<PendingLicenseRequestArgs> pendingLicenseRequests = new ArrayList<>();

  boolean handlePengindLicenseRequest(PendingLicenseRequestArgs args) {
    try {
      createSessionInternal(args);
    } catch (NotProvisionedException e) {
      Log.e(TAG, "Met Provisioning error while handling pending license request. Try again.");
      startProvisioning(args.session, SB_DRM_TICKET_INVALID);
      return false;
    }

    Log.e(TAG, "Handled pending license request successfully.");
    return true;
  }

  @CalledByNative
  void runPendingTasks() {
    if (!HANDLE_PENDING_LICENSE_REQUESTS) {
      Log.i(TAG, "Does not handle pending license requests, since app will re-generate the license request.");
      return;
    }

    Log.i(TAG, "runPendingTasks(): pendingLicenseRequests.size()=" + pendingLicenseRequests.size());

    while (pendingLicenseRequests.size() > 0) {
      PendingLicenseRequestArgs args = pendingLicenseRequests.remove(0);
      Log.i(TAG, "Handling pending license request.");
      if (!handlePengindLicenseRequest(args)) {
        pendingLicenseRequests.add(0, args);
      }
      break;
    }
  }

  @CalledByNative
  void createSession(int ticket, byte[] initData, String mime) {
    Log.d(TAG, "createSession()");
    assert mMediaDrm != null;
    Session session = mSessionManager.createSession(mime);
    LicenseRequestArgs args = new LicenseRequestArgs(session, ticket, initData);

    try {
      createSessionInternal(args);
    } catch(NotProvisionedException e) {
      Log.i(TAG, "Device not provisioned. Start provisioning. session=" + session);

      pendingLicenseRequests.add(new PendingLicenseRequestArgs(args));
      startProvisioning(session, ticket);
      return;
    }
  }

  private void createSessionInternal(LicenseRequestArgs args) throws android.media.NotProvisionedException {
    createMediaCryptoSession();

    Session session = args.session;
    int ticket = args.ticket;

    byte[] mediaDrmSessionId;
    try {
      mediaDrmSessionId = openSession();
    } catch (NotProvisionedException e) {
      Log.e(TAG, "Device not provisioned", e);
      return;
    }

    MediaDrm.KeyRequest request = getKeyRequest(mediaDrmSessionId, args.initData, session.getMimeType());
    if (request == null) {
      closeMediaDrmSession(mediaDrmSessionId);
      Log.e(TAG, "Generate request failed.");
      return;
    }

    session.setMediaDrmId(mediaDrmSessionId);

    // Success!
    Log.d(TAG, "MediaDrm session is created: session=" + session);
    onSessionMessage(args.ticket, session, request);
  }

  /**
   * Update a session with response.
   *
   * @param sessionId Reference ID of session to be updated.
   * @param response Response data from the server.
   */
  @CalledByNative
  UpdateSessionResult updateSession(int ticket, byte[] emeSessionId, byte[] response) {
    assert mMediaDrm != null;
    Session session = mSessionManager.findForEmeSessionId(emeSessionId);
    if (session == null) {
      return UpdateSessionResult.Failure(
          "Failed to update session because no active session exists: emeSessionId=" + Session.bytesToString(emeSessionId),
          new Throwable());
    }
    byte[] mediaDrmSessionId = session.getMediaDrmId();
    if (mediaDrmSessionId == null) {
      Log.i(TAG, "MediaDrm session is not created. Use response data as provision response.");
      return handleProvisionResponse(response);
    }

    Log.d(TAG, "updateSession(): session=" + session);

    try {
      try {
        Log.i(TAG, "Calling mMediaDrm.provideKeyResponse: session=" + session
              + ", response(bytes)=" + response.length);
        mMediaDrm.provideKeyResponse(mediaDrmSessionId, response);
      } catch (IllegalStateException e) {
        // This is not really an exception. Some error codes are incorrectly
        // reported as an exception.
        Log.e(TAG, "Exception intentionally caught when calling provideKeyResponse()", e);
      }
      Log.d(TAG, "Key successfully added for session=" + session);
      return UpdateSessionResult.Success();
    } catch (NotProvisionedException e) {
      // TODO: Should we handle this?
      Log.e(TAG, "Failed to provide key response", e);
      release();
      return UpdateSessionResult.failure(
          "Update session failed due to lack of provisioning.", e);
    } catch (DeniedByServerException e) {
      Log.e(TAG, "Failed to provide key response.", e);
      release();
      return UpdateSessionResult.failure(
          "Update session failed because we were denied by server.", e);
    } catch (Exception e) {
      Log.e(TAG, "", e);
      release();
      return UpdateSessionResult.failure(
          "Update session failed. Caught exception: " + e.getMessage(), e);
    }
  }

  /**
   * Close a session that was previously created by createSession().
   *
   * @param emeSessionId ID of session to be closed.
   */
  @CalledByNative
  void closeSession(byte[] emeSessionId) {
    Log.d(TAG, "closeSession()");
    if (mMediaDrm == null) {
      Log.e(TAG, "closeSession() called when MediaDrm is null.");
      return;
    }
    Session session = mSessionManager.findForEmeSessionId(emeSessionId);
    if (session == null) {
      Log.e(TAG, "closeSession() failed, since there is not active session: emeSessionId=" + Session.bytesToString(emeSessionId));
      return;
    }

    byte[] mediaDrmSessionId = session.getMediaDrmId();
    if (mediaDrmSessionId != null) {
      try {
        // Some implementations don't have removeKeys.
        // https://bugs.chromium.org/p/chromium/issues/detail?id=475632
        mMediaDrm.removeKeys(mediaDrmSessionId);
      } catch (Exception e) {
        Log.e(TAG, "removeKeys failed: ", e);
      }

      closeMediaDrmSession(mediaDrmSessionId);
    }
    mSessionManager.remove(session);

    Log.d(TAG, "Session closed: session=" + session);
  }

  @CalledByNative
  byte[] getMetricsInBase64() {
    if (Build.VERSION.SDK_INT < 28) {
      return null;
    }
    byte[] metrics;
    try {
      metrics = mMediaDrm.getPropertyByteArray("metrics");
    } catch (Exception e) {
      Log.e(TAG, "Failed to retrieve DRM Metrics.");
      return null;
    }
    return Base64.encode(metrics, Base64.NO_PADDING | Base64.NO_WRAP | Base64.URL_SAFE);
  }

  @CalledByNative
  MediaCrypto getMediaCrypto() {
    return mMediaCrypto;
  }

  private MediaDrmBridge(String keySystem, UUID schemeUUID, long nativeMediaDrmBridge)
      throws android.media.UnsupportedSchemeException {
    mSchemeUUID = schemeUUID;
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

    mMediaDrm.setOnEventListener(
        new OnEventListener() {
          @Override
          public void onEvent(MediaDrm md, byte[] mediaDrmSessionId, int event, int extra, byte[] data) {
            if (event == MediaDrm.EVENT_KEY_REQUIRED) {
              Log.d(TAG, "MediaDrm.EVENT_KEY_REQUIRED");
              Session session = mSessionManager.findForMediaDrmSessionId(mediaDrmSessionId);
              if (session == null) {
                Log.e(TAG, "EventListener:EVENT_KEY_REQUIRED: Invalid media drm session id=" + Session.bytesToString(mediaDrmSessionId));
                return;
              }

              MediaDrm.KeyRequest request = null;
              try {
                request = getKeyRequest(mediaDrmSessionId, data, session.getMimeType());
              } catch (NotProvisionedException e) {
                Log.e(TAG, "EventListener: getKeyRequest failed.", e);
                return;
              }

              onSessionMessage(SB_DRM_TICKET_INVALID, session, request);
            } else if (event == MEDIA_DRM_EVENT_KEY_EXPIRED) {
              Log.d(TAG, "MediaDrm.EVENT_KEY_EXPIRED");
            } else if (event == MediaDrm.EVENT_VENDOR_DEFINED) {
              Log.d(TAG, "MediaDrm.EVENT_VENDOR_DEFINED");
            } else if (event == MEDIA_DRM_EVENT_PROVISION_REQUIRED) {
              // Log.d(TAG, "MediaDrm.EVENT_PROVISION_REQUIRED");
            } else if (event == MEDIA_DRM_EVENT_SESSION_RECLAIMED) {
              Log.d(TAG, "MediaDrm.EVENT_SESSION_RECLAIMED");
            } else {
              Log.e(TAG, "Invalid DRM event " + event);
              return;
            }
          }
        });

    mMediaDrm.setOnKeyStatusChangeListener(
        new MediaDrm.OnKeyStatusChangeListener() {
          @Override
          public void onKeyStatusChange(
              MediaDrm md,
              byte[] mediaDrmSessionId,
              List<MediaDrm.KeyStatus> keyInformation,
              boolean hasNewUsableKey) {

            Session session = mSessionManager.findForMediaDrmSessionId(mediaDrmSessionId);
            if (session == null) {
              Log.i(TAG, "onKeyStatusChange is ignored, since there is no active session: mediaDrmSessionId=" + Session.bytesToString(mediaDrmSessionId));
              return;
            }

            nativeOnKeyStatusChange(
                mNativeMediaDrmBridge,
                session.getEmeId(),
                keyInformation.toArray(new MediaDrm.KeyStatus[keyInformation.size()]));
          }
        },
        null);

    mMediaDrm.setPropertyString("privacyMode", "disable");
    mMediaDrm.setPropertyString("sessionSharing", "enable");
    if (keySystem.equals("com.youtube.widevine.l3")
        && !mMediaDrm.getPropertyString("securityLevel").equals("L3")) {
        mMediaDrm.setPropertyString("securityLevel", "L3");
    }
  }

  private void onSessionMessage(
      int ticket, Session session, final MediaDrm.KeyRequest request) {
    if (!isNativeMediaDrmBridgeValid()) {
      return;
    }

    int requestType = request.getRequestType();
    nativeOnSessionMessage(
        mNativeMediaDrmBridge, ticket, session.getEmeId(), requestType, request.getData());
  }

  /**
   * Get a key request.
   *
   * @param sessionId ID of session on which we need to get the key request.
   * @param data Data needed to get the key request.
   * @param mime Mime type to get the key request.
   * @return the key request.
   */
  private MediaDrm.KeyRequest getKeyRequest(byte[] mediaDrmSessionId, byte[] data, String mime)
      throws android.media.NotProvisionedException {
    if (mMediaDrm == null) {
      throw new IllegalStateException("mMediaDrm cannot be null in getKeyRequest");
    }
    if (mMediaCryptoSession == null) {
      throw new IllegalStateException("mMediaCryptoSession cannot be null in getKeyRequest.");
    }
    // TODO: Cannot do this during provisioning pending.

    MediaDrm.KeyRequest request;
    try {
      request =
          mMediaDrm.getKeyRequest(
              mediaDrmSessionId, data, mime, MediaDrm.KEY_TYPE_STREAMING, /*optionalParameters*/null);
    } catch (IllegalStateException e) {
      Log.e(TAG, "Exception is fired during getKeyRequest().", e);
      return null;
    }

    Log.d(TAG, "getKeyRequest succeeded");
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
        MediaCrypto mediaCrypto = new MediaCrypto(mSchemeUUID, new byte[0]);
        Log.d(TAG, "MediaCrypto successfully created!");
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
    Log.d(TAG, "openSession()");
    if (mMediaDrm == null) {
      Log.e(TAG, "mMediaDrm cannot be null in openSession");
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

  private void closeMediaDrmSession(byte[] mediaDrmSessionId) {
    try {
      // Some implementations let this method throw exceptions.
      mMediaDrm.closeSession(mediaDrmSessionId);
    } catch (Exception e) {
      Log.e(TAG, "closeSession(sessionId=" + Session.bytesToString(mediaDrmSessionId) + ") failed: ", e);
    }
  }

  void createMediaCryptoSession() throws NotProvisionedException {
    Log.i(TAG, "createMediaCryptoSession()");
    if (mMediaCryptoSession != null) {
      Log.i(TAG, "MediaCryptoSession is already created");
      return;
    }

    try {
      mMediaCryptoSession = openSession();
    } catch (NotProvisionedException e) {
      Log.w(TAG, "createMediaCryptoSesion met: Device not provisioned. Re-throw NotProvisionedException");
      throw e;
    }
    assert mMediaCryptoSession != null;

    try {
      Log.i(TAG, "Calling MediaCrypto.setMediaDrmSession(mMediaCryptoSession)");
      mMediaCrypto.setMediaDrmSession(mMediaCryptoSession);
    } catch (MediaCryptoException e3) {
      Log.e(TAG, "Unable to set media drm session", e3);
      closeMediaDrmSession(mMediaCryptoSession);
      mMediaCryptoSession = null;
      return;
    }

    Log.i(TAG, "MediaCrypto Session created: sessionId=" + Session.bytesToString(mMediaCryptoSession));
  }

  private static final int INDIVIDUALIZATION_REQUEST_TYPE = 3;

  /**
   * Attempt to get the device that we are currently running on provisioned.
   *
   * @return whether provisioning was successful or not.
   */
  // Provisioning should be singletone activity.
  private void startProvisioning(Session session, int ticket) {
    MediaDrm.ProvisionRequest request = mMediaDrm.getProvisionRequest();
    Log.i(TAG, "start provisioning: request size=" + request.getData().length);

    nativeOnSessionMessage(
        mNativeMediaDrmBridge,
        ticket, session.getEmeId(),
        INDIVIDUALIZATION_REQUEST_TYPE, request.getData());
  }

  private UpdateSessionResult handleProvisionResponse(byte[] response) {
    Log.i(TAG, "handleProvisionResponse: size=" + response.length);

    try {
      mMediaDrm.provideProvisionResponse(response);
    } catch (android.media.DeniedByServerException e) {
      return new UpdateSessionResult(
          UpdateSessionResult.Status.FAILURE,
          e.getMessage());
    }
    Log.i(TAG, "provideProvisionResponse succeeded");

    return new UpdateSessionResult(UpdateSessionResult.Status.SUCCESS, "");
  }

  /** Release all allocated resources and finish all pending operations. */
  private void release() {
    Log.i(TAG, "release()");
    // Note that mNativeMediaDrmBridge may have already been reset (see destroy()).
    if (mMediaDrm == null) {
      throw new IllegalStateException("Called release with null mMediaDrm.");
    }

    // Close all open sessions.
    for (byte[] mediaDrmSessionId : mSessionManager.getMediaDrmIds()) {
      Log.i(TAG, "Closing session: mediaDrmSessionId=" + Session.bytesToString(mediaDrmSessionId));
      try {
        // Some implementations don't have removeKeys.
        // https://bugs.chromium.org/p/chromium/issues/detail?id=475632
        mMediaDrm.removeKeys(mediaDrmSessionId);
      } catch (Exception e) {
        Log.e(TAG, "removeKeys failed: ", e);
      }

      closeMediaDrmSession(mediaDrmSessionId);
      Log.d(TAG, "Successfully closed session: mediaDrmSessionId=", Session.bytesToString(mediaDrmSessionId));
    }
    mSessionManager.clearSessions();

    // Close mMediaCryptoSession if it's open.
    if (mMediaCryptoSession != null) {
      closeMediaDrmSession(mMediaCryptoSession);
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
    mediaDrm.release();
  }

  @RequiresApi(28)
  private void closeMediaDrmV28(MediaDrm mediaDrm) {
    mediaDrm.close();
  }

  @RequiresApi(29)
  private void getConnectedHdcpLevelInfoV29(MediaDrm mediaDrm) {
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

  @NativeMethods
  interface Natives {
    void onSessionMessage(
        long nativeMediaDrmBridge,
        int ticket,
        byte[] sessionId,
        int requestType,
        byte[] message);

    void onKeyStatusChange(
        long nativeMediaDrmBridge,
        byte[] sessionId,
        MediaDrm.KeyStatus[] keyInformation);
  }
}
