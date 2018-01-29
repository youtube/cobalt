// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_NETWORK_NET_LOG_LOGGER_H_
#define COBALT_NETWORK_NET_LOG_LOGGER_H_

#include <string>

#include "base/platform_file.h"
#include "base/threading/thread.h"
#include "net/base/net_log.h"

class FilePath;

namespace cobalt {
namespace network {

// NetLogLogger watches the NetLog event stream, and sends all entries to
// VLOG(1) or a path specified on creation.  This is to debug errors that
// prevent getting to the about:net-internals page.
//
// When writing directly to a file rather than VLOG(1), the text file will
// contain a single JSON object, with an extra comma on the end and missing
// a terminal "]}".
//
// This is more or less copied from Chromium's NetLogLogger.
class NetLogLogger : public net::NetLog::ThreadSafeObserver {
 public:
  // If |log_path| is empty or file creation fails, writes to VLOG(1).
  // Otherwise, writes to |log_path|.  Uses one line per entry, for
  // easy parsing.
  explicit NetLogLogger(const FilePath& log_path);
  virtual ~NetLogLogger();

  // Starts observing specified NetLog.  Must not already be watching a NetLog.
  // Separate from constructor to enforce thread safety.
  void StartObserving(net::NetLog* net_log);

  // net::NetLog::ThreadSafeObserver implementation:
  void OnAddEntry(const net::NetLog::Entry& entry) override;

 private:
  void WriteToLog(const std::string& data);

  base::Thread log_writer_thread_;
  base::PlatformFile log_file_;

  DISALLOW_COPY_AND_ASSIGN(NetLogLogger);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_NET_LOG_LOGGER_H_
