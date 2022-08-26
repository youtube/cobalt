/* Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/message_event.h"

#include <string>
#include <utility>

#include "base/basictypes.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/v8c/conversion_helpers.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/script/v8c/v8c_exception_state.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"
#include "v8/include/v8.h"

namespace {
const char* const kResponseTypes[] = {"text", "blob", "arraybuffer", "any"};

COMPILE_ASSERT(arraysize(kResponseTypes) ==
                   cobalt::web::MessageEvent::kResponseTypeMax,
               enum_and_array_size_mismatch);

}  // namespace

namespace cobalt {
namespace web {

// static
std::string MessageEvent::GetResponseTypeAsString(
    const MessageEvent::ResponseType response_type) {
  DCHECK_GE(response_type, kText);
  DCHECK_LT(response_type, kResponseTypeMax);
  return kResponseTypes[response_type];
}

// static
MessageEvent::ResponseType MessageEvent::GetResponseType(
    base::StringPiece to_match) {
  for (std::size_t i = 0; i != arraysize(kResponseTypes); ++i) {
    if (strncmp(kResponseTypes[i], to_match.data(), to_match.size()) == 0) {
      return MessageEvent::ResponseType(i);
    }
  }
  return kResponseTypeMax;
}

MessageEvent::Response MessageEvent::data(
    script::EnvironmentSettings* settings) const {
  if (!data_ && !data_io_buffer_) {
    return Response("");
  }

  script::GlobalEnvironment* global_environment = nullptr;
  if (response_type_ == kBlob || response_type_ == kArrayBuffer ||
      response_type_ == kAny) {
    DCHECK(settings);
    global_environment =
        base::polymorphic_downcast<web::EnvironmentSettings*>(settings)
            ->context()
            ->global_environment();
    DCHECK(global_environment);
  }

  switch (response_type_) {
    case kText: {
      // TODO: Find a way to remove two copies of data here.
      std::string string_response(data_io_buffer_->data(),
                                  data_io_buffer_->size());
      return Response(string_response);
    }
    case kBlob:
    case kArrayBuffer: {
      auto buffer_copy = script::ArrayBuffer::New(
          global_environment, data_io_buffer_->data(), data_io_buffer_->size());
      script::Handle<script::ArrayBuffer> response_buffer =
          std::move(buffer_copy);
      if (response_type_ == kBlob) {
        DCHECK(settings);
        scoped_refptr<web::Blob> blob =
            new web::Blob(settings, response_buffer);
        return Response(blob);
      }
      return Response(response_buffer);
    }
    case kAny: {
      v8::Isolate* isolate = global_environment->isolate();
      script::v8c::EntryScope entry_scope(isolate);
      return Response(script::Handle<script::ValueHandle>(
          std::move(script::DeserializeScriptValue(isolate, *data_))));
    }
    default:
      NOTREACHED() << "Invalid response type.";
      return Response("");
  }
}

}  // namespace web
}  // namespace cobalt
