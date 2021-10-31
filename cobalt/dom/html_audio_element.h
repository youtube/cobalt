// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DOM_HTML_AUDIO_ELEMENT_H_
#define COBALT_DOM_HTML_AUDIO_ELEMENT_H_

#include "cobalt/dom/html_media_element.h"

namespace cobalt {
namespace dom {

// The HTMLAudioElement is used to play audios.
//   https://www.w3.org/TR/html50/embedded-content-0.html#the-audio-element
class HTMLAudioElement : public HTMLMediaElement {
 public:
  static const char kTagName[];

  explicit HTMLAudioElement(Document* document);

  // Custom, not in any spec
  //
  // From HTMLElement
  scoped_refptr<HTMLAudioElement> AsHTMLAudioElement() override { return this; }

  DEFINE_WRAPPABLE_TYPE(HTMLAudioElement);

 private:
  DISALLOW_COPY_AND_ASSIGN(HTMLAudioElement);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_AUDIO_ELEMENT_H_
