// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/quic_connection_context.h"

#include "absl/base/attributes.h"
#include "quiche/common/quiche_text_utils.h"

#if defined(STARBOARD)
#include <pthread.h>

#include "base/check_op.h"
#include "starboard/thread.h"
#endif

namespace quic {

#if defined(STARBOARD)

namespace {
ABSL_CONST_INIT pthread_once_t s_once_flag = PTHREAD_ONCE_INIT;
ABSL_CONST_INIT pthread_key_t s_thread_local_key = 0;

void InitThreadLocalKey() {
  pthread_key_create(&s_thread_local_key, NULL);
  DCHECK(s_thread_local_key);
}

void EnsureThreadLocalKeyInited() {
  pthread_once(&s_once_flag, InitThreadLocalKey);
  DCHECK(s_thread_local_key);
}

QuicConnectionContext* GetQuicConnectionContext() {
  EnsureThreadLocalKeyInited();
  return static_cast<QuicConnectionContext*>(
      pthread_getspecific(s_thread_local_key));
}
}  // namespace

std::string QuicConnectionProcessPacketContext::DebugString() const {
  if (decrypted_payload.empty()) {
    return "Not processing packet";
  }

  return absl::StrCat("current_frame_offset: ", current_frame_offset,
                      ", payload size: ", decrypted_payload.size(),
                      ", payload hexdump: ",
                      quiche::QuicheTextUtils::HexDump(decrypted_payload));
}

// static
QuicConnectionContext* QuicConnectionContext::Current() {
  return GetQuicConnectionContext();
}

QuicConnectionContextSwitcher::QuicConnectionContextSwitcher(
    QuicConnectionContext* new_context)
    : old_context_(QuicConnectionContext::Current()) {
  EnsureThreadLocalKeyInited();
  pthread_setspecific(s_thread_local_key, new_context);
  if (new_context && new_context->tracer) {
    new_context->tracer->Activate();
  }
}

QuicConnectionContextSwitcher::~QuicConnectionContextSwitcher() {
  QuicConnectionContext* current = QuicConnectionContext::Current();
  if (current && current->tracer) {
    current->tracer->Deactivate();
  }

  EnsureThreadLocalKeyInited();
  pthread_setspecific(s_thread_local_key, old_context_);
}

#else  // defined(STARBOARD)

namespace {
ABSL_CONST_INIT thread_local QuicConnectionContext* current_context = nullptr;
}  // namespace

std::string QuicConnectionProcessPacketContext::DebugString() const {
  if (decrypted_payload.empty()) {
    return "Not processing packet";
  }

  return absl::StrCat("current_frame_offset: ", current_frame_offset,
                      ", payload size: ", decrypted_payload.size(),
                      ", payload hexdump: ",
                      quiche::QuicheTextUtils::HexDump(decrypted_payload));
}

// static
QuicConnectionContext* QuicConnectionContext::Current() {
  return current_context;
}

QuicConnectionContextSwitcher::QuicConnectionContextSwitcher(
    QuicConnectionContext* new_context)
    : old_context_(QuicConnectionContext::Current()) {
  current_context = new_context;
  if (new_context && new_context->tracer) {
    new_context->tracer->Activate();
  }
}

QuicConnectionContextSwitcher::~QuicConnectionContextSwitcher() {
  QuicConnectionContext* current = QuicConnectionContext::Current();
  if (current && current->tracer) {
    current->tracer->Deactivate();
  }
  current_context = old_context_;
}

#endif  // defined(STARBOARD)

}  // namespace quic
