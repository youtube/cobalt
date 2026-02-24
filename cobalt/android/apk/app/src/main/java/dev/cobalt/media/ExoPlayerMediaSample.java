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
import androidx.media3.common.util.UnstableApi;
import androidx.media3.extractor.TrackOutput.CryptoData;
import java.nio.ByteBuffer;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Encapsulates a media sample and its metadata, including encryption information.
 */
@JNINamespace("starboard")
@UnstableApi
public final class ExoPlayerMediaSample {
  private final ByteBuffer mSamples;
  private final int mSize;
  private final long mTimestamp;
  private final boolean mIsKeyFrame;
  private final int mType;
  private final int mEncryptionMode;
  private final byte[] mKey;
  private final int mEncryptedBlocks;
  private final int mClearBlocks;
  private final byte[] mInitializationVector;
  private final int mIvSize;
  private final int[] mSubsampleEncryptedBytes;
  private final int[] mSubsampleClearBytes;

  private final CryptoData mCryptoData;
  private final int mFlags;
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
    this.mTimestamp = timestamp;
    this.mIsKeyFrame = isKeyFrame;
    this.mType = type;
    this.mEncryptionMode = encryptionMode;
    this.mKey = key;
    this.mEncryptedBlocks = encryptedBlocks;
    this.mClearBlocks = clearBlocks;
    this.mInitializationVector = initializationVector;
    this.mIvSize = ivSize;
    this.mSubsampleEncryptedBytes = subsampleEncryptedBytes;
    this.mSubsampleClearBytes = subsampleClearBytes;

    this.mHasSubsamples =
        subsampleEncryptedBytes != null
            && subsampleClearBytes != null
            && subsampleEncryptedBytes.length > 0;

    int flags = 0;
    if (isKeyFrame) {
      flags |= C.BUFFER_FLAG_KEY_FRAME;
    }

    if (key != null) {
      flags |= C.BUFFER_FLAG_ENCRYPTED;
      this.mCryptoData = new CryptoData(encryptionMode, key, encryptedBlocks, clearBlocks);

      this.mEncryptionSignalByte = new ParsableByteArray(1);
      this.mEncryptionSignalByte.getData()[0] = (byte) (ivSize | (mHasSubsamples ? 0x80 : 0));

      this.mIvData = new ParsableByteArray(initializationVector, ivSize);

      if (mHasSubsamples) {
        int subsampleCount = subsampleEncryptedBytes.length;
        int subsampleDataLength = 2 + 6 * subsampleCount;
        ByteBuffer subsampleBuffer = ByteBuffer.allocate(subsampleDataLength);
        subsampleBuffer.putShort((short) subsampleCount);
        for (int i = 0; i < subsampleCount; i++) {
          subsampleBuffer.putShort((short) subsampleClearBytes[i]);
          subsampleBuffer.putInt(subsampleEncryptedBytes[i]);
        }
        this.mSubsampleData = new ParsableByteArray(subsampleBuffer.array());
      } else {
        this.mSubsampleData = null;
      }
    } else {
      this.mCryptoData = null;
      this.mEncryptionSignalByte = null;
      this.mIvData = null;
      this.mSubsampleData = null;
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

  public long getTimestamp() {
    return mTimestamp;
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

  /** Returns the sample flags, including keyframe and encryption flags. */
  public int getFlags() {
    return mFlags;
  }

  /** Returns the total size of the sample, including encryption preamble data. */
  public int getTotalSize() {
    return mTotalSize;
  }

  /** Returns the {@link CryptoData} for this sample, or null if it is not encrypted. */
  @Nullable
  public CryptoData getCryptoData() {
    return mCryptoData;
  }

  /** Returns a {@link ParsableByteArray} containing the encryption signal byte. */
  @Nullable
  public ParsableByteArray getEncryptionSignalByte() {
    return mEncryptionSignalByte;
  }

  /** Returns a {@link ParsableByteArray} containing the initialization vector. */
  @Nullable
  public ParsableByteArray getIvData() {
    return mIvData;
  }

  /**
   * Returns a {@link ParsableByteArray} containing the subsample count and subsample data,
   * or null if there are no subsamples.
   */
  @Nullable
  public ParsableByteArray getSubsampleData() {
    return mSubsampleData;
  }
}
