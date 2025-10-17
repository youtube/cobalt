// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_SAFE_BASE_NAME_H_
#define BASE_FILES_SAFE_BASE_NAME_H_

#include "base/base_export.h"
#include "base/files/file_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// Represents the last path component of a FilePath object, either a file or a
// directory. This type does not allow absolute paths or references to parent
// directories and is considered safe to be passed over IPC. See
// FilePath::BaseName().
// Usage examples:
// absl::optional<SafeBaseName> a
//     (SafeBaseName::Create(FILE_PATH_LITERAL("file.txt")));
// FilePath dir(FILE_PATH_LITERAL("foo")); dir.Append(*a);
class BASE_EXPORT SafeBaseName {
 public:
  // TODO(crbug.com/1269986): Change to only be exposed to Mojo.
  SafeBaseName() = default;

  // Factory method that returns a valid SafeBaseName or absl::nullopt.
  static absl::optional<SafeBaseName> Create(const FilePath&);

  // Same as above, but takes a StringPieceType for convenience.
  static absl::optional<SafeBaseName> Create(FilePath::StringPieceType);
  const FilePath& path() const { return path_; }

  bool operator==(const SafeBaseName& that) const;

 private:
  // Constructs a new SafeBaseName from the given FilePath.
  explicit SafeBaseName(const FilePath&);
  FilePath path_;
};

}  // namespace base

#endif  // BASE_FILES_SAFE_BASE_NAME_H_
