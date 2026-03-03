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

import androidx.media3.common.C;
import java.nio.ByteBuffer;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Encapsulates a media sample and its metadata.
 */
@JNINamespace("starboard")
public final class ExoPlayerMediaSample {
  private final ByteBuffer mSamples;
  private final int mSize;
  private final long mTimestampUsec;
  private final int mType;
  private final int mFlags;

  @CalledByNative
  public ExoPlayerMediaSample(
      ByteBuffer samples,
      int size,
      long timestamp,
      boolean isKeyFrame,
      int type) {
    this.mSamples = samples;
    this.mSize = size;
    this.mTimestampUsec = timestamp;
    this.mType = type;

    int flags = 0;
    if (isKeyFrame) {
      flags |= C.BUFFER_FLAG_KEY_FRAME;
    }
    this.mFlags = flags;
  }

  public ByteBuffer getSamples() {
    return mSamples;
  }

  public int getSize() {
    return mSize;
  }

  public long getTimestampUsec() {
    return mTimestampUsec;
  }

  public int getType() {
    return mType;
  }

  public int getFlags() {
    return mFlags;
  }
}
