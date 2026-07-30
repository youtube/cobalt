// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.AutofillSuggestion.Payload;

import java.util.Objects;

@JNINamespace("autofill")
@NullMarked
public final class AutofillAiPayload implements Payload {
    private final String mGuid;
    private final boolean mRequiresServerFetch;

    @CalledByNative
    public AutofillAiPayload(@JniType("std::string") String guid, boolean requiresServerFetch) {
        mGuid = guid;
        mRequiresServerFetch = requiresServerFetch;
    }

    public String getGuid() {
        return mGuid;
    }

    public boolean requiresServerFetch() {
        return mRequiresServerFetch;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof AutofillAiPayload other)) {
            return false;
        }
        return this.mGuid.equals(other.mGuid)
                && this.mRequiresServerFetch == other.mRequiresServerFetch;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mGuid, mRequiresServerFetch);
    }
}
