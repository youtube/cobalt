/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import org.jni_zero.NativeMethods;

/**
 * Java wrapper for Logging::LogSink which is used to output logs to a call session file rotating
 * log sink.
 */
public class CallSessionFileRotatingLogSink {
  private long nativeSink;

  public static byte[] getLogData(String dirPath) {
    if (dirPath == null) {
      throw new IllegalArgumentException("dirPath may not be null.");
    }
    return CallSessionFileRotatingLogSinkJni.get().getLogData(dirPath);
  }

  @SuppressWarnings("EnumOrdinal")
  public CallSessionFileRotatingLogSink(
      String dirPath, int maxFileSize, Logging.Severity severity) {
    if (dirPath == null) {
      throw new IllegalArgumentException("dirPath may not be null.");
    }
    nativeSink =
        CallSessionFileRotatingLogSinkJni.get().addSink(dirPath, maxFileSize, severity.ordinal());
  }

  public void dispose() {
    if (nativeSink != 0) {
      CallSessionFileRotatingLogSinkJni.get().deleteSink(nativeSink);
      nativeSink = 0;
    }
  }

  @NativeMethods
  interface Natives {
    long addSink(String dirPath, int maxFileSize, int severity);

    void deleteSink(long sink);

    byte[] getLogData(String dirPath);
  }
}
