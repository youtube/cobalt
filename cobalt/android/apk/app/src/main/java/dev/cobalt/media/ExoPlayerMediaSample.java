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

  // Reusable ParsableByteArray instances to avoid frequent memory allocations on the hot path
  // (e.g., during 60fps playback). These are stored in ThreadLocal to ensure thread safety
  // while allowing memory reuse for encryption-related metadata.
  private static final ThreadLocal<ParsableByteArray> sEncryptionSignalByte = new ThreadLocal<>();
  private static final ThreadLocal<ParsableByteArray> sIvData = new ThreadLocal<>();
  private static final ThreadLocal<ParsableByteArray> sSubsampleData = new ThreadLocal<>();
  private static final ThreadLocal<ByteBuffer> sSubsampleBuffer = new ThreadLocal<>();

  private static ParsableByteArray getParsableByteArray(
      ThreadLocal<ParsableByteArray> threadLocal, int requiredSize) {
    ParsableByteArray byteArray = threadLocal.get();
    if (byteArray == null) {
      byteArray = new ParsableByteArray(requiredSize);
      threadLocal.set(byteArray);
    } else if (byteArray.capacity() < requiredSize) {
      byteArray.reset(requiredSize);
    } else {
      byteArray.setPosition(0);
      byteArray.setLimit(requiredSize);
    }
    return byteArray;
  }

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

    int flags = 0;
    if (isKeyFrame) {
      flags |= C.BUFFER_FLAG_KEY_FRAME;
    }

    if (key != null) {
      flags |= C.BUFFER_FLAG_ENCRYPTED;
      this.mCryptoData = new CryptoData(encryptionMode, key, encryptedBlocks, clearBlocks);

      // Acquire a reused buffer for the encryption signal byte.
      // This byte contains the IV size and a flag indicating if subsamples are present.
      this.mEncryptionSignalByte = getParsableByteArray(sEncryptionSignalByte, 1);
      // Top bit (0x80) signals presence of subsamples; remaining bits are the IV size.
      this.mEncryptionSignalByte.getData()[0] = (byte) (ivSize | (mHasSubsamples ? 0x80 : 0));

      // Re-use or resize the IV buffer to avoid allocation.
      this.mIvData = getParsableByteArray(sIvData, 0);
      this.mIvData.reset(initializationVector, ivSize);

      if (mHasSubsamples) {
        int subsampleCount = subsampleEncryptedBytes.length;
        // 2 bytes for count + 6 bytes per subsample (2 for clear, 4 for encrypted).
        int subsampleDataLength = 2 + 6 * subsampleCount;
        this.mSubsampleData = getParsableByteArray(sSubsampleData, subsampleDataLength);

        // We use a ThreadLocal ByteBuffer to provide a readable 'put' API over the
        // raw reused byte array.
        ByteBuffer buffer = sSubsampleBuffer.get();
        // If the ParsableByteArray was reallocated due to size requirements, we must
        // re-wrap our ByteBuffer to point to the new underlying array.
        if (buffer == null || buffer.array() != mSubsampleData.getData()) {
          buffer = ByteBuffer.wrap(mSubsampleData.getData());
          sSubsampleBuffer.set(buffer);
        }
        buffer.clear();
        buffer.putShort((short) subsampleCount);
        for (int i = 0; i < subsampleCount; i++) {
          buffer.putShort((short) subsampleClearBytes[i]);
          buffer.putInt(subsampleEncryptedBytes[i]);
        }
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
