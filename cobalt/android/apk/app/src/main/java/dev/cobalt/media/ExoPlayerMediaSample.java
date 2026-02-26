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

import androidx.annotation.Nullable;
import androidx.media3.common.C;
import androidx.media3.common.util.ParsableByteArray;
import androidx.media3.extractor.TrackOutput.CryptoData;
import java.nio.ByteBuffer;
import java.util.Arrays;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Encapsulates a media sample and its metadata, including encryption information.
 */
@JNINamespace("starboard")
public final class ExoPlayerMediaSample {
  private final ByteBuffer mSamples;
  // The size of the media data written by the native ExoPlayerBridge.
  private final int mSize;
  private final long mTimestampUsec;
  private final int mType;
  private final byte[] mKey;
  private final CryptoData mCryptoData;
  private final int mFlags;
  // The total size of the data written to the ExoPlayerSampleStream after adding encryption data.
  private final int mTotalSize;
  private final boolean mHasSubsamples;
  private final ParsableByteArray mEncryptionSignalByte;
  private final ParsableByteArray mIvData;
  private final ParsableByteArray mSubsampleData;

  @CalledByNative
  public ExoPlayerMediaSample(
      ByteBuffer samples,
      int size,
      long timestamp,
      boolean isKeyFrame,
      int type,
      int encryptionMode,
      @Nullable byte[] key,
      int encryptedBlocks,
      int clearBlocks,
      @Nullable byte[] initializationVector,
      int ivSize,
      @Nullable int[] subsampleEncryptedBytes,
      @Nullable int[] subsampleClearBytes) {
    this.mSamples = samples;
    this.mSize = size;
    this.mTimestampUsec = timestamp;
    this.mType = type;
    this.mKey = key;
    this.mHasSubsamples =
        subsampleEncryptedBytes != null
            && subsampleClearBytes != null
            && subsampleEncryptedBytes.length > 0;

    if (key != null) {
      this.mCryptoData = new CryptoData(encryptionMode, key, encryptedBlocks, clearBlocks);

      // Create a unique buffer for the encryption signal byte.
      // This byte contains the IV size and a flag indicating if subsamples are present.
      byte[] signalByte = new byte[1];
      // Top bit (0x80) signals presence of subsamples; remaining bits are the IV size.
      signalByte[0] = (byte) (ivSize | (mHasSubsamples ? 0x80 : 0));
      this.mEncryptionSignalByte = new ParsableByteArray(signalByte);

      this.mIvData = new ParsableByteArray(Arrays.copyOf(initializationVector, ivSize));

      if (mHasSubsamples) {
        int subsampleCount = subsampleEncryptedBytes.length;
        // 2 bytes for count + 6 bytes per subsample (2 for clear, 4 for encrypted).
        int subsampleDataLength = 2 + 6 * subsampleCount;
        byte[] subsampleData = new byte[subsampleDataLength];
        ByteBuffer buffer = ByteBuffer.wrap(subsampleData);

        buffer.putShort((short) subsampleCount);
        for (int i = 0; i < subsampleCount; i++) {
          buffer.putShort((short) subsampleClearBytes[i]);
          buffer.putInt(subsampleEncryptedBytes[i]);
        }
        this.mSubsampleData = new ParsableByteArray(subsampleData);
      } else {
        this.mSubsampleData = null;
      }
    } else {
      this.mCryptoData = null;
      this.mEncryptionSignalByte = null;
      this.mIvData = null;
      this.mSubsampleData = null;
    }

    int flags = 0;
    if (isKeyFrame) {
      flags |= C.BUFFER_FLAG_KEY_FRAME;
    }
    if (key != null) {
      flags |= C.BUFFER_FLAG_ENCRYPTED;
    }
    this.mFlags = flags;

    int totalSize = size;
    if (key != null) {
      totalSize++; // signal byte
      totalSize += ivSize;
      if (mHasSubsamples) {
        totalSize += 2; // subsample count
        totalSize += 6 * subsampleEncryptedBytes.length;
      }
    }
    this.mTotalSize = totalSize;
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

  public boolean isEncrypted() {
    return mKey != null;
  }

  public boolean hasSubsamples() {
    return mHasSubsamples;
  }

  public int getFlags() {
    return mFlags;
  }

  public int getTotalSize() {
    return mTotalSize;
  }

  @Nullable
  public CryptoData getCryptoData() {
    return mCryptoData;
  }

  @Nullable
  public ParsableByteArray getEncryptionSignalByte() {
    return mEncryptionSignalByte;
  }

  @Nullable
  public ParsableByteArray getIvData() {
    return mIvData;
  }

  @Nullable
  public ParsableByteArray getSubsampleData() {
    return mSubsampleData;
  }
}
