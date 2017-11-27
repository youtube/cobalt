// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_LOADER_MESH_MESH_DECODER_H_
#define COBALT_LOADER_MESH_MESH_DECODER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "cobalt/loader/decoder.h"
#include "cobalt/loader/mesh/mesh_projection.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace loader {
namespace mesh {

// |MeshDecoder| decodes mesh projections encoded in the format defined at:
// https://github.com/google/spatial-media/blob/master/docs/spherical-video-v2-rfc.md#mesh-projection-box-mshp
class MeshDecoder : public Decoder {
 public:
  typedef base::Callback<void(const scoped_refptr<MeshProjection>&)>
      SuccessCallback;
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  MeshDecoder(render_tree::ResourceProvider* resource_provider,
              const SuccessCallback& success_callback,
              const ErrorCallback& error_callback);

  // This function is used for binding a callback to create a MeshDecoder.
  static scoped_ptr<Decoder> Create(
      render_tree::ResourceProvider* resource_provider,
      const SuccessCallback& success_callback,
      const ErrorCallback& error_callback) {
    return scoped_ptr<Decoder>(
        new MeshDecoder(resource_provider, success_callback, error_callback));
  }

  // From Decoder.
  void DecodeChunk(const char* data, size_t size) override;
  void Finish() override;
  bool Suspend() override;
  void Resume(render_tree::ResourceProvider* resource_provider) override;

 private:
  void ReleaseRawData();

  render_tree::ResourceProvider* resource_provider_;
  const SuccessCallback success_callback_;
  const ErrorCallback error_callback_;

  scoped_ptr<std::vector<uint8> > raw_data_;

  bool is_suspended_;
};

}  // namespace mesh
}  // namespace loader
}  // namespace cobalt

#endif  // COBALT_LOADER_MESH_MESH_DECODER_H_
