// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_EXTERNAL_CLEAR_KEY_TEST_HELPER_H_
#define MEDIA_CDM_EXTERNAL_CLEAR_KEY_TEST_HELPER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_native_library.h"

namespace media {

// This class loads the library containing External Clear Key. The library is
// loaded and initialized in the constructor, and unloaded in the destructor.
class ExternalClearKeyTestHelper {
 public:
  ExternalClearKeyTestHelper();

  ExternalClearKeyTestHelper(const ExternalClearKeyTestHelper&) = delete;
  ExternalClearKeyTestHelper& operator=(const ExternalClearKeyTestHelper&) =
      delete;

  ~ExternalClearKeyTestHelper();

  std::string KeySystemName() { return "org.chromium.externalclearkey"; }
  base::FilePath LibraryPath() { return library_path_; }

 private:
  // Methods to load and unload the library. Required as the compiler
  // doesn't like ASSERTs in the constructor/destructor.
  void LoadLibrary();
  void UnloadLibrary();

  // Keep a reference to the loaded library.
  base::FilePath library_path_;
  base::ScopedNativeLibrary library_;
};

}  // namespace media

#endif  // MEDIA_CDM_EXTERNAL_CLEAR_KEY_TEST_HELPER_H_
