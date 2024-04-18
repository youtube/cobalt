// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.MainDex;

/**
 * A java wrapper for Parsed, GURL's internal parsed URI representation.
 */
@MainDex
@JNINamespace("url")
/* package */ class Parsed {
    /* package */ final int mSchemeBegin;
    /* package */ final int mSchemeLength;
    /* package */ final int mUsernameBegin;
    /* package */ final int mUsernameLength;
    /* package */ final int mPasswordBegin;
    /* package */ final int mPasswordLength;
    /* package */ final int mHostBegin;
    /* package */ final int mHostLength;
    /* package */ final int mPortBegin;
    /* package */ final int mPortLength;
    /* package */ final int mPathBegin;
    /* package */ final int mPathLength;
    /* package */ final int mQueryBegin;
    /* package */ final int mQueryLength;
    /* package */ final int mRefBegin;
    /* package */ final int mRefLength;
    private final Parsed mInnerUrl;
    private final boolean mPotentiallyDanglingMarkup;

    /* package */ static Parsed createEmpty() {
        return new Parsed(0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1, false, null);
    }

    @CalledByNative
    private Parsed(int schemeBegin, int schemeLength, int usernameBegin, int usernameLength,
            int passwordBegin, int passwordLength, int hostBegin, int hostLength, int portBegin,
            int portLength, int pathBegin, int pathLength, int queryBegin, int queryLength,
            int refBegin, int refLength, boolean potentiallyDanglingMarkup, Parsed innerUrl) {
        mSchemeBegin = schemeBegin;
        mSchemeLength = schemeLength;
        mUsernameBegin = usernameBegin;
        mUsernameLength = usernameLength;
        mPasswordBegin = passwordBegin;
        mPasswordLength = passwordLength;
        mHostBegin = hostBegin;
        mHostLength = hostLength;
        mPortBegin = portBegin;
        mPortLength = portLength;
        mPathBegin = pathBegin;
        mPathLength = pathLength;
        mQueryBegin = queryBegin;
        mQueryLength = queryLength;
        mRefBegin = refBegin;
        mRefLength = refLength;
        mPotentiallyDanglingMarkup = potentiallyDanglingMarkup;
        mInnerUrl = innerUrl;
    }

    /* package */ long toNativeParsed() {
        long inner = 0;
        if (mInnerUrl != null) {
            inner = mInnerUrl.toNativeParsed();
        }
        return ParsedJni.get().createNative(mSchemeBegin, mSchemeLength, mUsernameBegin,
                mUsernameLength, mPasswordBegin, mPasswordLength, mHostBegin, mHostLength,
                mPortBegin, mPortLength, mPathBegin, mPathLength, mQueryBegin, mQueryLength,
                mRefBegin, mRefLength, mPotentiallyDanglingMarkup, inner);
    }

    /* package */ String serialize() {
        StringBuilder builder = new StringBuilder();
        builder.append(mSchemeBegin).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mSchemeLength).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mUsernameBegin).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mUsernameLength).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mPasswordBegin).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mPasswordLength).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mHostBegin).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mHostLength).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mPortBegin).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mPortLength).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mPathBegin).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mPathLength).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mQueryBegin).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mQueryLength).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mRefBegin).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mRefLength).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mPotentiallyDanglingMarkup).append(GURL.SERIALIZER_DELIMITER);
        builder.append(mInnerUrl != null);
        if (mInnerUrl != null) {
            builder.append(GURL.SERIALIZER_DELIMITER).append(mInnerUrl.serialize());
        }
        return builder.toString();
    }

    /* package */ static Parsed deserialize(String[] tokens, int startIndex) {
        int schemeBegin = Integer.parseInt(tokens[startIndex++]);
        int schemeLength = Integer.parseInt(tokens[startIndex++]);
        int usernameBegin = Integer.parseInt(tokens[startIndex++]);
        int usernameLength = Integer.parseInt(tokens[startIndex++]);
        int passwordBegin = Integer.parseInt(tokens[startIndex++]);
        int passwordLength = Integer.parseInt(tokens[startIndex++]);
        int hostBegin = Integer.parseInt(tokens[startIndex++]);
        int hostLength = Integer.parseInt(tokens[startIndex++]);
        int portBegin = Integer.parseInt(tokens[startIndex++]);
        int portLength = Integer.parseInt(tokens[startIndex++]);
        int pathBegin = Integer.parseInt(tokens[startIndex++]);
        int pathLength = Integer.parseInt(tokens[startIndex++]);
        int queryBegin = Integer.parseInt(tokens[startIndex++]);
        int queryLength = Integer.parseInt(tokens[startIndex++]);
        int refBegin = Integer.parseInt(tokens[startIndex++]);
        int refLength = Integer.parseInt(tokens[startIndex++]);
        boolean potentiallyDanglingMarkup = Boolean.parseBoolean(tokens[startIndex++]);
        Parsed innerParsed = null;
        if (Boolean.parseBoolean(tokens[startIndex++])) {
            innerParsed = Parsed.deserialize(tokens, startIndex);
        }
        return new Parsed(schemeBegin, schemeLength, usernameBegin, usernameLength, passwordBegin,
                passwordLength, hostBegin, hostLength, portBegin, portLength, pathBegin, pathLength,
                queryBegin, queryLength, refBegin, refLength, potentiallyDanglingMarkup,
                innerParsed);
    }

    @NativeMethods
    interface Natives {
        /**
         * Create and return the pointer to a native Parsed.
         */
        long createNative(int schemeBegin, int schemeLength, int usernameBegin, int usernameLength,
                int passwordBegin, int passwordLength, int hostBegin, int hostLength, int portBegin,
                int portLength, int pathBegin, int pathLength, int queryBegin, int queryLength,
                int refBegin, int refLength, boolean potentiallyDanglingMarkup, long innerUrl);
    }
}
