/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_STUB_GLES_BUFFER_IMPL_H_
#define GLIMP_STUB_GLES_BUFFER_IMPL_H_

#include "glimp/gles/buffer_impl.h"
#include "nb/ref_counted.h"

namespace glimp {
namespace gles {

class BufferImplStub : public BufferImpl {
 public:
  BufferImplStub();
  ~BufferImplStub() override {}

  bool Allocate(Usage usage, size_t size) override;
  bool SetData(intptr_t offset, size_t size, const void* data) override;

  void* Map() override;
  bool Unmap() override;

 private:
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_STUB_GLES_BUFFER_IMPL_H_
