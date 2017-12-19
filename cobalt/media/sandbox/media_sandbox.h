// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SANDBOX_MEDIA_SANDBOX_H_
#define COBALT_MEDIA_SANDBOX_MEDIA_SANDBOX_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/media_module.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace media {
namespace sandbox {

class MediaSandbox {
 public:
  typedef render_tree::Image Image;
  typedef base::Callback<scoped_refptr<Image>(const base::TimeDelta&)> FrameCB;

  MediaSandbox(int argc, char** argv, const FilePath& trace_log_path);
  ~MediaSandbox();

  // This function registers a callback so the MediaSandbox instance can pull
  // the current frame to render.  Call this function will a null callback will
  // reset the frame callback.
  void RegisterFrameCB(const FrameCB& frame_cb);
  MediaModule* GetMediaModule();
  loader::FetcherFactory* GetFetcherFactory();
  render_tree::ResourceProvider* resource_provider();

 private:
  class Impl;

  Impl* impl_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaSandbox);
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SANDBOX_MEDIA_SANDBOX_H_
