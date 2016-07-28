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

#include "cobalt/network/net_log_logger.h"

#include <stdio.h>

#include <limits>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/values.h"
#include "cobalt/network/net_log_constants.h"

namespace cobalt {
namespace network {
namespace {
base::PlatformFile OpenFile(const FilePath& log_path) {
  int creation_flags =
      base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  return base::CreatePlatformFile(log_path, creation_flags, NULL /* created */,
                                  NULL /* error */);
}

void AppendToFile(base::PlatformFile file, const std::string& data) {
  DCHECK_LE(data.size(), static_cast<size_t>(std::numeric_limits<int>::max()));
  int num_bytes_to_write = static_cast<int>(data.size());
  int bytes_written = base::WritePlatformFileAtCurrentPos(file, data.c_str(),
                                                          num_bytes_to_write);
  UNREFERENCED_PARAMETER(bytes_written);
  DLOG_IF(FATAL, bytes_written != num_bytes_to_write)
      << "Did not write all bytes to file.";
}

void CloseFile(base::PlatformFile file) {
  bool success = base::ClosePlatformFile(file);
  UNREFERENCED_PARAMETER(success);
  DLOG_IF(ERROR, !success) << "Failed to close file.";
}
}  // namespace

NetLogLogger::NetLogLogger(const FilePath& log_path)
    : log_writer_thread_("NetLogLogger IO"),
      log_file_(base::kInvalidPlatformFileValue) {
  if (!log_path.empty()) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    log_file_ = OpenFile(log_path);
    if (log_file_ == base::kInvalidPlatformFileValue) {
      LOG(ERROR) << "Could not open file " << log_path.value()
                 << " for net logging";
      return;
    }

    const int kDefaultStackSize = 0;
    log_writer_thread_.StartWithOptions(
        base::Thread::Options(MessageLoop::TYPE_IO, kDefaultStackSize));

    // Write constants to the output file.  This allows loading files that have
    // different source and event types, as they may be added and removed
    // between Chrome versions.
    scoped_ptr<Value> value(NetLogConstants::GetConstants());
    std::string json;
    base::JSONWriter::Write(value.get(), &json);
    WriteToLog("{\"constants\": ");
    WriteToLog(json);
    WriteToLog(",\n");
    WriteToLog("\"events\": [\n");
  }
}

NetLogLogger::~NetLogLogger() {
  if (log_file_ != base::kInvalidPlatformFileValue) {
    log_writer_thread_.message_loop()->PostTask(
        FROM_HERE, base::Bind(&CloseFile, log_file_));
  }
  log_writer_thread_.Stop();
}

void NetLogLogger::StartObserving(net::NetLog* net_log) {
  net_log->AddThreadSafeObserver(this, net::NetLog::LOG_ALL_BUT_BYTES);
}

void NetLogLogger::OnAddEntry(const net::NetLog::Entry& entry) {
  scoped_ptr<Value> value(entry.ToValue());
  // Don't pretty print, so each JSON value occupies a single line, with no
  // breaks (Line breaks in any text field will be escaped).  Using strings
  // instead of integer identifiers allows logs from older versions to be
  // loaded, though a little extra parsing has to be done when loading a log.
  std::string json;
  base::JSONWriter::Write(value.get(), &json);
  if (log_file_ == base::kInvalidPlatformFileValue) {
    VLOG(1) << json;
  } else {
    WriteToLog(json);
    WriteToLog(",\n");
  }
}

void NetLogLogger::WriteToLog(const std::string& data) {
  DCHECK_NE(log_file_, base::kInvalidPlatformFileValue);
  DCHECK(log_writer_thread_.IsRunning());
  log_writer_thread_.message_loop()->PostTask(
      FROM_HERE, base::Bind(&AppendToFile, log_file_, data));
}

}  // namespace network
}  // namespace cobalt
