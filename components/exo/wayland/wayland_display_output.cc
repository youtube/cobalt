// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_output.h"

#include <cstring>

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/containers/cxx20_erase.h"
#include "base/task/single_thread_task_runner.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {
namespace {

void DoDelete(WaylandDisplayOutput* output, int retry_count) {
  // Retry if a client hasn't released the output yet, or if no client has
  // even made the initial binding yet.
  if (output->output_counts() > 0 || !output->had_registered_output()) {
    if (retry_count > 0) {
      // If we can't post the task successfully, just delete the output
      // resource now, otherwise we would leak memory.
      if (base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
              FROM_HERE, base::BindOnce(&DoDelete, output, retry_count - 1),
              WaylandDisplayOutput::kDeleteTaskDelay)) {
        return;
      } else {
        LOG(WARNING) << "Failed to post delayed deletion task for "
                        "WaylandDisplayOutput with display id="
                     << output->id()
                     << " and remaining retry count: " << retry_count;
      }
    } else {
      LOG(WARNING)
          << "Timed out waiting for clients to unbind registered output for id="
          << output->id()
          << " with remaining bound outputs=" << output->output_counts();
    }
  }
  delete output;
}

}  // namespace

WaylandDisplayOutput::WaylandDisplayOutput(int64_t id) : id_(id) {}

WaylandDisplayOutput::~WaylandDisplayOutput() {
  // Empty the output_ids_ so that Unregister will be no op.
  auto ids = std::move(output_ids_);
  for (auto pair : ids) {
    wl_resource_destroy(pair.second);
  }

  if (global_) {
    wl_global_destroy(global_);
  }
}

void WaylandDisplayOutput::OnDisplayRemoved() {
  if (global_) {
    wl_global_remove(global_);
  }

  is_destructing_ = true;

  if (!base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, base::BindOnce(&DoDelete, this, kDeleteRetries),
          kDeleteTaskDelay)) {
    // If we can't schedule the delete task, just delete now to not leak memory.
    LOG(WARNING) << "Failed to post initial delayed deletion task for "
                    "WaylandDisplayOutput with display id="
                 << id();
    delete this;
  }
}

int64_t WaylandDisplayOutput::id() const {
  return id_;
}

void WaylandDisplayOutput::set_global(wl_global* global) {
  global_ = global;
}

void WaylandDisplayOutput::UnregisterOutput(wl_resource* output_resource) {
  base::EraseIf(output_ids_, [output_resource](auto& pair) {
    return pair.second == output_resource;
  });
}

void WaylandDisplayOutput::RegisterOutput(wl_resource* output_resource) {
  auto* client = wl_resource_get_client(output_resource);
  output_ids_.insert(std::make_pair(client, output_resource));
  had_registered_output_ = true;

  if (is_destructing_) {
    return;
  }

  // Notify All wl surfaces that a new output was added.
  wl_client_for_each_resource(
      client,
      [](wl_resource* resource, void*) {
        if (std::strcmp("wl_surface", wl_resource_get_class(resource)) == 0) {
          if (auto* surface = GetUserDataAs<Surface>(resource)) {
            surface->OnNewOutputAdded();
          }
        }
        return WL_ITERATOR_CONTINUE;
      },
      nullptr);
}

wl_resource* WaylandDisplayOutput::GetOutputResourceForClient(
    wl_client* client) {
  auto iter = output_ids_.find(client);
  if (iter == output_ids_.end()) {
    return nullptr;
  }
  return iter->second;
}

}  // namespace wayland
}  // namespace exo
