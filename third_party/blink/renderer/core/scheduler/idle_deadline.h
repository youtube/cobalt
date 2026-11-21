// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_IDLE_DEADLINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_IDLE_DEADLINE_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CORE_EXPORT IdleDeadline : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum class CallbackType {
    kCalledWhenIdle,
    kCalledByTimeout,
  };

  IdleDeadline(base::TimeTicks deadline,
               bool cross_origin_isolated_capability,
               CallbackType);

  double timeRemaining() const;

  bool didTimeout() const {
    return callback_type_ == CallbackType::kCalledByTimeout;
  }

 private:
  base::TimeTicks deadline_;
  bool cross_origin_isolated_capability_;
  CallbackType callback_type_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCHEDULER_IDLE_DEADLINE_H_
