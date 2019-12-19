// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_OVERLAY_INFO_OVERLAY_INFO_REGISTRY_H_
#define COBALT_OVERLAY_INFO_OVERLAY_INFO_REGISTRY_H_

#include <sstream>
#include <string>

#include "starboard/types.h"

namespace cobalt {
namespace overlay_info {

// This class allows register of arbitrary overlay information from anywhere
// inside Cobalt.  It also allows a consumer to retrieve and clear all
// registered info, such info will be displayed as overlay.  The data is stored
// as std::string internally.
// The class is thread safe and all its methods can be accessed from any thread.
// On average it expects to have less than 10 such info registered per frame.
//
// The info is stored in the following format:
//   name0,value0,name1,value1,...
// Binary data will be converted into hex string before storing.
class OverlayInfoRegistry {
 public:
  static const char kDelimiter = ',';  // ':' doesn't work with some scanners
  // The size of category and data combined should be less than or equal to
  // kMaxSizeOfData - 1.  The extra room of one byte is used by the delimiter.
  static const size_t kMaxSizeOfData = 255;
  // The app DCHECKs if the number of pending overlay exceeds the following
  // value.
  static const size_t kMaxNumberOfPendingOverlayInfo = 1024;

  static void Disable();

  // Both |category| and |data| cannot contain the delimiter.
  static void Register(const char* category, const char* data);
  // Both |category| and |data| cannot contain the delimiter.
  static void Register(const char* category, const void* data,
                       size_t data_size);
  // Both |category| and |data| cannot contain the delimiter.
  template <typename T>
  static void Register(const char* category, T data) {
    std::stringstream ss;
    ss << data;
    Register(category, ss.str().c_str());
  }

  static void RetrieveAndClear(std::string* infos);
};

}  // namespace overlay_info
}  // namespace cobalt

#endif  // COBALT_OVERLAY_INFO_OVERLAY_INFO_REGISTRY_H_
