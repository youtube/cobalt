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

#include "cobalt/media/sandbox/media_sandbox.h"

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/synchronization/lock.h"
#include "cobalt/base/init_cobalt.h"
#include "cobalt/math/size_f.h"
#include "cobalt/network/network_module.h"
#include "cobalt/render_tree/animations/node_animations_map.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/system_window/create_system_window.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

namespace cobalt {
namespace media {
namespace sandbox {

namespace {

// This class is introduced to ensure that CobaltInit() is called after AtExit
// is initialized.
class CobaltInit {
 public:
  CobaltInit(int argc, char** argv) { InitCobalt(argc, argv); }
};

}  // namespace

class MediaSandbox::Impl {
 public:
  Impl(int argc, char** argv, const FilePath& trace_log_path)
      : cobalt_init_(argc, argv),
        trace_to_file_(trace_log_path),
        fetcher_factory_(&network_module_),
        system_window_(system_window::CreateSystemWindow()),
        renderer_module_(system_window_.get(),
                         renderer::RendererModule::Options()),
        media_module_(MediaModule::Create(
            renderer_module_.pipeline()->GetResourceProvider())) {
    SetupAndSubmitScene();
  }

  void RegisterFrameCB(const MediaSandbox::FrameCB& frame_cb) {
    base::AutoLock auto_lock(lock_);
    frame_cb_ = frame_cb;
  }

  MediaModule* GetMediaModule() { return media_module_.get(); }

  loader::FetcherFactory* GetFetcherFactory() { return &fetcher_factory_; }

 private:
  void SetupAndSubmitScene() {
    scoped_refptr<render_tree::ImageNode> image_node =
        new render_tree::ImageNode(NULL);
    render_tree::animations::NodeAnimationsMap::Builder
        node_animations_map_builder;

    node_animations_map_builder.Add(
        image_node, base::Bind(&Impl::AnimateCB, base::Unretained(this)));

    renderer_module_.pipeline()->Submit(renderer::Pipeline::Submission(
        image_node, new render_tree::animations::NodeAnimationsMap(
                        node_animations_map_builder.Pass()),
        base::TimeDelta()));
  }
  void AnimateCB(render_tree::ImageNode::Builder* image_node,
                 base::TimeDelta time) {
    DCHECK(image_node);
    math::SizeF output_size(
        renderer_module_.render_target()->GetSurfaceInfo().size);
    image_node->destination_size = output_size;
    base::AutoLock auto_lock(lock_);
    image_node->source = frame_cb_.is_null() ? NULL : frame_cb_.Run(time);
  }

  base::Lock lock_;
  base::AtExitManager at_exit;
  CobaltInit cobalt_init_;
  trace_event::ScopedTraceToFile trace_to_file_;
  MessageLoop message_loop_;
  network::NetworkModule network_module_;
  loader::FetcherFactory fetcher_factory_;
  // System window used as a render target.
  scoped_ptr<system_window::SystemWindow> system_window_;
  renderer::RendererModule renderer_module_;
  scoped_ptr<MediaModule> media_module_;
  MediaSandbox::FrameCB frame_cb_;
};

MediaSandbox::MediaSandbox(int argc, char** argv,
                           const FilePath& trace_log_path) {
  impl_ = new Impl(argc, argv, trace_log_path);
}

MediaSandbox::~MediaSandbox() {
  DCHECK(impl_);
  delete impl_;
}

void MediaSandbox::RegisterFrameCB(const FrameCB& frame_cb) {
  DCHECK(impl_);
  impl_->RegisterFrameCB(frame_cb);
}

MediaModule* MediaSandbox::GetMediaModule() {
  DCHECK(impl_);
  return impl_->GetMediaModule();
}

loader::FetcherFactory* MediaSandbox::GetFetcherFactory() {
  DCHECK(impl_);
  return impl_->GetFetcherFactory();
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
