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

#include "cobalt/media/media_module.h"

#include "base/compiler_specific.h"
#include "cobalt/math/size.h"
#include "cobalt/media/media_module_stub.h"
#include "media/audio/shell_audio_streamer.h"

namespace cobalt {
namespace media {

// There is no media stack on Windows and the XB1 media stack cannot be used
// directly on Windows. So MediaModule on windows does nothing.
scoped_ptr<MediaModule> MediaModule::Create(
    const math::Size& output_size,
    render_tree::ResourceProvider* resource_provider, const Options& options) {
  UNREFERENCED_PARAMETER(output_size);
  UNREFERENCED_PARAMETER(resource_provider);
  UNREFERENCED_PARAMETER(options);
  return make_scoped_ptr(new MediaModuleStub);
}

}  // namespace media
}  // namespace cobalt

// ShellAudioStreamer is used by H5vccAudioConfigArray.  Ideally we should get
// the ShellAudioStreamer from ShellMediaPlatform.  But before that's done, we
// have to have the following stub implementation to ensure that Cobalt links on
// Windows.
namespace media {

// static
void ShellAudioStreamer::Initialize() { NOTIMPLEMENTED(); }

// static
void ShellAudioStreamer::Terminate() { NOTIMPLEMENTED(); }

// static
ShellAudioStreamer* ShellAudioStreamer::Instance() {
  NOTIMPLEMENTED();
  return NULL;
}

}  // namespace media
