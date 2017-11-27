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

#ifndef COBALT_NETWORK_COBALT_NET_LOG_H_
#define COBALT_NETWORK_COBALT_NET_LOG_H_

#include "base/atomic_sequence_num.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "net/base/net_log.h"

namespace cobalt {
namespace network {

class NetLogLogger;

// CobaltNetLog is an implementation of NetLog that dispatches network log
// messages to a list of observers.
//
// All methods are thread safe, with the exception that no NetLog or
// NetLog::ThreadSafeObserver functions may be called by an observer's
// OnAddEntry() method.  Doing so will result in a deadlock.
//
// CobaltNetLog is largely copied from ChromeNetLog. The differences are the
// removal of a different type of Observer, and the path and level are passed
// in as constructor parameters.
class CobaltNetLog : public net::NetLog {
 public:
  CobaltNetLog(const FilePath& log_path, net::NetLog::LogLevel level);
  virtual ~CobaltNetLog();

  // NetLog implementation:
  uint32 NextID() override;
  LogLevel GetLogLevel() const override;
  void AddThreadSafeObserver(ThreadSafeObserver* observer,
                             LogLevel log_level) override;
  void SetObserverLogLevel(ThreadSafeObserver* observer,
                           LogLevel log_level) override;
  void RemoveThreadSafeObserver(ThreadSafeObserver* observer) override;

 private:
  // NetLog implementation:
  void OnAddEntry(const net::NetLog::Entry& entry) override;

  // Called whenever an observer is added or removed, or has its log level
  // changed.  Must have acquired |lock_| prior to calling.
  void UpdateLogLevel();

  // |lock_| protects access to |observers_|.
  base::Lock lock_;

  // Used to get a unique source id.
  base::AtomicSequenceNumber source_id_;

  // The lowest allowed log level, regardless of any ChromeNetLogObservers.
  // Normally defaults to LOG_BASIC, but can be changed with command line flags.
  LogLevel base_log_level_;

  // The current log level.
  base::subtle::Atomic32 effective_log_level_;

  scoped_ptr<NetLogLogger> net_log_logger_;

  // |lock_| must be acquired whenever reading or writing to this.
  ObserverList<ThreadSafeObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(CobaltNetLog);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_COBALT_NET_LOG_H_
