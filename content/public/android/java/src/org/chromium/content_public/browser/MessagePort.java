// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.Handler;

import org.chromium.build.annotations.UsedByReflection;
import org.chromium.content.browser.AppWebMessagePort;

/**
 * Interface for message ports that handle postMessage requests.
 */
@UsedByReflection("")
public interface MessagePort {
    /**
     * The message callback for receiving messages.
     */
    public interface MessageCallback {
        /**
         * Sent when the associated {@link MessagePort} gets a postMessage.
         * @param messagePayload   The message payload that was received.
         * @param sentPorts The {@link MessagePort}s that were sent if any.
         */
        void onMessage(MessagePayload messagePayload, MessagePort[] sentPorts);
    }

    /**
     * Called to create an entangled pair of ports.
     * @return An array of a pair of{@link MessagePort} instances.
     */
    public static MessagePort[] createPair() {
        return AppWebMessagePort.createPair();
    }

    /**
     * Close the port for use.
     */
    void close();

    /**
     * @return Whether the port has been closed before.
     */
    boolean isClosed();

    /**
     * @return Whether the port has been transferred using
     *         {@link MessagePort#postMessage(MessagePayload, MessagePort[])} before.
     */
    boolean isTransferred();

    /**
     * @return Whether the port has been started.
     */
    boolean isStarted();

    /**
     * Sets the handler and message callback to be used for the messages received. If the given
     * {@link Handler} is not null, then the callback is received on the handler thread, if not
     * it is on UI thread.
     *
     * See {@link MessagePort.MessageCallback}
     */
    void setMessageCallback(MessageCallback messageCallback, Handler handler);

    /**
     * Send a postMessage request through this port to its designated receiving end.
     * @param messagePayload   The message payload to be sent.
     * @param sentPorts The ports to be transferred.
     */
    void postMessage(MessagePayload messagePayload, MessagePort[] sentPorts);
}
