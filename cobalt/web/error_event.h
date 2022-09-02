// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_WEB_ERROR_EVENT_H_
#define COBALT_WEB_ERROR_EVENT_H_

#include <memory>
#include <string>

#include "cobalt/base/token.h"
#include "cobalt/base/tokens.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/web/error_event_init.h"
#include "cobalt/web/event.h"

namespace cobalt {
namespace web {

// Whenever an uncaught runtime script error occurs in one of the scripts
// associated with a Document, the user agent must report the error for the
// relevant script.
//   https://www.w3.org/TR/html50/webappapis.html#errorevent
class ErrorEvent : public Event {
 public:
  ErrorEvent() : Event(base::Tokens::error()) {}
  explicit ErrorEvent(const std::string& type) : Event(type) {}
  explicit ErrorEvent(const web::ErrorEventInit& init_dict)
      : Event(base::Tokens::error(), init_dict),
        message_(init_dict.message()),
        filename_(init_dict.filename()),
        lineno_(init_dict.lineno()),
        colno_(init_dict.colno()) {
    InitError(init_dict);
  }
  ErrorEvent(const std::string& type, const web::ErrorEventInit& init_dict)
      : Event(type, init_dict),
        message_(init_dict.message()),
        filename_(init_dict.filename()),
        lineno_(init_dict.lineno()),
        colno_(init_dict.colno()) {
    InitError(init_dict);
  }

  // Web API: ErrorEvent
  //
  const std::string& message() const { return message_; }
  const std::string& filename() const { return filename_; }
  uint32 lineno() const { return lineno_; }
  uint32 colno() const { return colno_; }

  const script::ValueHandleHolder* error() const {
    if (!error_) {
      return NULL;
    }
    return &(error_->referenced_value());
  }

  DEFINE_WRAPPABLE_TYPE(ErrorEvent);

 protected:
  ~ErrorEvent() override {}

 private:
  void InitError(const web::ErrorEventInit& init_dict) {
    const script::ValueHandleHolder* error = init_dict.error();
    if (error) {
      error_.reset(new script::ValueHandleHolder::Reference(this, *error));
    }
  }

  std::string message_;
  std::string filename_;
  uint32 lineno_ = 0;
  uint32 colno_ = 0;
  std::unique_ptr<script::ValueHandleHolder::Reference> error_;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_ERROR_EVENT_H_
