// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Locale;
import java.util.Objects;

/** A class to contain HTML metadata of the focused element. */
@NullMarked
public final class HtmlMetadata {
    public final @Nullable String label;
    public final @Nullable String fieldName;
    public final @Nullable String placeholder;

    public static final HtmlMetadata EMPTY = new HtmlMetadata(null, null, null);

    public static HtmlMetadata create(
            @Nullable String label, @Nullable String fieldName, @Nullable String placeholder) {
        if (label == null && fieldName == null && placeholder == null) {
            return EMPTY;
        }
        return new HtmlMetadata(label, fieldName, placeholder);
    }

    private HtmlMetadata(
            @Nullable String label, @Nullable String fieldName, @Nullable String placeholder) {
        this.label = label;
        this.fieldName = fieldName;
        this.placeholder = placeholder;
    }

    public boolean equals(
            @Nullable String label, @Nullable String fieldName, @Nullable String placeholder) {
        return Objects.equals(this.label, label)
                && Objects.equals(this.fieldName, fieldName)
                && Objects.equals(this.placeholder, placeholder);
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) return true;
        if (!(o instanceof HtmlMetadata)) return false;
        HtmlMetadata that = (HtmlMetadata) o;
        return equals(that.label, that.fieldName, that.placeholder);
    }

    @Override
    public int hashCode() {
        return Objects.hash(label, fieldName, placeholder);
    }

    @Override
    public String toString() {
        return String.format(
                Locale.US,
                "HtmlMetadata{label='%s', fieldName='%s', placeholder='%s'}",
                label,
                fieldName,
                placeholder);
    }
}
