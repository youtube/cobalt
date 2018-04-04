// Copyright 2018 Google Inc. All Rights Reserved.
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

#include <vector>

#include "starboard/types.h"

namespace cobalt {
namespace overlay_info {

// This class allows register of arbitrary overlay information in the form of
// string or binary data from anywhere inside Cobalt.  It also allows a consumer
// to retrieve and clear all registered info, such info will be displayed as
// overlay.
// The class is thread safe and all its methods can be accessed from any thread.
// On average it expects to have less than 10 such info registered per frame.
//
// The binary data or string are stored in the following format:
// [<one byte size> <size bytes data>]*
// Each data entry contains a string category and some binary data, separated by
// the delimiter '<'.
// For example, the overlay infos ("media", "pts"), ("renderer", "fps\x60"), and
// ("dom", "keydown") will be stored as
//   '\x09', 'm', 'e', 'd', 'i', 'a', '<', 'p', 't', 's',
//   '\x0d', 'r', 'e', 'n', 'd', 'e', 'r', 'e', 'r', '<', 'f', 'p', 's', '\x60',
//   '\x0b', 'd', 'o', 'm', '<', 'k', 'e', 'y', 'd', 'o', 'w', 'n',
// and the size of the vector will be 36.  Note that C strings won't be NULL
// terminated and their sizes are calculated by the size of the data.
class OverlayInfoRegistry {
 public:
  static const uint8_t kDelimiter = '<';  // ':' doesn't work with some scanners
  // The size of category and data combined should be less than or equal to
  // kMaxSizeOfData - 1.  The extra room of one byte is used by the delimiter.
  static const size_t kMaxSizeOfData = 255;
  // The app DCHECKs if the number of pending overlay exceeds the following
  // value.
  static const size_t kMaxNumberOfPendingOverlayInfo = 1024;

  static void Disable();

  // |category| cannot contain ':'.  The sum of size of |category| and |string|
  // cannot exceed 254.  It leaves room for the delimiter.
  static void Register(const char* category, const char* str);
  // |category| cannot contain ':'.  The sum of size of |category| and |data|
  // cannot exceed 254.  It leaves room for the delimiter.
  static void Register(const char* category, const void* data,
                       size_t data_size);
  static void RetrieveAndClear(std::vector<uint8_t>* infos);
};

}  // namespace overlay_info
}  // namespace cobalt

#endif  // COBALT_OVERLAY_INFO_OVERLAY_INFO_REGISTRY_H_
