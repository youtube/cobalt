// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_helper.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

typedef std::map<int, RenderWidgetHelper*> WidgetHelperMap;
base::LazyInstance<WidgetHelperMap>::DestructorAtExit g_widget_helpers =
    LAZY_INSTANCE_INITIALIZER;

void AddWidgetHelper(int render_process_id,
                     const scoped_refptr<RenderWidgetHelper>& widget_helper) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // We don't care if RenderWidgetHelpers overwrite an existing process_id. Just
  // want this to be up to date.
  g_widget_helpers.Get()[render_process_id] = widget_helper.get();
}

}  // namespace

RenderWidgetHelper::FrameTokens::FrameTokens(
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::DocumentToken& document_token)
    : frame_token(frame_token),
      devtools_frame_token(devtools_frame_token),
      document_token(document_token) {}

RenderWidgetHelper::FrameTokens::FrameTokens(const FrameTokens& other) =
    default;

RenderWidgetHelper::FrameTokens& RenderWidgetHelper::FrameTokens::operator=(
    const FrameTokens& other) = default;

RenderWidgetHelper::FrameTokens::~FrameTokens() = default;

RenderWidgetHelper::RenderWidgetHelper() : render_process_id_(-1) {}

RenderWidgetHelper::~RenderWidgetHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Delete this RWH from the map if it is found.
  WidgetHelperMap& widget_map = g_widget_helpers.Get();
  auto it = widget_map.find(render_process_id_);
  if (it != widget_map.end() && it->second == this)
    widget_map.erase(it);
}

void RenderWidgetHelper::Init(int render_process_id) {
  render_process_id_ = render_process_id;

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&AddWidgetHelper, render_process_id_,
                                base::WrapRefCounted(this)));
}

int RenderWidgetHelper::GetNextRoutingID() {
  int next_routing_id = next_routing_id_.GetNext();
  CHECK_LT(next_routing_id, std::numeric_limits<int>::max());
  return next_routing_id + 1;
}

bool RenderWidgetHelper::TakeFrameTokensForFrameRoutingID(
    int32_t routing_id,
    blink::LocalFrameToken& frame_token,
    base::UnguessableToken& devtools_frame_token,
    blink::DocumentToken& document_token) {
  base::AutoLock lock(frame_token_map_lock_);
  auto iter = frame_token_routing_id_map_.find(routing_id);
  if (iter == frame_token_routing_id_map_.end())
    return false;
  frame_token = iter->second.frame_token;
  devtools_frame_token = iter->second.devtools_frame_token;
  document_token = iter->second.document_token;
  frame_token_routing_id_map_.erase(iter);
  return true;
}

void RenderWidgetHelper::StoreNextFrameRoutingID(
    int32_t routing_id,
    const blink::LocalFrameToken& frame_token,
    const base::UnguessableToken& devtools_frame_token,
    const blink::DocumentToken& document_token) {
  base::AutoLock lock(frame_token_map_lock_);
  bool result =
      frame_token_routing_id_map_
          .emplace(routing_id, FrameTokens(frame_token, devtools_frame_token,
                                           document_token))
          .second;
  DCHECK(result);
}

}  // namespace content
