// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/android/child_process_importance.h"

#include <ostream>

#include "base/android/android_info.h"

namespace content {

std::ostream& operator<<(std::ostream& out, ChildProcessImportance importance) {
  switch (importance) {
    case ChildProcessImportance::NORMAL:
      return out << "NORMAL";
    case ChildProcessImportance::NOT_PERCEPTIBLE:
      return out << "NOT_PERCEPTIBLE";
    case ChildProcessImportance::MODERATE:
      return out << "MODERATE";
    case ChildProcessImportance::IMPORTANT:
      return out << "IMPORTANT";
  }
}

bool IsNotPerceptibleImportanceSupported() {
  // This is the same as `SUPPORT_NOT_PERCEPTIBLE_BINDING` in
  // ChildProcessConnection.java.
  return base::android::android_info::sdk_int() >=
         base::android::android_info::SDK_VERSION_Q;
}

content::ChildProcessImportance PriorityToChildProcessImportance(
    base::Process::Priority priority) {
  switch (priority) {
    case base::Process::Priority::kBestEffort:
      return ChildProcessImportance::NORMAL;
    case base::Process::Priority::kUserVisible:
      return ChildProcessImportance::MODERATE;
    case base::Process::Priority::kUserBlocking:
      return ChildProcessImportance::IMPORTANT;
  }
}

}  // namespace content
