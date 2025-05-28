// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import static com.uber.nullaway.LibraryModels.MethodRef.methodRef;

import com.google.common.collect.ImmutableSet;
import com.google.common.collect.ImmutableSetMultimap;
import com.uber.nullaway.LibraryModels;

import org.chromium.build.annotations.ServiceImpl;

/** Checks for calls to .stream(). See //styleguide/java/java.md for rationale. */
@ServiceImpl(LibraryModels.class)
public class ChromeNullAwayLibraryModel implements LibraryModels {

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> failIfNullParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> explicitlyNullableParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> nonNullParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> nullImpliesTrueParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> nullImpliesFalseParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> nullImpliesNullParameters() {
        String sharedPrefs = "android.content.SharedPreferences";
        return new ImmutableSetMultimap.Builder<MethodRef, Integer>()
                .put(methodRef(sharedPrefs, "getString(java.lang.String,java.lang.String)"), 1)
                .put(
                        methodRef(
                                sharedPrefs,
                                "getStringSet(java.lang.String,java.util.Set<java.lang.String>)"),
                        1)
                .put(methodRef("android.content.Intent", "normalizeMimeType(java.lang.String)"), 0)
                .put(methodRef("android.util.LongSparseArray", "get(long,E)"), 1)
                .put(methodRef("android.util.SparseArray", "get(int,E)"), 1)
                .build();
    }

    @Override
    public ImmutableSet<MethodRef> nullableReturns() {
        return ImmutableSet.of(
                methodRef("android.util.SparseArray", "get(int)"),
                methodRef("android.util.LongSparseArray", "get(long)"),
                methodRef("java.util.Map", "put(K, V)"),
                methodRef("java.util.Map", "remove(java.lang.Object)"));
    }

    @Override
    public ImmutableSet<MethodRef> nonNullReturns() {
        return ImmutableSet.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> castToNonNullMethods() {
        return ImmutableSetMultimap.of();
    }
}
