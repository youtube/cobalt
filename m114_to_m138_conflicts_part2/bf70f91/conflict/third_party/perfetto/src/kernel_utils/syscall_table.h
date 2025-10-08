/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_KERNEL_UTILS_SYSCALL_TABLE_H_
#define SRC_KERNEL_UTILS_SYSCALL_TABLE_H_

<<<<<<< HEAD
=======
#include <memory>
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include <optional>
#include <string>

#include "perfetto/ext/base/string_view.h"
<<<<<<< HEAD
#include "perfetto/ext/base/utils.h"
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

namespace perfetto {

static constexpr size_t kMaxSyscalls = 550;

<<<<<<< HEAD
enum class Architecture {
  kUnknown = 0,
  kArm64,
  kArm32,
=======
enum Architecture {
  kUnknown = 0,
  kArmEabi,  // 32-bit kernel running a 32-bit process (most old devices).
  kAarch32,  // 64-bit kernel running a 32-bit process (should be rare).
  kAarch64,  // 64-bit kernel running a 64-bit process (most new devices).
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  kX86_64,
  kX86,
};

class SyscallTable {
 public:
<<<<<<< HEAD
  using OffT = uint16_t;
  // Exposed for testing.
  template <typename Table>
  static SyscallTable Load() {
    static_assert(base::ArraySize(Table::offsets) <= kMaxSyscalls,
                  "kMaxSyscalls too small");
    return SyscallTable(Table::names, Table::offsets,
                        base::ArraySize(Table::offsets));
  }

  explicit SyscallTable(Architecture arch);

=======
  explicit SyscallTable(Architecture arch);

  // Use for testing.
  SyscallTable(const char* const* table, size_t count)
      : syscall_count_(count), syscall_table_(table) {}

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  // Return the architecture enum for the given uname machine string.
  static Architecture ArchFromString(base::StringView machine);

  // Returns the syscall table based on the current machine's architecture. Only
  // works on Linux based systems.
  static SyscallTable FromCurrentArch();

  // Returns the syscall id for the syscall with the given name. If the syscall
  // is not found, returns std::nullopt.
  std::optional<size_t> GetByName(const std::string& name) const;

  // Returns the syscall name for the syscall with the given id. If the syscall
  // is not found, returns nullptr.
  const char* GetById(size_t id) const;

 private:
<<<<<<< HEAD
  SyscallTable(const char* names, const OffT* off, size_t count)
      : syscall_names_(names), syscall_offsets_(off), syscall_count_(count) {}

  const char* syscall_names_ = "";
  const OffT* syscall_offsets_ = {};
  size_t syscall_count_ = 0;
=======
  size_t syscall_count_;
  const char* const* syscall_table_;
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
};
}  // namespace perfetto

#endif  // SRC_KERNEL_UTILS_SYSCALL_TABLE_H_
