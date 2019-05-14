// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <net_connection.h>

#include "starboard/common/log.h"
#include "starboard/system.h"

SbSystemConnectionType SbSystemGetConnectionType() {
  SbSystemConnectionType type = kSbSystemConnectionTypeUnknown;

  connection_h conn;
  if (connection_create(&conn) == CONNECTION_ERROR_NONE) {
    connection_type_e state;
    connection_get_type(conn, &state);
    switch (state) {
      case CONNECTION_TYPE_DISCONNECTED:
        SB_DLOG(ERROR) << "No Network. We must do something.";
        break;
      case CONNECTION_TYPE_WIFI:      // fall through
      case CONNECTION_TYPE_CELLULAR:  // fall through
      case CONNECTION_TYPE_BT:
        SB_DLOG(INFO) << "Connection Type : Wireless " << state;
        type = kSbSystemConnectionTypeWireless;
        break;
      case CONNECTION_TYPE_ETHERNET:  // fall through
      case CONNECTION_TYPE_NET_PROXY:
        SB_DLOG(INFO) << "Connection Type : Wired " << state;
        type = kSbSystemConnectionTypeWired;
        break;
      default:
        SB_DLOG(ERROR) << "Unknown connection state : " << state;
        break;
    }
    connection_destroy(conn);
  }

  return type;
}
