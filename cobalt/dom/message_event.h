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

#ifndef COBALT_DOM_MESSAGE_EVENT_H_
#define COBALT_DOM_MESSAGE_EVENT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "cobalt/base/token.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/event.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/wrappable.h"
#include "net/base/io_buffer.h"

namespace cobalt {
namespace dom {

class MessageEvent : public dom::Event {
 public:
  typedef script::UnionType3<std::string, scoped_refptr<dom::Blob>,
                             script::Handle<script::ArrayBuffer> >
      ResponseType;
  // These response codes are ordered in the likelyhood of being used.
  // Keeping them in expected order will help make code faster.
  enum ResponseTypeCode { kText, kBlob, kArrayBuffer, kResponseTypeCodeMax };

  MessageEvent(base::Token type, script::EnvironmentSettings* settings,
               ResponseTypeCode response_type,
               const scoped_refptr<net::IOBufferWithSize>& data)
      : Event(type),
        settings_(settings),
        response_type_(response_type),
        data_(data) {}

  // Creates an event with its "initialized flag" unset.
  explicit MessageEvent(UninitializedFlag uninitialized_flag)
      : Event(uninitialized_flag) {}

  ResponseType data() const;

  // These helper functions are custom, and not in any spec.
  static std::string GetResponseTypeAsString(const ResponseTypeCode code);
  // This function returns kResponseTypeCodeMax if no match is found.
  static ResponseTypeCode GetResponseTypeCode(base::StringPiece to_match);

  DEFINE_WRAPPABLE_TYPE(MessageEvent)

 private:
  script::EnvironmentSettings* settings_;
  ResponseTypeCode response_type_;
  scoped_refptr<net::IOBufferWithSize> data_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MESSAGE_EVENT_H_
