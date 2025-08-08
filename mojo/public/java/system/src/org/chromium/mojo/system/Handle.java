// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system;

import org.chromium.mojo.system.Core.HandleSignalsState;

import java.io.Closeable;

/**
 * A generic mojo handle.
 */
public interface Handle extends Closeable {

    /**
     * Closes the given |handle|.
     * <p>
     * Concurrent operations on |handle| may succeed (or fail as usual) if they happen before the
     * close, be cancelled with result |MojoResult.CANCELLED| if they properly overlap (this is
     * likely the case with |wait()|, etc.), or fail with |MojoResult.INVALID_ARGUMENT| if they
     * happen after.
     */
    @Override
    public void close();

    /**
     * @return the last known signaling state of the handle.
     */
    public HandleSignalsState querySignalsState();

    /**
     * @return whether the handle is valid. A handle is valid until it has been explicitly closed or
     *         send through a message pipe via |MessagePipeHandle.writeMessage|.
     */
    public boolean isValid();

    /**
     * Converts this handle into an {@link UntypedHandle}, invalidating this handle.
     */
    public UntypedHandle toUntypedHandle();

    /**
     * Returns the {@link Core} implementation for this handle. Can be null if this handle is
     * invalid.
     */
    public Core getCore();

    /**
     * Passes ownership of the handle from this handle to the newly created Handle object,
     * invalidating this handle object in the process.
     */
    public Handle pass();

    /**
     * Releases the native handle backed by this {@link Handle}. The caller owns the handle and must
     * close it.
     */
    public long releaseNativeHandle();
}
