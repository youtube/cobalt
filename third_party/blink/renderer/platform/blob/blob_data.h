/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_DATA_H_

// This file is required via serialized_blob.typemap and, transitively,
// encoded_form_data.typemap. To avoid build circularity issues, it should not
// transitively include anything that is generated from a mojom_blink target.
//
// This requires some gymnastics below, to explicitly forward-declare the
// required types without reference to the generator output headers.

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/blob/data_element.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace network {
namespace mojom {
namespace blink {
class DataPipeGetter;
}  // namespace blink
}  // namespace mojom
}  // namespace network

namespace blink {
namespace mojom {
namespace blink {
class Blob;

class BlobReaderClient;

class BlobRegistry;

class DataElement;
using DataElementPtr = mojo::StructPtr<DataElement>;
}
}  // namespace mojom

class BlobBytesProvider;
class BlobDataHandle;

class PLATFORM_EXPORT RawData : public ThreadSafeRefCounted<RawData> {
 public:
  static scoped_refptr<RawData> Create() {
    return base::AdoptRef(new RawData());
  }

  const char* data() const { return data_.data(); }
  size_t length() const { return data_.size(); }
  Vector<char>* MutableData() { return &data_; }

 private:
  RawData();

  Vector<char> data_;
};

class PLATFORM_EXPORT BlobData {
  USING_FAST_MALLOC(BlobData);

 public:
  static constexpr int64_t kToEndOfFile = -1;
  enum class FileCompositionStatus {
    kSingleUnknownSizeFile,
    kNoUnknownSizeFiles
  };

  explicit BlobData(
      FileCompositionStatus = FileCompositionStatus::kNoUnknownSizeFiles);
  BlobData(const BlobData&) = delete;
  BlobData& operator=(const BlobData&) = delete;
  ~BlobData();

  // Calling append* on objects returned by createFor___WithUnknownSize will
  // check-fail. The caller can only have an unknown-length file if it is the
  // only item in the blob.
  static std::unique_ptr<BlobData> CreateForFileWithUnknownSize(
      const String& path);
  static std::unique_ptr<BlobData> CreateForFileWithUnknownSize(
      const String& path,
      const absl::optional<base::Time>& expected_modification_time);

  const String& ContentType() const { return content_type_; }
  void SetContentType(const String&);

  const Vector<mojom::blink::DataElementPtr>& ElementsForTesting() const {
    return elements_;
  }
  Vector<mojom::blink::DataElementPtr> ReleaseElements();

  void AppendBytes(const void*, size_t length);
  void AppendData(scoped_refptr<RawData>);
  void AppendFile(const String& path,
                  int64_t offset,
                  int64_t length,
                  const absl::optional<base::Time>& expected_modification_time);

  // The given blob must not be a file with unknown size. Please use the
  // File::appendTo instead.
  void AppendBlob(scoped_refptr<BlobDataHandle>,
                  int64_t offset,
                  int64_t length);
  void AppendText(const String&, bool normalize_line_endings_to_native);

  // The value of the size property for a Blob who has this data.
  // BlobDataItem::toEndOfFile if the Blob has a file whose size was not yet
  // determined.
  uint64_t length() const;

  bool IsSingleUnknownSizeFile() const {
    return file_composition_ == FileCompositionStatus::kSingleUnknownSizeFile;
  }

 private:
  void AppendDataInternal(base::span<const char> data,
                          scoped_refptr<RawData> = nullptr);

  String content_type_;
  FileCompositionStatus file_composition_;

  Vector<mojom::blink::DataElementPtr> elements_;
  size_t current_memory_population_ = 0;

  // These two members are used to combine multiple consecutive 'bytes' elements
  // (as created by `AppendBytes`, `AppendData` or `AppendText`) into a single
  // element. When one has a value the other also has a value. Before using
  // `elements_` to actually create a blob, `last_bytes_provider_` should be
  // bound to `last_bytes_provider_receiver_`.
  std::unique_ptr<BlobBytesProvider> last_bytes_provider_;
  mojo::PendingReceiver<mojom::blink::BytesProvider>
      last_bytes_provider_receiver_;
};

class PLATFORM_EXPORT BlobDataHandle
    : public ThreadSafeRefCounted<BlobDataHandle> {
  USING_FAST_MALLOC(BlobDataHandle);

 public:
  // For empty blob construction.
  static scoped_refptr<BlobDataHandle> Create() {
    return base::AdoptRef(new BlobDataHandle());
  }

  // For initial creation.
  static scoped_refptr<BlobDataHandle> Create(std::unique_ptr<BlobData> data,
                                              uint64_t size) {
    return base::AdoptRef(new BlobDataHandle(std::move(data), size));
  }

  // For deserialization of script values and ipc messages.
  static scoped_refptr<BlobDataHandle> Create(const String& uuid,
                                              const String& type,
                                              uint64_t size) {
    return base::AdoptRef(new BlobDataHandle(uuid, type, size));
  }

  static scoped_refptr<BlobDataHandle> Create(
      const String& uuid,
      const String& type,
      uint64_t size,
      mojo::PendingRemote<mojom::blink::Blob>);

  String Uuid() const { return uuid_; }
  String GetType() const { return type_; }
  uint64_t size() const { return size_; }

  bool IsSingleUnknownSizeFile() const { return is_single_unknown_size_file_; }

  ~BlobDataHandle();

  mojo::PendingRemote<mojom::blink::Blob> CloneBlobRemote();
  void CloneBlobRemote(mojo::PendingReceiver<mojom::blink::Blob>);
  mojo::PendingRemote<network::mojom::blink::DataPipeGetter> AsDataPipeGetter();

  void ReadAll(mojo::ScopedDataPipeProducerHandle,
               mojo::PendingRemote<mojom::blink::BlobReaderClient>);
  void ReadRange(uint64_t offset,
                 uint64_t length,
                 mojo::ScopedDataPipeProducerHandle,
                 mojo::PendingRemote<mojom::blink::BlobReaderClient>);

  // This does synchronous IPC, and possibly synchronous file operations. Think
  // twice before calling this function.
  bool CaptureSnapshot(uint64_t* snapshot_size,
                       absl::optional<base::Time>* snapshot_modification_time);

  void SetBlobRemoteForTesting(mojo::PendingRemote<mojom::blink::Blob> remote) {
    base::AutoLock locker(blob_remote_lock_);
    blob_remote_ = std::move(remote);
  }

  static mojom::blink::BlobRegistry* GetBlobRegistry();
  static void SetBlobRegistryForTesting(mojom::blink::BlobRegistry*);

 private:
  BlobDataHandle();
  BlobDataHandle(std::unique_ptr<BlobData>, uint64_t size);
  BlobDataHandle(const String& uuid, const String& type, uint64_t size);
  BlobDataHandle(const String& uuid,
                 const String& type,
                 uint64_t size,
                 mojo::PendingRemote<mojom::blink::Blob>);

  const String uuid_;
  const String type_;
  const uint64_t size_;
  const bool is_single_unknown_size_file_;
  // This class is supposed to be thread safe. So to be able to use the mojo
  // Blob interface from multiple threads store a PendingRemote combined with
  // a lock, and make sure any access to the mojo interface is done protected
  // by the lock.
  mojo::PendingRemote<mojom::blink::Blob> blob_remote_
      GUARDED_BY(blob_remote_lock_);
  base::Lock blob_remote_lock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BLOB_BLOB_DATA_H_
