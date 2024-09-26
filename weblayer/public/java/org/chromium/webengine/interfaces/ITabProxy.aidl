// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.interfaces;

import org.chromium.webengine.interfaces.IFullscreenCallbackDelegate;
import org.chromium.webengine.interfaces.IPostMessageCallback;
import org.chromium.webengine.interfaces.IStringCallback;
import org.chromium.webengine.interfaces.ITabObserverDelegate;

import java.util.List;

oneway interface ITabProxy {
  void setActive() = 1;
  void close() = 2;
  void executeScript(in String script, in boolean useSeparateIsolate, in IStringCallback callback) = 3;

  void setTabObserverDelegate(ITabObserverDelegate tabObserverDelegate) = 6;

  // PostMessage:
  void postMessage(in String message, in String targetOrigin) = 7;
  void createMessageEventListener(in IPostMessageCallback callback, in List<String> allowedOrigins) = 8;
  void addMessageEventListener(in List<String> allowedOrigins) = 9;
  void removeMessageEventListener(in List<String> allowedOrigins) = 10;

  // Fullscreen mode:
  void setFullscreenCallbackDelegate(in IFullscreenCallbackDelegate fullscreenCallbackDelegate) = 11;

  // Reserved Tags: 4, 5
}
