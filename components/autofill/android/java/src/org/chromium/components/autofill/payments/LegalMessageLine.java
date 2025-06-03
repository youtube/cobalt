// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import java.util.LinkedList;
import java.util.List;

/**
 * Legal message line with links to show in the autofill ui.
 */
@JNINamespace("autofill")
public class LegalMessageLine {
    /**
     * A link in the legal message line.
     */
    public static class Link {
        /**
         * The starting inclusive index of the link position in the text.
         */
        public int start;

        /**
         * The ending exclusive index of the link position in the text.
         */
        public int end;

        /**
         * The URL of the link.
         */
        public String url;

        /**
         * Creates a new instance of the link.
         *
         * @param start The starting inclusive index of the link position in the text.
         * @param end The ending exclusive index of the link position in the text.
         * @param url The URL of the link.
         */
        @CalledByNative("Link")
        public Link(int start, int end, String url) {
            this.start = start;
            this.end = end;
            this.url = url;
        }
    }

    /**
     * The plain text legal message line.
     */
    public String text;

    /**
     * A collection of links in the legal message line.
     */
    public final List<Link> links = new LinkedList<Link>();

    /**
     * Creates a new instance of the legal message line.
     *
     * @param text The plain text legal message.
     */
    @CalledByNative
    public LegalMessageLine(String text) {
        this.text = text;
    }

    /**
     * Creates a new instance of the legal message line with text and links.
     * @param text The plain text legal message.
     * @param links List of {@link Link} objects representing the links.
     */
    @VisibleForTesting
    public LegalMessageLine(String text, List<Link> links) {
        this.text = text;
        links.forEach(this::addLink);
    }

    /**
     * Adds a link to this legal message
     *
     * @param link The link to be added.
     */
    @CalledByNative
    /*package*/ void addLink(Link link) {
        links.add(link);
    }

    @CalledByNative
    /*package*/ static LinkedList<LegalMessageLine> addToList_createListIfNull(
            LinkedList<LegalMessageLine> list, String text) {
        if (list == null) list = new LinkedList<>();
        list.add(new LegalMessageLine(text));
        return list;
    }

    @CalledByNative
    /*package*/ static void addLinkToLastInList(
            LinkedList<LegalMessageLine> list, int start, int end, String url) {
        list.getLast().addLink(new Link(start, end, url));
    }
}
