/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BASE_TOKENS_H_
#define COBALT_BASE_TOKENS_H_

#include "base/memory/singleton.h"
#include "cobalt/base/token.h"

namespace base {

// clang-format off
#define TOKENS_FOR_EACH_WITH_NAME_ONLY(MacroOpWithNameOnly)   \
    MacroOpWithNameOnly(addsourcebuffer)                      \
    MacroOpWithNameOnly(abort)                                \
    MacroOpWithNameOnly(animationend)                         \
    MacroOpWithNameOnly(blur)                                 \
    MacroOpWithNameOnly(canplay)                              \
    MacroOpWithNameOnly(canplaythrough)                       \
    MacroOpWithNameOnly(durationchange)                       \
    MacroOpWithNameOnly(emptied)                              \
    MacroOpWithNameOnly(ended)                                \
    MacroOpWithNameOnly(error)                                \
    MacroOpWithNameOnly(focus)                                \
    MacroOpWithNameOnly(hashchange)                           \
    MacroOpWithNameOnly(keyadded)                             \
    MacroOpWithNameOnly(keydown)                              \
    MacroOpWithNameOnly(keyerror)                             \
    MacroOpWithNameOnly(keymessage)                           \
    MacroOpWithNameOnly(keypress)                             \
    MacroOpWithNameOnly(keyup)                                \
    MacroOpWithNameOnly(load)                                 \
    MacroOpWithNameOnly(loadeddata)                           \
    MacroOpWithNameOnly(loadedmetadata)                       \
    MacroOpWithNameOnly(loadend)                              \
    MacroOpWithNameOnly(loadstart)                            \
    MacroOpWithNameOnly(needkey)                              \
    MacroOpWithNameOnly(nomatch)                              \
    MacroOpWithNameOnly(pause)                                \
    MacroOpWithNameOnly(play)                                 \
    MacroOpWithNameOnly(playing)                              \
    MacroOpWithNameOnly(progress)                             \
    MacroOpWithNameOnly(ratechange)                           \
    MacroOpWithNameOnly(readystatechange)                     \
    MacroOpWithNameOnly(removesourcebuffer)                   \
    MacroOpWithNameOnly(result)                               \
    MacroOpWithNameOnly(securitypolicyviolation)              \
    MacroOpWithNameOnly(seeked)                               \
    MacroOpWithNameOnly(seeking)                              \
    MacroOpWithNameOnly(soundend)                             \
    MacroOpWithNameOnly(soundstart)                           \
    MacroOpWithNameOnly(sourceclose)                          \
    MacroOpWithNameOnly(sourceended)                          \
    MacroOpWithNameOnly(sourceopen)                           \
    MacroOpWithNameOnly(storage)                              \
    MacroOpWithNameOnly(stalled)                              \
    MacroOpWithNameOnly(suspend)                              \
    MacroOpWithNameOnly(timeout)                              \
    MacroOpWithNameOnly(timeupdate)                           \
    MacroOpWithNameOnly(transitionend)                        \
    MacroOpWithNameOnly(unload)                               \
    MacroOpWithNameOnly(volumechange)                         \
    MacroOpWithNameOnly(waiting)

#define TOKENS_FOR_EACH_WITH_NAME_AND_VALUE(MacroOpWithNameAndValue)    \
    MacroOpWithNameAndValue(active_pseudo_class_selector, "active")     \
    MacroOpWithNameAndValue(after_pseudo_element_selector, "after")     \
    MacroOpWithNameAndValue(before_pseudo_element_selector, "before")   \
    MacroOpWithNameAndValue(cdata_section_node_name, "#cdata_section")  \
    MacroOpWithNameAndValue(class_selector_prefix, ".")                 \
    MacroOpWithNameAndValue(comment_node_name, "#comment")              \
    MacroOpWithNameAndValue(document_name, "#document")                 \
    MacroOpWithNameAndValue(domcontentloaded, "DOMContentLoaded")       \
    MacroOpWithNameAndValue(empty_pseudo_class_selector, "empty")       \
    MacroOpWithNameAndValue(focus_pseudo_class_selector, "focus")       \
    MacroOpWithNameAndValue(hover_pseudo_class_selector, "hover")       \
    MacroOpWithNameAndValue(id_selector_prefix, "#")                    \
    MacroOpWithNameAndValue(not_pseudo_class_selector, "not")           \
    MacroOpWithNameAndValue(pseudo_class_selector_prefix, ":")          \
    MacroOpWithNameAndValue(pseudo_element_selector_prefix, "::")       \
    MacroOpWithNameAndValue(text_node_name, "#text")
// clang-format on

// Singleton containing commonly used Tokens.
// This prevents runtime Token allocations when dispatching events, and avoids
// typos.
class Tokens {
 public:
  static Tokens* GetInstance();

#define DEFINE_TOKEN_ACCESSOR_WITH_NAME_ONLY(name) \
  static base::Token name() { return GetInstance()->name##_; }
#define DEFINE_TOKEN_ACCESSOR_WITH_NAME_AND_VALUE(name, value) \
  static base::Token name() { return GetInstance()->name##_; }

  TOKENS_FOR_EACH_WITH_NAME_ONLY(DEFINE_TOKEN_ACCESSOR_WITH_NAME_ONLY)
  TOKENS_FOR_EACH_WITH_NAME_AND_VALUE(DEFINE_TOKEN_ACCESSOR_WITH_NAME_AND_VALUE)
#undef DEFINE_TOKEN_ACCESSOR_WITH_NAME_ONLY
#undef DEFINE_TOKEN_ACCESSOR_WITH_NAME_AND_VALUE

 private:
  // To make Singleton<Tokens, LeakySingletonTraits<Tokens> > work with
  // our private constructor.
  friend struct DefaultSingletonTraits<Tokens>;

#define INITIALIZE_TOKEN_WITH_NAME_ONLY(name) name##_(#name),
#define INITIALIZE_TOKEN_WITH_NAME_AND_VALUE(name, value) name##_(value),
  Tokens()
      : TOKENS_FOR_EACH_WITH_NAME_ONLY(INITIALIZE_TOKEN_WITH_NAME_ONLY)
            TOKENS_FOR_EACH_WITH_NAME_AND_VALUE(
                INITIALIZE_TOKEN_WITH_NAME_AND_VALUE) dummy_(0) {}
#undef INITIALIZE_TOKEN_WITH_NAME_ONLY
#undef INITIALIZE_TOKEN_WITH_NAME_AND_VALUE

#define DECLARE_TOKEN_WITH_NAME_ONLY(name) const base::Token name##_;
#define DECLARE_TOKEN_WITH_NAME_AND_VALUE(name, value) \
  const base::Token name##_;
  TOKENS_FOR_EACH_WITH_NAME_ONLY(DECLARE_TOKEN_WITH_NAME_ONLY)
  TOKENS_FOR_EACH_WITH_NAME_AND_VALUE(DECLARE_TOKEN_WITH_NAME_AND_VALUE)
#undef DECLARE_TOKEN_WITH_NAME_ONLY
#undef DECLARE_TOKEN_WITH_NAME_AND_VALUE

  // Dummy member variable to be included as the last item in the ctor's
  // initialization list.  As otherwise we will have one extra ',' at the end of
  // the initialization list which causes a compile error.
  char dummy_;

  DISALLOW_COPY_AND_ASSIGN(Tokens);
};

#undef EVENT_NAMES_FOR_EACH

}  // namespace base

#endif  // COBALT_BASE_TOKENS_H_
