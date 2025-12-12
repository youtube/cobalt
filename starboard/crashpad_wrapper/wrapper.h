// Copyright 2020 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STARBOARD_CRASHPAD_WRAPPER_WRAPPER_H_
#define STARBOARD_CRASHPAD_WRAPPER_WRAPPER_H_

#include <string>

#include "starboard/elf_loader/evergreen_info.h"  // nogncheck

namespace crashpad {

// The key name used in Crashpad for the version annotation.
extern const char kCrashpadVersionKey[];

// The key name used in Crashpad for the product annotation.
extern const char kCrashpadProductKey[];

// The key name used in Crashpad for the user_agent_string annotation.
extern const char kCrashpadUserAgentStringKey[];

// The key name used in Crashpad for the cert_scope annotation.
extern const char kCrashpadCertScopeKey[];

// Installs a signal handler to handle a crash. The signal handler will launch a
// Crashpad handler process in response to a crash.
// |ca_certificates_path| is the absolute path to a directory containing
// Cobalt's trusted Certificate Authority (CA) root certificates, and must be
// passed so that the certificates can be accessed by the handler process during
// upload.
void InstallCrashpadHandler(const std::string& ca_certificates_path);

bool AddEvergreenInfoToCrashpad(EvergreenInfo evergreen_info);

// Associates the given value with the given key in Crashpad's map of
// annotations, updating the existing value if the map already contains the
// key. Returns true on success and false on failure.
bool InsertCrashpadAnnotation(const char* key, const char* value);

}  // namespace crashpad

#endif  // STARBOARD_CRASHPAD_WRAPPER_WRAPPER_H_
