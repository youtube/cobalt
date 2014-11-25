/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/pipeline.h"

#include "base/bind.h"

namespace cobalt {
namespace renderer {

namespace {
// The frequency we signal a new rasterization fo the render tree.
const float kRefreshRate = 60.0f;
}  // namespace

Pipeline::Pipeline(Rasterizer* rasterizer)
    : refresh_rate_(kRefreshRate),
      rasterizer_(rasterizer),
      rasterizer_thread_(base::in_place, "Rasterizer") {
  // The actual Pipeline can be constructed from any thread, but we want
  // rasterizer_thread_checker_ to be associated with the rasterizer thread,
  // so we detach it here and let it reattach itself to the rasterizer thread
  // when CalledOnValidThread() is called on rasterizer_thread_checker_ below.
  rasterizer_thread_checker_.DetachFromThread();
  rasterizer_thread_->Start();
}

Pipeline::~Pipeline() {
  // Submit a shutdown task to the rasterizer thread so that it can shutdown
  // anything that must be shutdown from that thread.
  rasterizer_thread_->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&Pipeline::ShutdownRasterizerThread, base::Unretained(this)));

  rasterizer_thread_ = base::nullopt;
}

void Pipeline::Submit(const scoped_refptr<render_tree::Node>& render_tree) {
  // Execute the actual set of the new render tree on the rasterizer tree.
  rasterizer_thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&Pipeline::SetNewRenderTree, base::Unretained(this),
                            render_tree));
}

void Pipeline::SetNewRenderTree(
    const scoped_refptr<render_tree::Node>& render_tree) {
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  DCHECK(render_tree.get());

  current_tree_ = render_tree;

  // Start the rasterization timer if it is not yet started.
  if (!refresh_rate_timer_) {
    refresh_rate_timer_.emplace(
        FROM_HERE,
        base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond *
                                          1.0f / kRefreshRate),
        base::Bind(&Pipeline::RasterizeCurrentTree, base::Unretained(this)),
        true);
    refresh_rate_timer_->Reset();
  }
}

void Pipeline::RasterizeCurrentTree() {
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  DCHECK(current_tree_.get());

  // Rasterize the last submitted render tree.
  rasterizer_->Submit(current_tree_);
}

void Pipeline::ShutdownRasterizerThread() {
  DCHECK(rasterizer_thread_checker_.CalledOnValidThread());
  // Stop and shutdown the raterizer timer.
  refresh_rate_timer_ = base::nullopt;
}

}  // namespace renderer
}  // namespace cobalt
