// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PROCESS_PRIORITY_POLICY_SETTINGS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PROCESS_PRIORITY_POLICY_SETTINGS_H_

namespace performance_manager {

// Global staging area for embedders to configure process priority rules prior
// to graph and voter instantiation.
struct ProcessPriorityPolicySettings {
  // If true, visibility of hosting frames (main frames and subframes) will be
  // ignored when determining the priority of a process.
  bool ignore_visibility = false;

  // If true (and `ignore_visibility` is false), visibility of
  // hosting main frames will be ignored when determining the priority of a
  // process. Child frames (iframes) will still have their visibility
  // considered.
  bool ignore_main_frame_visibility = false;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_PROCESS_PRIORITY_POLICY_SETTINGS_H_
