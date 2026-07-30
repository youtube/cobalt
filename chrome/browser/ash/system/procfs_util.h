// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_PROCFS_UTIL_H_
#define CHROME_BROWSER_ASH_SYSTEM_PROCFS_UTIL_H_

#include "base/files/file.h"
#include "base/process/process_handle.h"

namespace ash {
namespace system {

// A file object for "/proc/<pid>/stat".
class ProcStatFile {
 public:
  explicit ProcStatFile(base::ProcessId process_id);
  ProcStatFile(ProcStatFile&&) = default;
  ProcStatFile(ProcStatFile&) = delete;
  ~ProcStatFile();

  // Returns whether the stat file is valid. See `base::File::IsValid()` for
  // details.
  bool IsValid() const;

  // Returns whether the process is still alive or not.
  bool IsPidAlive();

 private:
  base::File file_;
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_PROCFS_UTIL_H_
