// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_MOJO_COMMON_STARBOARD_EMPTY_MEDIA_RESOURCE_H_
#define MEDIA_MOJO_COMMON_STARBOARD_EMPTY_MEDIA_RESOURCE_H_

#include <vector>

#include "media/base/demuxer_stream.h"
#include "media/base/media_resource.h"

namespace media {

// Empty media resource passed to MojoRenderer or MojoRendererService to avoid
// creating unused MojoDemuxerStreamImpl / MediaResourceShim instances and
// message pipes when the bypass bridge is active.
//
// Lifetime and Ownership:
// This class can be instantiated as a global static (e.g., base::NoDestructor)
// or owned via std::unique_ptr<MediaResource>. When passed to a Renderer, its
// lifetime must match or exceed the lifetime of the Renderer.
//
// Threading Model:
// This class is stateless and thread-safe, and its methods can be safely
// called from any thread.
class EmptyMediaResource : public MediaResource {
 public:
  EmptyMediaResource() = default;
  ~EmptyMediaResource() override = default;

  std::vector<DemuxerStream*> GetAllStreams() override { return {}; }
};

}  // namespace media

#endif  // MEDIA_MOJO_COMMON_STARBOARD_EMPTY_MEDIA_RESOURCE_H_
