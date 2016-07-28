/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DEPRECATED_PLATFORM_DELEGATE_H_
#define COBALT_DEPRECATED_PLATFORM_DELEGATE_H_

#include <string>

#include "cobalt/export.h"

namespace cobalt {
namespace deprecated {

// Abstract base class for handling per-platform details.
// Each platform must implement a PlatformDelegate that derives from this
// class, and performs any required startup and shutdown operations.
// The factory method PlatformDelegate::Create() will be called, which should
// construct the derived class.
class COBALT_EXPORT PlatformDelegate {
 public:
  // Must be first method called by main()
  static void Init();
  // Must be called during AtExit callback or before exit.
  static void Teardown();

  static PlatformDelegate* Get() { return instance_; }

  // Gets the system language.
  // NOTE: should be in the format described by bcp47.
  // http://www.rfc-editor.org/rfc/bcp/bcp47.txt
  // Example: "en-US" or "de"
  virtual std::string GetSystemLanguage();

  // Some platforms swap the meaning of the 'Enter' and 'Back' keys in
  // certain regions.
  virtual bool AreKeysReversed() const;

  virtual std::string GetPlatformName() const = 0;

  virtual bool IsUserLogRegistrationSupported() const;

  // Thread-safe registration of a user log index, associating it with the
  // specified label and memory address. This is used by some platforms to
  // provide additional context with crashes.
  //
  // Returns true if the registration is successful.
  virtual bool RegisterUserLog(int user_log_index, const char* label,
                               const void* address, size_t size) const;
  virtual bool DeregisterUserLog(int user_log_index) const;

  const std::string& dir_source_root() const { return dir_source_root_; }
  const std::string& game_content_path() const { return game_content_path_; }
  const std::string& screenshot_output_path() const {
    return screenshot_output_path_;
  }
  const std::string& logging_output_path() const {
    return logging_output_path_;
  }
  const std::string& temp_path() const { return temp_path_; }

 protected:
  // Class can only be created via Init() / Teardown().
  PlatformDelegate();
  virtual ~PlatformDelegate() = 0;

  std::string dir_source_root_;
  std::string game_content_path_;
  std::string screenshot_output_path_;
  std::string logging_output_path_;
  std::string temp_path_;

 private:
  // Each platform implements the Create() function to return its own
  // class derived from PlatformDelegate.
  static PlatformDelegate* Create();
  static PlatformDelegate* instance_;
};

}   // namespace deprecated
}   // namespace cobalt

#endif  // COBALT_DEPRECATED_PLATFORM_DELEGATE_H_
