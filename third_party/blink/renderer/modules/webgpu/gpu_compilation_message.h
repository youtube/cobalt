// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPILATION_MESSAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPILATION_MESSAGE_H_

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class GPUCompilationMessage : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUCompilationMessage(
      String message,
      WGPUCompilationMessageType type = WGPUCompilationMessageType_Error,
      uint64_t line_num = 0,
      uint64_t line_pos = 0,
      uint64_t offset = 0,
      uint64_t length = 0);

  GPUCompilationMessage(const GPUCompilationMessage&) = delete;
  GPUCompilationMessage& operator=(const GPUCompilationMessage&) = delete;

  const String& message() const { return message_; }
  const String& type() const { return type_string_; }
  uint64_t lineNum() const { return line_num_; }
  uint64_t linePos() const { return line_pos_; }
  uint64_t offset() const { return offset_; }
  uint64_t length() const { return length_; }

 private:
  const String message_;
  String type_string_;
  const uint64_t line_num_;
  const uint64_t line_pos_;
  const uint64_t offset_;
  const uint64_t length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMPILATION_MESSAGE_H_
