// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Base class for Token and UnguessableToken. */
@NullMarked
public abstract class TokenBase {
  protected final long mHigh;
  protected final long mLow;

  protected TokenBase(long high, long low) {
    mHigh = high;
    mLow = low;
  }

  private long getHighForSerialization() {
    return mHigh;
  }

  private long getLowForSerialization() {
    return mLow;
  }

  @Override
  public boolean equals(@Nullable Object obj) {
    if (obj == null || obj.getClass() != getClass()) {
      return false;
    }
    TokenBase other = (TokenBase) obj;
    return other.mHigh == mHigh && other.mLow == mLow;
  }

  @Override
  public int hashCode() {
    int mLowHash = (int) (mLow ^ (mLow >>> 32));
    int mHighHash = (int) (mHigh ^ (mHigh >>> 32));
    return 31 * mLowHash + mHighHash;
  }
}
