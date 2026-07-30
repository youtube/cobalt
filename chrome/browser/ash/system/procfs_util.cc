// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/procfs_util.h"

#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"

namespace ash {
namespace system {

ProcStatFile::ProcStatFile(base::ProcessId process_id) {
  base::FilePath procfs_stat_path =
      base::FilePath("/proc")
          .Append(base::NumberToString(process_id))
          .Append("stat");
  // Opening procfs file is not blocking.
  base::ScopedAllowBlocking allow_blocking;
  file_ = base::File(procfs_stat_path,
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
}

ProcStatFile::~ProcStatFile() {
  // Closing procfs file is not blocking.
  base::ScopedAllowBlocking allow_blocking;
  file_.Close();
}

bool ProcStatFile::IsValid() const {
  return file_.IsValid();
}

bool ProcStatFile::IsPidAlive() {
  // Reading procfs is not blocking.
  base::ScopedAllowBlocking allow_blocking;
  // If the process/thread dies, read(2)ing stat file fails as ESRCH.
  std::array<uint8_t, 1> dummy;
  return file_.IsValid() && file_.Read(0, dummy) == 1;
}

}  // namespace system
}  // namespace ash
