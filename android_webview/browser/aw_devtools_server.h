// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DEVTOOLS_SERVER_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DEVTOOLS_SERVER_H_

namespace android_webview {

// This class controls WebView-specific Developer Tools remote debugging server.
class AwDevToolsServer {
 public:
  AwDevToolsServer();

  AwDevToolsServer(const AwDevToolsServer&) = delete;
  AwDevToolsServer& operator=(const AwDevToolsServer&) = delete;

  ~AwDevToolsServer();

  // Opens linux abstract socket to be ready for remote debugging.
  void Start();

  // Closes debugging socket, stops debugging.
  void Stop();

  bool IsStarted() const;

 private:
  bool is_started_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DEVTOOLS_SERVER_H_
