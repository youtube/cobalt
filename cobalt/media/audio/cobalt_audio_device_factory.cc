// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/audio/cobalt_audio_device_factory.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "cobalt/media/audio/audio_helpers.h"
#include "cobalt/media/audio/cobalt_audio_renderer_sink.h"

namespace media {

namespace {

// When enabled, CobaltAudioRendererSink runs SbAudioSinkCreate() and
// SbAudioSinkDestroy() on a background ThreadPool sequence instead of
// blocking the renderer main thread.
BASE_FEATURE(kCobaltAsyncAudioSinkCreate,
             "CobaltAsyncAudioSinkCreate",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Returns the ThreadPool sequence on which CobaltAudioRendererSink creates and
// destroys Starboard audio sinks, or nullptr when kCobaltAsyncAudioSinkCreate
// is disabled and those operations should stay on the calling thread. One
// sequence is shared by all sinks so creations and destructions are ordered.
// TODO(b/581424327): The cached task runner stays bound to the first ThreadPool
// instance, so tests that enable the feature across multiple TaskEnvironments
// would post to a destroyed pool. Fix this before adding unit tests.
scoped_refptr<base::SequencedTaskRunner> GetAudioSinkTaskRunner() {
  if (!base::FeatureList::IsEnabled(kCobaltAsyncAudioSinkCreate)) {
    return nullptr;
  }
  static const base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>>
      task_runner(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING}));
  return *task_runner;
}

}  // namespace

CobaltAudioDeviceFactory::CobaltAudioDeviceFactory() {
  LOG(INFO) << "Register CobaltAudioDeviceFactory";
}

CobaltAudioDeviceFactory::~CobaltAudioDeviceFactory() {
  LOG(INFO) << "Unregister CobaltAudioDeviceFactory";
}

scoped_refptr<media::AudioRendererSink>
CobaltAudioDeviceFactory::NewAudioRendererSink(
    blink::WebAudioDeviceSourceType source_type,
    const blink::LocalFrameToken& frame_token,
    const media::AudioSinkParameters& params) {
  return base::MakeRefCounted<media::CobaltAudioRendererSink>(
      GetAudioSinkTaskRunner());
}

OutputDeviceInfo CobaltAudioDeviceFactory::GetOutputDeviceInfo(
    const blink::LocalFrameToken& frame_token,
    const std::string& device_id) {
  return GetPreferredOutputParameters();
}
}  // namespace media
