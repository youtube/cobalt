// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_DRM_SYSTEM_CLIENT_H_
#define COBALT_MEDIA_BASE_DRM_SYSTEM_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace cobalt {
namespace media {

// An interface to be implemented by clients of |DrmSystem|.
//
// Calls to |DrmSystemClient| are always performed from the same thread
// where |DrmSystem| was instantiated.
class DrmSystemClient {
 public:
  // When called as a result of |DrmSystem::GenerateSessionUpdateRequest|, this
  // method denotes a successful request generation.
  //
  // This method may be called multiple times after a single call to
  // |GenerateSessionUpdateRequest|, for example when the underlying DRM system
  // needs to update a license. In this case |ticket| will be
  // |kSbDrmTicketInvalid|.
  virtual void OnSessionUpdateRequestGenerated(int ticket,
                                               const std::string& session_id,
                                               scoped_array<uint8> message,
                                               int message_size) = 0;
  // Called as a result of |GenerateSessionUpdateRequest| and denotes a failure
  // during request generation.
  //
  // Unlike its successful counterpart, never called spontaneously.
  virtual void OnSessionUpdateRequestDidNotGenerate(int ticket) = 0;

  // Called as a result of |UpdateSession| and denotes a successful session
  // update.
  virtual void OnSessionUpdated(int ticket) = 0;
  // Called as a result of |UpdateSession| and denotes a failure during session
  // update.
  virtual void OnSessionDidNotUpdate(int ticket) = 0;

 protected:
  virtual ~DrmSystemClient() {}
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_DRM_SYSTEM_CLIENT_H_
