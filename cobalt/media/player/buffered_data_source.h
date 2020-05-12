// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_PLAYER_BUFFERED_DATA_SOURCE_H_
#define COBALT_MEDIA_PLAYER_BUFFERED_DATA_SOURCE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/media/base/data_source.h"
#include "starboard/types.h"
#include "url/gurl.h"

namespace cobalt {
namespace media {

enum Preload {
  kPreloadNone,
  kPreloadMetaData,
  kPreloadAuto,
};

// TODO: Investigate if we still need BufferedDataSource.
class BufferedDataSource : public DataSource {
 public:
  typedef base::Callback<void(bool)> DownloadingStatusCB;

  virtual void SetDownloadingStatusCB(
      const DownloadingStatusCB& downloading_status_cb) {
  }
  virtual void SetPreload(Preload preload) {
  }
  virtual bool HasSingleOrigin() { return true; }
  virtual bool DidPassCORSAccessCheck() const { return true; }
  virtual void Abort() {}
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PLAYER_BUFFERED_DATA_SOURCE_H_
