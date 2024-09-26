// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.mojo.bindings.Callbacks.Callback1;
import org.chromium.mojo.bindings.Callbacks.Callback7;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Testing generated callbacks
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class CallbacksTest {
    /**
     * Testing {@link Callback1}.
     */
    @Test
    @SmallTest
    public void testCallback1() {
        final List<Integer> parameters = new ArrayList<Integer>();
        new Callback1<Integer>() {
            @Override
            public void call(Integer i1) {
                parameters.add(i1);
            }
        }
                .call(1);
        Assert.assertEquals(Arrays.asList(1), parameters);
    }

    /**
     * Testing {@link Callback7}.
     */
    @Test
    @SmallTest
    public void testCallback7() {
        final List<Integer> parameters = new ArrayList<Integer>();
        new Callback7<Integer, Integer, Integer, Integer, Integer, Integer, Integer>() {
            @Override
            public void call(Integer i1, Integer i2, Integer i3, Integer i4, Integer i5, Integer i6,
                    Integer i7) {
                parameters.add(i1);
                parameters.add(i2);
                parameters.add(i3);
                parameters.add(i4);
                parameters.add(i5);
                parameters.add(i6);
                parameters.add(i7);
            }
        }
                .call(1, 2, 3, 4, 5, 6, 7);
        Assert.assertEquals(Arrays.asList(1, 2, 3, 4, 5, 6, 7), parameters);
    }
}
