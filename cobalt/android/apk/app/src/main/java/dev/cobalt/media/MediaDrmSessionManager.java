// Copyright 2025 The Chromium Authors. All rights reserved.
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

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Set;
import java.util.stream.Collectors;

class MediaDrmSessionManager {
  private static final String TAG = "CobaltDrm";

  static class Session {
    private static final char[] HEX_CHAR_LOOKUP = "0123456789ABCDEF".toCharArray();

    private static int mSessionIdCount = 0;
    private static synchronized byte[] generateEmeSessiondId() {
      return ("cobalt.eme." + mSessionIdCount++).getBytes();
    }

    private final byte[] emeId;
    private final String mimeType;
    private byte[] mediaDrmId;

    private Session(byte[] emeId, String mimeType) {
      this.emeId = emeId;
      this.mimeType = mimeType;
    }

    private static Session create(String mimeType) {
      return new Session(generateEmeSessiondId(), mimeType);
    }

    public byte[] getEmeId() {
      return emeId;
    }

    public byte[] getMediaDrmId() {
      return mediaDrmId;
    }

    public String getMimeType() {
      return mimeType;
    }

    public void setMediaDrmId(byte[] mediaDrmId) {
      this.mediaDrmId = mediaDrmId;
    }

    @Override
    public String toString() {
      return "{"
          + "emeId=" + bytesToString(emeId)
          + ", mimeType=" + mimeType
          + ", mediaDrmId=" + bytesToString(mediaDrmId)
          + '}';
    }

    public static String bytesToString(byte[] bytes) {
      if (bytes == null) {
        return "(null)";
      }
      try {
        return StandardCharsets.UTF_8.newDecoder().decode(ByteBuffer.wrap(bytes)).toString();
      } catch (Exception e) {
        return "hex(" + bytesToHexString(bytes) + ")";
      }
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
  }

  // The map of all opened sessions (excluding mMediaCryptoSession) to their
  // mime types.
  private final HashMap<ByteBuffer, Session> mEmeIdSessionMap = new HashMap<>();

  Session createSession(String mimeType) {
    Session session = Session.create(mimeType);
    mEmeIdSessionMap.put(ByteBuffer.wrap(session.getEmeId()), session);
    return session;
  }

  void remove(Session sessionId) {
    mEmeIdSessionMap.remove(ByteBuffer.wrap(sessionId.getEmeId()));
  }

  Session findForEmeSessionId(byte[] emeSessionId) {
    return mEmeIdSessionMap.get(ByteBuffer.wrap(emeSessionId));
  }

  Session findForMediaDrmSessionId(byte[] mediaDrmSessionId) {
    for (Session session : mEmeIdSessionMap.values()) {
      if (Arrays.equals(session.getMediaDrmId(), mediaDrmSessionId)) {
        return session;
      }
    }
    return null;
  }

  Set<byte[]> getMediaDrmIds() {
    return mEmeIdSessionMap.values().stream()
        .map(Session::getMediaDrmId)
        .collect(Collectors.toSet());
  }

  void clearSessions() {
    mEmeIdSessionMap.clear();
  }
}
