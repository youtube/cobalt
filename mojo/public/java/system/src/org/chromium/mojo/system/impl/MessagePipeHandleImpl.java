// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.system.impl;

import org.chromium.mojo.system.Handle;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.ResultAnd;

import java.nio.ByteBuffer;
import java.util.List;

/**
 * Implementation of {@link MessagePipeHandle}.
 */
class MessagePipeHandleImpl extends HandleBase implements MessagePipeHandle {
    /**
     * @see HandleBase#HandleBase(CoreImpl, long)
     */
    MessagePipeHandleImpl(CoreImpl core, long mojoHandle) {
        super(core, mojoHandle);
    }

    /**
     * @see HandleBase#HandleBase(HandleBase)
     */
    MessagePipeHandleImpl(HandleBase handle) {
        super(handle);
    }

    /**
     * @see org.chromium.mojo.system.MessagePipeHandle#pass()
     */
    @Override
    public MessagePipeHandle pass() {
        return new MessagePipeHandleImpl(this);
    }

    /**
     * @see MessagePipeHandle#writeMessage(ByteBuffer, List, WriteFlags)
     */
    @Override
    public void writeMessage(ByteBuffer bytes, List<? extends Handle> handles, WriteFlags flags) {
        mCore.writeMessage(this, bytes, handles, flags);
    }

    /**
     * @see MessagePipeHandle#readMessage(ReadFlags)
     */
    @Override
    public ResultAnd<ReadMessageResult> readMessage(ReadFlags flags) {
        return mCore.readMessage(this, flags);
    }
}
