// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_MEDIA_QUERY_LIST_H_
#define COBALT_DOM_MEDIA_QUERY_LIST_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/cssom/media_list.h"
#include "cobalt/dom/screen.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The MediaQueryList interface represents a list of Media Queries.
//   https://www.w3.org/TR/cssom/#mediaQuerylist
class MediaQueryList : public script::Wrappable {
 public:
  MediaQueryList(const scoped_refptr<cssom::MediaList>& media_list,
                 const scoped_refptr<Screen>& screen);

  // Web API: MediaQueryList
  //
  std::string media() const;

  bool matches() const;

  DEFINE_WRAPPABLE_TYPE(MediaQueryList);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~MediaQueryList();

  scoped_refptr<cssom::MediaList> media_list_;
  scoped_refptr<Screen> screen_;

  DISALLOW_COPY_AND_ASSIGN(MediaQueryList);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_QUERY_LIST_H_
