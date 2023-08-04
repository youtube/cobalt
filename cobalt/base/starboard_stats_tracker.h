// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_STARBOARD_STATS_TRACKER_H_
#define COBALT_BASE_STARBOARD_STATS_TRACKER_H_

#include "cobalt/base/c_val.h"
#include "starboard/common/metrics/stats_tracker.h"

class StarboardStatsTracker : public starboard::StatsTracker {
 public:
  StarboardStatsTracker()
      : file_write_success_("Starboard.FileWrite.Success", 0,
                            "SbFileWrite() success count from cobalt."),
        file_write_fail_("Starboard.FileWrite.Fail", 0,
                         "SbFileWrite() fail count from cobalt."),
        file_write_bytes_written_("Starboard.FileWrite.BytesWritten", 0,
                                  "SbFileWrite() bytes written from cobalt."),
        storage_write_record_success_(
            "Starboard.StorageWriteRecord.Success", 0,
            "SbStorageWriteRecord() success count from cobalt."),
        storage_write_record_fail_(
            "Starboard.StorageWriteRecord.Fail", 0,
            "SbStorageWriteRecord() fail count from cobalt."),
        storage_write_record_bytes_written_(
            "Starboard.StorageWriteRecord.BytesWritten", 0,
            "SbStorageWriteRecord() bytes written from cobalt.") {}

  void FileWriteSuccess() override { ++file_write_success_; }

  void FileWriteFail() override { ++file_write_fail_; }

  void FileWriteBytesWritten(int bytes_written) override {
    file_write_bytes_written_ += bytes_written;
  }

  void StorageWriteRecordSuccess() override { ++storage_write_record_success_; }

  void StorageWriteRecordFail() override { ++storage_write_record_fail_; }

  void StorageWriteRecordBytesWritten(int bytes_written) override {
    storage_write_record_bytes_written_ += bytes_written;
  }

 private:
  base::CVal<int, base::CValPublic> file_write_success_;
  base::CVal<int, base::CValPublic> file_write_fail_;
  base::CVal<base::cval::SizeInBytes, base::CValPublic>
      file_write_bytes_written_;
  base::CVal<int, base::CValPublic> storage_write_record_success_;
  base::CVal<int, base::CValPublic> storage_write_record_fail_;
  base::CVal<base::cval::SizeInBytes, base::CValPublic>
      storage_write_record_bytes_written_;
};

#endif  // COBALT_BASE_STARBOARD_STATS_TRACKER_H_
