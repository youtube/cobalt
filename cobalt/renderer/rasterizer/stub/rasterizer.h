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

#ifndef COBALT_RENDERER_RASTERIZER_STUB_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_STUB_RASTERIZER_H_

#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace stub {

class Rasterizer : public rasterizer::Rasterizer {
 public:
  Rasterizer();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options) override {}

  render_tree::ResourceProvider* GetResourceProvider() override;

  void MakeCurrent() override {}

 private:
  render_tree::ResourceProviderStub resource_provider_stub_;
};

}  // namespace stub
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_STUB_RASTERIZER_H_
