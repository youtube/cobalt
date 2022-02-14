// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/updater/unzipper.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "starboard/event.h"
#include "starboard/memory.h"
#include "starboard/system.h"
#include "starboard/time.h"
#include "third_party/lz4_lib/lz4frame.h"
#include "third_party/zlib/google/zip.h"
#include "third_party/zlib/google/zip_reader.h"

namespace cobalt {
namespace updater {

namespace {

constexpr size_t kChunkSize = 8 * 1024;
constexpr LZ4F_preferences_t kPrefs = {
    {LZ4F_max256KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame,
     0 /* unknown content size */, 0 /* no dictID */, LZ4F_noBlockChecksum},
    0,
    /* compression level; 0 == default */ 0,
    /* autoflush */ 0,
    /* favor decompression speed */ {0, 0, 0},
    /* reserved, must be set to 0 */};

class LZ4WriterDelegate : public zip::WriterDelegate {
 public:
  explicit LZ4WriterDelegate(const base::FilePath& output_file_path)
      : output_file_path_(output_file_path) {}

  bool PrepareOutput() override {
    if (!base::CreateDirectory(output_file_path_.DirName())) {
      return false;
    }

    output_file_path_ = output_file_path_.ReplaceExtension("lz4");

    LOG(INFO) << "LZ4WriterDelegate::PrepareOutput: output_file_path_="
              << output_file_path_.value();
    file_.Initialize(output_file_path_,
                     base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    if (!file_.IsValid()) {
      LOG(ERROR) << "LZ4WriterDelegate::PrepareOutput: invalid file"
                 << output_file_path_.value();
      return false;
    }

    LZ4F_errorCode_t res = LZ4F_createCompressionContext(&ctx_, LZ4F_VERSION);
    if (LZ4F_isError(res)) {
      LOG(ERROR) << "LZ4WriterDelegate::PrepareOutput: lz4 error code= " << res;
      return false;
    }

    out_capacity_ = LZ4F_compressBound(kChunkSize, &kPrefs);
    LOG(INFO) << "out_capacity=" << out_capacity_;
    out_buffer_.resize(out_capacity_);
    if (!WriteHeader()) {
      LOG(ERROR) << "LZ4WriterDelegate::PrepareOutput: Failed to write header";
      return false;
    }
    return true;
  }

  bool WriteBytes(const char* data, int num_bytes) override {
    LOG(INFO) << "LZ4WriterDelegate::WriteBytes: num_bytes=" << num_bytes
              << " out_capacity=" << out_capacity_;
    size_t compressed_size = LZ4F_compressUpdate(
        ctx_, out_buffer_.data(), out_buffer_.size(), data, num_bytes, nullptr);
    if (LZ4F_isError(compressed_size)) {
      LOG(ERROR) << "LZ4WriterDelegate::WriteBytes: Compression failed: error "
                 << (unsigned)compressed_size;
      return false;
    }

    LOG(INFO) << "LZ4WriterDelegate::WriteBytes: Writing "
              << (unsigned)compressed_size << " bytes";
    if (compressed_size !=
        file_.WriteAtCurrentPos(out_buffer_.data(), compressed_size)) {
      LOG(ERROR) << "LZ4WriterDelegate::WriteBytes: failed to write "
                 << compressed_size << " bytes";
      return false;
    }
    return true;
  }

  void SetTimeModified(const base::Time& time) override {
    size_t compressed_size =
        LZ4F_compressEnd(ctx_, out_buffer_.data(), out_buffer_.size(), nullptr);
    if (LZ4F_isError(compressed_size)) {
      LOG(ERROR) << "LZ4WriterDelegate::SetTimeModified: Failed to end "
                    "compression: error "
                 << (unsigned)compressed_size;
      return;
    }

    LOG(INFO) << "Writing " << (unsigned)compressed_size << " bytes";

    if (compressed_size !=
        file_.WriteAtCurrentPos(out_buffer_.data(), compressed_size)) {
      LOG(ERROR) << "LZ4WriterDelegate::SetTimeModified: failed to write "
                 << compressed_size << " bytes";
      return;
    }

    success_ = true;
  }

  bool IsSuccessful() override { return success_; }

  ~LZ4WriterDelegate() {
    if (ctx_) {
      LZ4F_freeCompressionContext(ctx_);
    }
    file_.Close();
  }

 private:
  bool WriteHeader() {
    size_t header_size = LZ4F_compressBegin(ctx_, out_buffer_.data(),
                                            out_buffer_.size(), &kPrefs);
    if (LZ4F_isError(header_size)) {
      LOG(ERROR) << "Failed to start compression: error "
                 << (unsigned)header_size;
      return false;
    }
    LOG(INFO) << "Header size " << (unsigned)header_size;
    return header_size ==
           file_.WriteAtCurrentPos(out_buffer_.data(), header_size);
  }

  base::FilePath output_file_path_;
  base::File file_;
  LZ4F_compressionContext_t ctx_ = nullptr;
  std::vector<char> out_buffer_;
  size_t out_capacity_ = 64 * 1024;
  bool success_ = false;
};

class UnzipperImpl : public update_client::Unzipper {
 public:
  explicit UnzipperImpl(bool updater_compression)
      : compress_update_(updater_compression) {}

  void Unzip(const base::FilePath& zip_path, const base::FilePath& output_path,
             UnzipCompleteCallback callback) override {
    if (compress_update_) {
      UnzipAndLZ4Compress(zip_path, output_path, std::move(callback));
    } else {
      UnzipToPath(zip_path, output_path, std::move(callback));
    }
  }

  void UnzipAndLZ4Compress(const base::FilePath& zip_path,
                           const base::FilePath& output_path,
                           UnzipCompleteCallback callback) {
    SbTimeMonotonic time_before_unzip = SbTimeGetMonotonicNow();

    // Filter to unzip just libcobalt.so
    auto filter = base::BindRepeating([](const base::FilePath& path) {
      return (path.BaseName().value() == "libcobalt.so");
    });
    auto directory_creator = base::BindRepeating(
        [](const base::FilePath& extract_dir,
           const base::FilePath& entry_path) {
          return base::CreateDirectory(extract_dir.Append(entry_path));
        },
        output_path);
    auto writer_factory = base::BindRepeating(
        [](const base::FilePath& extract_dir, const base::FilePath& entry_path)
            -> std::unique_ptr<zip::WriterDelegate> {
          return std::make_unique<LZ4WriterDelegate>(
              extract_dir.Append(entry_path));
        },
        output_path);

    if (!zip::UnzipWithFilterAndWriters(zip_path, writer_factory,
                                        directory_creator, filter, true)) {
      LOG(ERROR) << "Failed to unzip libcobalt.so";
      std::move(callback).Run(false);
      return;
    } else {
      LOG(INFO) << "Successfully unzipped and lz4 compressed libcobalt.so";
    }

    // Filter to unzip the rest of the CRX package.
    filter = base::BindRepeating([](const base::FilePath& path) {
      if (path.BaseName().value() == "libcobalt.so") {
        return false;
      }
      return true;
    });

    if (!zip::UnzipWithFilterCallback(zip_path, output_path, filter, true)) {
      LOG(ERROR) << "Failed to unzip the CRX package";
      std::move(callback).Run(false);
      return;
    } else {
      LOG(INFO) << "Successfully unzipped the CRX package";
    }

    std::move(callback).Run(true);

    SbTimeMonotonic time_unzip_took =
        SbTimeGetMonotonicNow() - time_before_unzip;
    LOG(INFO) << "Unzip file path = " << zip_path;
    LOG(INFO) << "output_path = " << output_path;
    LOG(INFO) << "Unzip took " << time_unzip_took / kSbTimeMillisecond
              << " milliseconds.";
  }

  void UnzipToPath(const base::FilePath& zip_path,
                   const base::FilePath& output_path,
                   UnzipCompleteCallback callback) {
    SbTimeMonotonic time_before_unzip = SbTimeGetMonotonicNow();
    std::move(callback).Run(zip::Unzip(zip_path, output_path));
    SbTimeMonotonic time_unzip_took =
        SbTimeGetMonotonicNow() - time_before_unzip;
    LOG(INFO) << "Unzip file path = " << zip_path;
    LOG(INFO) << "output_path = " << output_path;
    LOG(INFO) << "Unzip took " << time_unzip_took / kSbTimeMillisecond
              << " milliseconds.";
  }

 private:
  bool compress_update_ = false;
};

}  // namespace

UnzipperFactory::UnzipperFactory(bool compress_update)
    : compress_update_(compress_update) {}

std::unique_ptr<update_client::Unzipper> UnzipperFactory::Create() const {
  return std::make_unique<UnzipperImpl>(compress_update_);
}

UnzipperFactory::~UnzipperFactory() = default;

}  // namespace updater
}  // namespace cobalt
