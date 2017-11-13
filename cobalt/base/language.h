// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BASE_LANGUAGE_H_
#define COBALT_BASE_LANGUAGE_H_

#include <string>

namespace base {

// Gets the system language and ISO 3166-1 country code.
// NOTE: should be in the format described by bcp47.
// http://www.rfc-editor.org/rfc/bcp/bcp47.txt
// Example: "en-US" or "de"
std::string GetSystemLanguage();

// Gets the system language and ISO 15924 script code.
// NOTE: should be in the format described by bcp47.
// http://www.rfc-editor.org/rfc/bcp/bcp47.txt
// Example: "en-US" or "de"
std::string GetSystemLanguageScript();

}  // namespace base

#endif  // COBALT_BASE_LANGUAGE_H_
