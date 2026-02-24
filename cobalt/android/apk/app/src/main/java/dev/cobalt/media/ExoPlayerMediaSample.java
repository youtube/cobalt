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
  private final int size;
  private final long timestamp;
  private final boolean isKeyFrame;
  private final int type;
  private final int encryptionMode;
  private final byte[] key;
  private final int encryptedBlocks;
  private final int clearBlocks;
  private final byte[] initializationVector;
  private final int ivSize;
  private final int[] subsampleEncryptedBytes;
  private final int[] subsampleClearBytes;
  private final CryptoData mCryptoData;

  @CalledByNative
  public ExoPlayerMediaSample(
      ByteBuffer mSamples,
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
    this.mSamples = mSamples;
    this.size = size;
    this.timestamp = timestamp;
    this.isKeyFrame = isKeyFrame;
    this.type = type;
    this.encryptionMode = encryptionMode;
    this.key = key;
    this.encryptedBlocks = encryptedBlocks;
    this.clearBlocks = clearBlocks;
    this.initializationVector = initializationVector;
    this.ivSize = ivSize;
    this.subsampleEncryptedBytes = subsampleEncryptedBytes;
    this.subsampleClearBytes = subsampleClearBytes;
    this.mCryptoData = key != null ? new CryptoData(encryptionMode, key, encryptedBlocks, clearBlocks) : null;
  }

  public ByteBuffer getSamples() {
    return mSamples;
  }

  public int getSize() {
    return size;
  }

  public long getTimestamp() {
    return timestamp;
  }

  public int getType() {
    return type;
  }

  public boolean isEncrypted() {
    return key != null;
  }

  private boolean hasSubsamples() {
    return subsampleEncryptedBytes != null
        && subsampleClearBytes != null
        && subsampleEncryptedBytes.length > 0;
  }

  /** Returns the sample flags, including keyframe and encryption flags. */
  public int getFlags() {
    int flags = 0;
    if (isKeyFrame) {
      flags |= C.BUFFER_FLAG_KEY_FRAME;
    }
    if (isEncrypted()) {
      flags |= C.BUFFER_FLAG_ENCRYPTED;
    }
    return flags;
  }

  /** Returns the total size of the sample, including encryption preamble data. */
  public int getTotalSize() {
    int totalSize = size;
    if (isEncrypted()) {
      totalSize++; // signal byte
      totalSize += ivSize;
      if (hasSubsamples()) {
        totalSize += 2; // subsample count
        totalSize += 6 * subsampleEncryptedBytes.length;
      }
    }
    return totalSize;
  }

  /** Returns the {@link CryptoData} for this sample, or null if it is not encrypted. */
  @Nullable
  public CryptoData getCryptoData() {
    return mCryptoData;
  }

  /** Returns a {@link ParsableByteArray} containing the encryption signal byte. */
  @Nullable
  public ParsableByteArray getEncryptionSignalByte() {
    if (!isEncrypted()) {
      return null;
    }
    ParsableByteArray encryptionSignalByte = new ParsableByteArray(1);
    encryptionSignalByte.getData()[0] = (byte) (ivSize | (hasSubsamples() ? 0x80 : 0));
    return encryptionSignalByte;
  }

  /** Returns a {@link ParsableByteArray} containing the initialization vector. */
  @Nullable
  public ParsableByteArray getIvData() {
    if (!isEncrypted()) {
      return null;
    }
    return new ParsableByteArray(initializationVector, ivSize);
  }

  /**
   * Returns a {@link ParsableByteArray} containing the subsample count and subsample data,
   * or null if there are no subsamples.
   */
  @Nullable
  public ParsableByteArray getSubsampleData() {
    if (!hasSubsamples()) {
      return null;
    }
    int subsampleCount = subsampleEncryptedBytes.length;
    int subsampleDataLength = 2 + 6 * subsampleCount;
    ByteBuffer subsampleBuffer = ByteBuffer.allocate(subsampleDataLength);
    subsampleBuffer.putShort((short) subsampleCount);
    for (int i = 0; i < subsampleCount; i++) {
      subsampleBuffer.putShort((short) subsampleClearBytes[i]);
      subsampleBuffer.putInt(subsampleEncryptedBytes[i]);
    }
    return new ParsableByteArray(subsampleBuffer.array());
  }
}
