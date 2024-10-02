// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.ListObservable.ListObserver;

/**
 * Basic test ensuring the {@link ListModel} notifies listeners properly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SimpleListObservableTest {
    @Mock
    private ListObserver<Void> mObserver;

    private ListModel<Integer> mIntegerList = new ListModel<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mIntegerList.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        mIntegerList.removeObserver(mObserver);
    }

    @Test
    public void testNotifiesSuccessfulInsertions() {
        // Replacing an empty list with a non-empty one is always an insertion.
        assertThat(mIntegerList.size(), is(0));
        mIntegerList.set(new Integer[] {333, 88888888, 22});
        verify(mObserver).onItemRangeInserted(mIntegerList, 0, 3);
        assertThat(mIntegerList.size(), is(3));
        assertThat(mIntegerList.get(1), is(88888888));

        // Adding Items is always an insertion.
        mIntegerList.add(55555);
        verify(mObserver).onItemRangeInserted(mIntegerList, 3, 1);
        assertThat(mIntegerList.size(), is(4));
        assertThat(mIntegerList.get(3), is(55555));
    }

    @Test
    public void testModelNotifiesSuccessfulRemoval() {
        Integer eightEights = 88888888;
        mIntegerList.set(new Integer[] {333, eightEights, 22});
        assertThat(mIntegerList.size(), is(3));

        // Removing any item by instance is always a removal.
        mIntegerList.remove(eightEights);
        verify(mObserver).onItemRangeRemoved(mIntegerList, 1, 1);

        // Setting an empty list is a removal of all items.
        mIntegerList.set(new Integer[] {});
        verify(mObserver).onItemRangeRemoved(mIntegerList, 0, 2);
    }

    @Test
    public void testModelNotifiesReplacedDataAndRemoval() {
        // The initial setting is an insertion.
        mIntegerList.set(new Integer[] {333, 88888888, 22});
        verify(mObserver).onItemRangeInserted(mIntegerList, 0, 3);

        // Setting a smaller number of items is a removal and a change.
        mIntegerList.set(new Integer[] {4444, 22});
        verify(mObserver).onItemRangeChanged(mIntegerList, 0, 2, null);
        verify(mObserver).onItemRangeRemoved(mIntegerList, 2, 1);
    }

    @Test
    public void testModelNotifiesReplacedDataAndInsertion() {
        // The initial setting is an insertion.
        mIntegerList.set(new Integer[] {1234, 56});
        verify(mObserver).onItemRangeInserted(mIntegerList, 0, 2);

        // Setting a larger number of items is an insertion and a change.
        mIntegerList.set(new Integer[] {4444, 22, 1, 666666});
        verify(mObserver).onItemRangeChanged(mIntegerList, 0, 2, null);
        verify(mObserver).onItemRangeInserted(mIntegerList, 2, 2);

        // Setting empty data is a removal.
        mIntegerList.set(new Integer[] {});
        verify(mObserver).onItemRangeRemoved(mIntegerList, 0, 4);

        // Replacing an empty list with another empty list is a no-op.
        mIntegerList.set(new Integer[] {});
        verifyNoMoreInteractions(mObserver);
    }
}