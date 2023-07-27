// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/sandbox/media_sandbox.h"

#include <memory>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/size.h"
#include "cobalt/network/network_module.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/renderer/pipeline.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/renderer/submission.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/system_window/system_window.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"
#include "third_party/chromium/media/cobalt/ui/gfx/geometry/size.h"

namespace cobalt {
namespace media {
namespace sandbox {

namespace {
const int kViewportWidth = 1920;
const int kViewportHeight = 1080;
}  // namespace

class MediaSandbox::Impl {
 public:
  Impl(int argc, char** argv, const base::FilePath& trace_log_path);

  void RegisterFrameCB(const MediaSandbox::FrameCB& frame_cb);
  MediaModule* GetMediaModule() { return media_module_.get(); }
  loader::FetcherFactory* GetFetcherFactory() { return fetcher_factory_.get(); }
  render_tree::ResourceProvider* GetResourceProvider() {
    return renderer_module_->pipeline()->GetResourceProvider();
  }
  gfx::Size GetViewportSize() const {
    return gfx::Size(kViewportWidth, kViewportHeight);
  }

 private:
  void SetupAndSubmitScene();
  void AnimateCB(render_tree::ImageNode::Builder* image_node,
                 base::TimeDelta time);

  base::Lock lock_;
  base::MessageLoop message_loop_;
  MediaSandbox::FrameCB frame_cb_;

  std::unique_ptr<trace_event::ScopedTraceToFile> trace_to_file_;
  std::unique_ptr<network::NetworkModule> network_module_;
  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;
  base::EventDispatcher event_dispatcher_;
  // System window used as a render target.
  std::unique_ptr<system_window::SystemWindow> system_window_;
  std::unique_ptr<renderer::RendererModule> renderer_module_;
  std::unique_ptr<MediaModule> media_module_;
};

MediaSandbox::Impl::Impl(int argc, char** argv,
                         const base::FilePath& trace_log_path) {
  trace_to_file_.reset(new trace_event::ScopedTraceToFile(trace_log_path));
  // A one-per-process task scheduler is needed for usage of APIs in
  // base/post_task.h which will be used by some net APIs like
  // URLRequestContext;
  base::TaskScheduler::CreateAndStartWithDefaultParams("Cobalt TaskScheduler");
  network::NetworkModule::Options network_options;
  network_options.https_requirement = network::kHTTPSOptional;

  network_module_.reset(new network::NetworkModule(network_options));
  fetcher_factory_.reset(new loader::FetcherFactory(network_module_.get()));
  gfx::Size view_size(kViewportWidth, kViewportHeight);
  system_window_.reset(new system_window::SystemWindow(
      &event_dispatcher_,
      cobalt::math::Size(view_size.width(), view_size.height())));

  renderer::RendererModule::Options renderer_options;
  renderer_module_.reset(
      new renderer::RendererModule(system_window_.get(), renderer_options));
  media_module_.reset(
      new MediaModule(system_window_.get(), GetResourceProvider()));
  SetupAndSubmitScene();
}

void MediaSandbox::Impl::RegisterFrameCB(
    const MediaSandbox::FrameCB& frame_cb) {
  base::AutoLock auto_lock(lock_);
  frame_cb_ = frame_cb;
}

void MediaSandbox::Impl::AnimateCB(render_tree::ImageNode::Builder* image_node,
                                   base::TimeDelta time) {
  DCHECK(image_node);
  const auto render_target_size = renderer_module_->render_target()->GetSize();
  math::SizeF output_size(render_target_size.width(),
                          render_target_size.height());
  image_node->destination_rect = math::RectF(output_size);
  base::AutoLock auto_lock(lock_);
  image_node->source = frame_cb_.is_null() ? NULL : frame_cb_.Run(time);
}

void MediaSandbox::Impl::SetupAndSubmitScene() {
  scoped_refptr<render_tree::ImageNode> image_node =
      new render_tree::ImageNode(nullptr);
  render_tree::animations::AnimateNode::Builder animate_node_builder;

  animate_node_builder.Add(
      image_node, base::Bind(&Impl::AnimateCB, base::Unretained(this)));

  renderer_module_->pipeline()->Submit(
      renderer::Submission(new render_tree::animations::AnimateNode(
                               animate_node_builder, image_node),
                           base::TimeDelta()));
}

MediaSandbox::MediaSandbox(int argc, char** argv,
                           const base::FilePath& trace_log_path) {
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

render_tree::ResourceProvider* MediaSandbox::resource_provider() {
  DCHECK(impl_);
  return impl_->GetResourceProvider();
}

gfx::Size MediaSandbox::GetViewportSize() const {
  DCHECK(impl_);
  return impl_->GetViewportSize();
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
