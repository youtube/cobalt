// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import java.util.LinkedList;
import java.util.List;

/**
 * Legal message line with links to show in the autofill ui.
 */
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
    public LegalMessageLine(String text) {
        this.text = text;
    }
}
