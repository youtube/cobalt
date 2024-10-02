// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_
#define GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_

#include <stdint.h>
#include <string.h>

#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

// From glextchromium.h.
#ifndef GL_SYNC_TOKEN_SIZE_CHROMIUM
#define GL_SYNC_TOKEN_SIZE_CHROMIUM 24
#endif

namespace gpu {

// A Sync Token is a binary blob which represents a waitable fence sync
// on a particular command buffer namespace and id.
// See src/gpu/GLES2/extensions/CHROMIUM/CHROMIUM_sync_point.txt for more
// details.
struct GPU_EXPORT SyncToken {
  SyncToken();

  SyncToken(CommandBufferNamespace namespace_id,
            CommandBufferId command_buffer_id,
            uint64_t release_count);

  SyncToken(const SyncToken& other);
  SyncToken& operator=(const SyncToken& other);

  void Set(CommandBufferNamespace namespace_id,
           CommandBufferId command_buffer_id,
           uint64_t release_count) {
    namespace_id_ = namespace_id;
    command_buffer_id_ = command_buffer_id;
    release_count_ = release_count;
  }

  void Clear() {
    verified_flush_ = false;
    namespace_id_ = CommandBufferNamespace::INVALID;
    command_buffer_id_ = CommandBufferId();
    release_count_ = 0;
  }

  void SetVerifyFlush() {
    verified_flush_ = true;
  }

  bool HasData() const {
    return namespace_id_ != CommandBufferNamespace::INVALID;
  }

  int8_t* GetData() { return reinterpret_cast<int8_t*>(this); }

  const int8_t* GetConstData() const {
    return reinterpret_cast<const int8_t*>(this);
  }

  bool verified_flush() const { return verified_flush_; }
  CommandBufferNamespace namespace_id() const { return namespace_id_; }
  CommandBufferId command_buffer_id() const { return command_buffer_id_; }
  uint64_t release_count() const { return release_count_; }

  bool operator<(const SyncToken& other) const {
    return std::tie(namespace_id_, command_buffer_id_, release_count_) <
           std::tie(other.namespace_id_, other.command_buffer_id_,
                    other.release_count_);
  }

  bool operator==(const SyncToken& other) const {
    return verified_flush_ == other.verified_flush() &&
           namespace_id_ == other.namespace_id() &&
           command_buffer_id_ == other.command_buffer_id() &&
           release_count_ == other.release_count();
  }

  bool operator!=(const SyncToken& other) const { return !(*this == other); }

  std::string ToDebugString() const;

 private:
  bool verified_flush_;
  CommandBufferNamespace namespace_id_;
  CommandBufferId command_buffer_id_;
  uint64_t release_count_;
};

static_assert(sizeof(SyncToken) <= GL_SYNC_TOKEN_SIZE_CHROMIUM,
              "SyncToken size must not exceed GL_SYNC_TOKEN_SIZE_CHROMIUM");

// Remove redundant tokens such that it should be equivalent to wait on all
// tokens in the output instead of waiting on all input `tokens`.
GPU_EXPORT std::vector<SyncToken> ReduceSyncTokens(
    base::span<const SyncToken> tokens);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_SYNC_TOKEN_H_
