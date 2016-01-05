/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef GLIMP_GLES_PROGRAM_H_
#define GLIMP_GLES_PROGRAM_H_

#include "glimp/gles/program_impl.h"
#include "glimp/nb/ref_counted.h"
#include "glimp/nb/scoped_ptr.h"

namespace glimp {
namespace gles {

class Program : public nb::RefCountedThreadSafe<Program> {
 public:
  explicit Program(nb::scoped_ptr<ProgramImpl> impl);

 private:
  friend class nb::RefCountedThreadSafe<Program>;
  ~Program() {}

  nb::scoped_ptr<ProgramImpl> impl_;
};

}  // namespace gles
}  // namespace glimp

#endif  // GLIMP_GLES_PROGRAM_H_
