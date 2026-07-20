// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package dev.cobalt.util;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class JavaSwitchesTest {

    @Test
    public void splitArguments_nullOrEmpty() {
        assertThat(JavaSwitches.splitArguments(null)).isEmpty();
        assertThat(JavaSwitches.splitArguments("")).isEmpty();
    }

    @Test
    public void splitArguments_singleArg() {
        assertThat(JavaSwitches.splitArguments("--foo")).containsExactly("--foo");
        assertThat(JavaSwitches.splitArguments("--foo=bar")).containsExactly("--foo=bar");
    }

    @Test
    public void splitArguments_multipleArgs() {
        assertThat(JavaSwitches.splitArguments("--foo --bar")).containsExactly("--foo", "--bar");
        assertThat(JavaSwitches.splitArguments("--foo   --bar")).containsExactly("--foo", "--bar");
    }

    @Test
    public void splitArguments_doubleQuotes() {
        assertThat(JavaSwitches.splitArguments("--foo=\"bar baz\""))
                .containsExactly("--foo=bar baz");
        assertThat(JavaSwitches.splitArguments("--foo=\"\"")).containsExactly("--foo=");
    }

    @Test
    public void splitArguments_singleQuotes() {
        assertThat(JavaSwitches.splitArguments("--foo='bar baz'")).containsExactly("--foo=bar baz");
        assertThat(JavaSwitches.splitArguments("--foo=''")).containsExactly("--foo=");
    }

    @Test
    public void splitArguments_mixedQuotes() {
        assertThat(JavaSwitches.splitArguments("--foo=\"bar 'baz'\""))
                .containsExactly("--foo=bar 'baz'");
        assertThat(JavaSwitches.splitArguments("--foo='bar \"baz\"'"))
                .containsExactly("--foo=bar \"baz\"");
    }

    @Test
    public void splitArguments_otherWhitespace() {
        assertThat(JavaSwitches.splitArguments("--foo\t--bar")).containsExactly("--foo", "--bar");
        assertThat(JavaSwitches.splitArguments("--foo\n--bar")).containsExactly("--foo", "--bar");
    }

    @Test
    public void splitArguments_emptyQuotesPreserved() {
        // "foo "" bar" -> ["foo", "", "bar"]
        assertThat(JavaSwitches.splitArguments("foo \"\" bar")).containsExactly("foo", "", "bar");
        assertThat(JavaSwitches.splitArguments("foo '' bar")).containsExactly("foo", "", "bar");
    }
}
