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

#ifndef STARBOARD_SHARED_UWP_DECODER_UTILS_H_
#define STARBOARD_SHARED_UWP_DECODER_UTILS_H_

#include <d3d11_1.h>
#include <wrl/client.h>

#include <algorithm>

#include "starboard/media.h"

namespace starboard {
namespace shared {
namespace uwp {

Microsoft::WRL::ComPtr<ID3D11Device> GetDirectX11Device(void* display);

// This is a utility function for finding entries with the method timestamp()
// in containers. Useful in decoders for getting data from buffers keeped in
// input queues;
template <typename Container>
const auto FindByTimestamp(const Container& container, int64_t timestamp) {
  return std::find_if(container.begin(), container.end(),
                      [=](const typename Container::value_type& value) {
                        return value->timestamp() == timestamp;
                      });
}

// This is a utility function for removing entries with the method timestamp()
// from containers. Useful in decoders for cleaning up input and output queues
// from expired buffers.
template <typename Container>
void RemoveByTimestamp(Container* container, int64_t timestamp) {
  auto to_remove =
      std::find_if(container->begin(), container->end(),
                   [=](const typename Container::value_type& value) {
                     return value->timestamp() == timestamp;
                   });
  if (to_remove != container->end()) {
    container->erase(to_remove);
  }
}

void UpdateHdrColorMetadataToCurrentDisplay(
    const SbMediaColorMetadata& color_metadata);

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_DECODER_UTILS_H_
