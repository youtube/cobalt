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

#ifndef COBALT_WEB_MESSAGE_EVENT_H_
#define COBALT_WEB_MESSAGE_EVENT_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "cobalt/base/token.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/union_type.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/web/blob.h"
#include "cobalt/web/event.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_event_init.h"
#include "cobalt/web/message_port.h"
#include "net/base/io_buffer.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace web {

class MessageEvent : public Event {
 public:
  typedef script::UnionType4<std::string, scoped_refptr<Blob>,
                             script::Handle<script::ArrayBuffer>,
                             script::Handle<script::ValueHandle>>
      Response;


  // These response types are ordered in the likelihood of being used.
  // Keeping them in expected order will help make code faster.
  enum ResponseType { kText, kBlob, kArrayBuffer, kAny, kResponseTypeMax };

  // Creates an event with its "initialized flag" unset.
  explicit MessageEvent(UninitializedFlag uninitialized_flag)
      : Event(uninitialized_flag) {}

  explicit MessageEvent(const std::string& type) : Event(type) {}
  MessageEvent(base_token::Token type,
               std::unique_ptr<script::StructuredClone> structured_clone)
      : Event(type),
        response_type_(kAny),
        structured_clone_(std::move(structured_clone)) {}
  MessageEvent(base_token::Token type, ResponseType response_type,
               const scoped_refptr<net::IOBufferWithSize>& data)
      : Event(type), response_type_(response_type), data_io_buffer_(data) {}
  MessageEvent(const std::string& type, const MessageEventInit& init_dict);

  Response data(script::EnvironmentSettings* settings = nullptr);
  const std::string& origin() const { return origin_; }
  const std::string& last_event_id() const { return last_event_id_; }
  const scoped_refptr<EventTarget>& source() const { return source_; }
  script::Sequence<scoped_refptr<MessagePort>> ports() const { return ports_; }

  // These helper functions are custom, and not in any spec.
  void set_origin(const std::string& origin) { origin_ = origin; }
  void set_last_event_id(const std::string& last_event_id) {
    last_event_id_ = last_event_id;
  }
  void set_source(const scoped_refptr<EventTarget>& source) {
    source_ = source;
  }
  void set_ports(script::Sequence<scoped_refptr<MessagePort>> ports) {
    ports_ = ports;
  }

  static std::string GetResponseTypeAsString(const ResponseType response_type);
  static MessageEvent::ResponseType GetResponseType(base::StringPiece to_match);

  DEFINE_WRAPPABLE_TYPE(MessageEvent)

 private:
  ResponseType response_type_ = kText;
  scoped_refptr<net::IOBufferWithSize> data_io_buffer_;
  std::unique_ptr<script::StructuredClone> structured_clone_;
  std::string origin_;
  std::string last_event_id_;
  scoped_refptr<EventTarget> source_;
  script::Sequence<scoped_refptr<MessagePort>> ports_;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_MESSAGE_EVENT_H_
