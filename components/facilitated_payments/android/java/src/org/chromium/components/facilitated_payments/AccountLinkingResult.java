// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Rich result structure for the instrument manager account linking flow. */
@JNINamespace("payments::facilitated")
@NullMarked
public class AccountLinkingResult {
    private final boolean mIsSuccessful;
    private final long mInstrumentId;
    private final @AccountLinkingResultCode int mErrorCode;

    public AccountLinkingResult(
            boolean isSuccessful, long instrumentId, @AccountLinkingResultCode int errorCode) {
        mIsSuccessful = isSuccessful;
        mInstrumentId = instrumentId;
        mErrorCode = errorCode;
    }

    /** Builder for {@link AccountLinkingResult}. */
    public static class Builder {
        private boolean mIsSuccessful;
        private long mInstrumentId;
        private @AccountLinkingResultCode int mErrorCode =
                AccountLinkingResultCode.COULD_NOT_INVOKE;

        public Builder setIsSuccessful(boolean isSuccessful) {
            mIsSuccessful = isSuccessful;
            return this;
        }

        public Builder setInstrumentId(long instrumentId) {
            mInstrumentId = instrumentId;
            return this;
        }

        public Builder setErrorCode(@AccountLinkingResultCode int errorCode) {
            mErrorCode = errorCode;
            return this;
        }

        public AccountLinkingResult build() {
            return new AccountLinkingResult(mIsSuccessful, mInstrumentId, mErrorCode);
        }
    }

    @CalledByNative
    public boolean isSuccessful() {
        return mIsSuccessful;
    }

    @CalledByNative
    public long getInstrumentId() {
        return mInstrumentId;
    }

    /** Returns the result code indicating the outcome of account linking. */
    @CalledByNative
    public @AccountLinkingResultCode int getErrorCode() {
        return mErrorCode;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof AccountLinkingResult)) return false;
        AccountLinkingResult that = (AccountLinkingResult) o;
        return mIsSuccessful == that.mIsSuccessful
                && mInstrumentId == that.mInstrumentId
                && mErrorCode == that.mErrorCode;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mIsSuccessful, mInstrumentId, mErrorCode);
    }
}
