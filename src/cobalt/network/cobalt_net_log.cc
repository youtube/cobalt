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

#include "cobalt/network/cobalt_net_log.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "cobalt/network/net_log_logger.h"

namespace cobalt {
namespace network {

CobaltNetLog::CobaltNetLog(const FilePath& log_path,
                           net::NetLog::LogLevel level)
    : base_log_level_(level), effective_log_level_(level) {
  // Increment this once so the first event gets 1 rather than 0.
  source_id_.GetNext();
  net_log_logger_.reset(new NetLogLogger(log_path));
  net_log_logger_->StartObserving(this);
}

CobaltNetLog::~CobaltNetLog() {
  // Remove the observers we own before we're destroyed.
  if (net_log_logger_.get()) RemoveThreadSafeObserver(net_log_logger_.get());
}

void CobaltNetLog::OnAddEntry(const net::NetLog::Entry& entry) {
  base::AutoLock lock(lock_);

  // Notify all of the log observers.
  FOR_EACH_OBSERVER(ThreadSafeObserver, observers_, OnAddEntry(entry));
}

uint32 CobaltNetLog::NextID() {
  return static_cast<uint32>(source_id_.GetNext());
}

net::NetLog::LogLevel CobaltNetLog::GetLogLevel() const {
  base::subtle::Atomic32 log_level =
      base::subtle::NoBarrier_Load(&effective_log_level_);
  return static_cast<net::NetLog::LogLevel>(log_level);
}

void CobaltNetLog::AddThreadSafeObserver(
    net::NetLog::ThreadSafeObserver* observer, LogLevel log_level) {
  base::AutoLock lock(lock_);

  observers_.AddObserver(observer);
  OnAddObserver(observer, log_level);
  UpdateLogLevel();
}

void CobaltNetLog::SetObserverLogLevel(
    net::NetLog::ThreadSafeObserver* observer, LogLevel log_level) {
  base::AutoLock lock(lock_);

  DCHECK(observers_.HasObserver(observer));
  OnSetObserverLogLevel(observer, log_level);
  UpdateLogLevel();
}

void CobaltNetLog::RemoveThreadSafeObserver(
    net::NetLog::ThreadSafeObserver* observer) {
  base::AutoLock lock(lock_);

  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
  OnRemoveObserver(observer);
  UpdateLogLevel();
}

void CobaltNetLog::UpdateLogLevel() {
  lock_.AssertAcquired();

  // Look through all the observers and find the finest granularity
  // log level (higher values of the enum imply *lower* log levels).
  LogLevel new_effective_log_level = base_log_level_;
  ObserverListBase<ThreadSafeObserver>::Iterator it(observers_);
  ThreadSafeObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    new_effective_log_level =
        std::min(new_effective_log_level, observer->log_level());
  }
  base::subtle::NoBarrier_Store(&effective_log_level_, new_effective_log_level);
}

}  // namespace network
}  // namespace cobalt
