// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_

#include <set>

#include "chrome/browser/devtools/protocol/target.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "net/base/host_port_pair.h"

using RemoteLocations = std::set<net::HostPortPair>;

class TargetHandler : public protocol::Target::Backend {
 public:
  TargetHandler(protocol::UberDispatcher* dispatcher,
                bool is_trusted,
                bool may_read_local_files);

  TargetHandler(const TargetHandler&) = delete;
  TargetHandler& operator=(const TargetHandler&) = delete;

  ~TargetHandler() override;

  RemoteLocations& remote_locations() { return remote_locations_; }

  // Target::Backend:
  protocol::Response SetRemoteLocations(
      std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
          in_locations) override;
  protocol::Response CreateTarget(
      const std::string& url,
      std::optional<int> left,
      std::optional<int> top,
      std::optional<int> width,
      std::optional<int> height,
      std::optional<std::string> window_state,
      std::optional<std::string> browser_context_id,
      std::optional<bool> enable_begin_frame_control,
      std::optional<bool> new_window,
      std::optional<bool> background,
      std::optional<bool> for_tab,
      std::string* out_target_id) override;

 private:
  RemoteLocations remote_locations_;
  const bool is_trusted_;
  const bool may_read_local_files_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
