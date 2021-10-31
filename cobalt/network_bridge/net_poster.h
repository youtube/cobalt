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

#ifndef COBALT_NETWORK_BRIDGE_NET_POSTER_H_
#define COBALT_NETWORK_BRIDGE_NET_POSTER_H_

#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace cobalt {
namespace network_bridge {

// Simple class to manage some fire-and-forget POST requests.
class NetPoster {
 public:
  NetPoster();
  virtual ~NetPoster();

  // POST the given data to the URL. No notification will be given if this
  // succeeds or fails.
  // content_type should reflect the mime type of data, e.g. "application/json".
  // data is the data to upload. May be empty.
  virtual void Send(const GURL& url, const std::string& content_type,
                    const std::string& data) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetPoster);
};

typedef base::Callback<void(const GURL& url, const std::string& content_type,
                            const std::string& data)> PostSender;

}  // namespace network_bridge
}  // namespace cobalt

#endif  // COBALT_NETWORK_BRIDGE_NET_POSTER_H_
