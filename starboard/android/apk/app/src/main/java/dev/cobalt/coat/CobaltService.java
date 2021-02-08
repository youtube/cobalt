// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.coat;

import dev.cobalt.util.UsedByNative;

/** Abstract class that provides an interface for Cobalt to interact with a platform service. */
public abstract class CobaltService {
  /** Interface that returns an object that extends CobaltService. */
  public interface Factory {
    /** Create the service. */
    public CobaltService createCobaltService(long nativeService);

    /** Get the name of the service. */
    public String getServiceName();
  }
  /** Take in a reference to StarboardBridge & use it as needed. Default behavior is no-op. */
  public void receiveStarboardBridge(StarboardBridge bridge) {}

  // Lifecycle
  /** Prepare service for start or resume. */
  public abstract void beforeStartOrResume();

  /** Prepare service for suspend. */
  public abstract void beforeSuspend();

  /** Prepare service for stop. */
  public abstract void afterStopped();

  // Service API
  /** Response to client from calls to receiveFromClient(). */
  @SuppressWarnings("unused")
  @UsedByNative
  public static class ResponseToClient {
    /** Indicate if the service was unable to receive data because it is in an invalid state. */
    @SuppressWarnings("unused")
    @UsedByNative
    public boolean invalidState;
    /** The synchronous response data from the service. */
    @SuppressWarnings("unused")
    @UsedByNative
    public byte[] data;
  }

  /** Receive data from client of the service. */
  @SuppressWarnings("unused")
  @UsedByNative
  public abstract ResponseToClient receiveFromClient(byte[] data);

  /** Close the service. */
  @SuppressWarnings("unused")
  @UsedByNative
  public abstract void close();

  /** Send data from the service to the client. */
  protected void sendToClient(long nativeService, byte[] data) {
    nativeSendToClient(nativeService, data);
  }

  private native void nativeSendToClient(long nativeService, byte[] data);
}
