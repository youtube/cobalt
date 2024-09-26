// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_android.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/ui_base_jni_headers/Clipboard_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"

// TODO:(andrewhayden) Support additional formats in Android: URI, HTML,
// HTML+text now that Android's clipboard system supports them, then nuke the
// legacy implementation note below.

// Legacy implementation note:
// The Android clipboard, and hence `ClipboardAndroid` as well, used to only
// support text. Since then, bitmap support has been added, but not support
// for any other formats.
//
// Therefore, Clipboard data is stored:
// - on the Android system, for text and bitmaps.
// - in a process-wide static variable protected by a lock, for other formats.
//
// These "other formats" only work within the same process, and can't be copied
// between Android applications.

using base::android::AttachCurrentThread;
using base::android::ClearException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace ui {

namespace {

constexpr char kPngExtension[] = ".png";

using ReadPngCallback = ClipboardAndroid::ReadPngCallback;

// Fetching image data from Java as PNG bytes.
std::vector<uint8_t> GetPngData(
    const base::android::ScopedJavaGlobalRef<jobject>& clipboard_manager) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> jimage_data =
      Java_Clipboard_getPng(env, clipboard_manager);
  if (jimage_data.is_null()) {
    return std::vector<uint8_t>();
  }
  DCHECK(jimage_data.obj());

  std::vector<uint8_t> png_data;
  JavaByteArrayToByteVector(env, jimage_data, &png_data);
  return png_data;
}

// Add a format:jstr pair to map, if jstr is null or is empty, then remove that
// entry.
void JNI_Clipboard_AddMapEntry(JNIEnv* env,
                               std::map<ClipboardFormatType, std::string>* map,
                               const ClipboardFormatType& format,
                               const ScopedJavaLocalRef<jstring>& jstr) {
  if (jstr.is_null()) {
    map->erase(format);
    return;
  }

  std::string str = ConvertJavaStringToUTF8(env, jstr.obj());
  if (!str.empty()) {
    (*map)[format] = str;
  } else {
    map->erase(format);
  }
}

class ClipboardMap {
 public:
  ClipboardMap();
  void SetModifiedCallback(ClipboardAndroid::ModifiedCallback cb);
  void SetJavaSideNativePtr(Clipboard* clipboard);
  std::string Get(const ClipboardFormatType& format);
  void GetPng(ReadPngCallback callback);
  void DidGetPng(ReadPngCallback callback, std::vector<uint8_t> result);
  const ClipboardSequenceNumberToken& GetSequenceNumber() const;
  base::Time GetLastModifiedTime() const;
  void ClearLastModifiedTime();
  bool HasFormat(const ClipboardFormatType& format);
  std::vector<ClipboardFormatType> GetFormats();
  void OnPrimaryClipboardChanged();
  void OnPrimaryClipTimestampInvalidated(int64_t timestamp_ms);
  void Set(const ClipboardFormatType& format, const std::string& data);
  void CommitToAndroidClipboard();
  void Clear();

  // Unlike the functions above, does not call |modified_cb_|.
  void SetLastModifiedTimeWithoutRunningCallback(base::Time time);

 private:
  enum class MapState {
    kOutOfDate,
    kUpToDate,
    kPreparingCommit,
  };

  // Updates |last_modified_time_| to |time| and writes it to |local_state_|.
  void UpdateLastModifiedTime(base::Time time);

  // Updates 'map_' and 'map_state_' if necessary by fetching data from Java.
  // Start from Android S, accessing the system clipboard will cause a
  // pop up notification showing that the app copied content from clipboard. To
  // avoid that, This function should only be called when we really need to read
  // the data.
  void UpdateFromAndroidClipboard();

  std::map<ClipboardFormatType, std::string> map_ GUARDED_BY(lock_);
  MapState map_state_;

  // This lock is for read/write |map_|.
  base::Lock lock_;

  ClipboardSequenceNumberToken sequence_number_;
  base::Time last_modified_time_;

  ClipboardAndroid::ModifiedCallback modified_cb_;

  // Java class and methods for the Android ClipboardManager.
  ScopedJavaGlobalRef<jobject> clipboard_manager_;
};
base::LazyInstance<ClipboardMap>::Leaky g_map = LAZY_INSTANCE_INITIALIZER;

ClipboardMap::ClipboardMap() : map_state_(MapState::kOutOfDate) {
  clipboard_manager_.Reset(Java_Clipboard_getInstance(AttachCurrentThread()));
  DCHECK(clipboard_manager_.obj());
}

void ClipboardMap::SetModifiedCallback(ClipboardAndroid::ModifiedCallback cb) {
  modified_cb_ = std::move(cb);
}

void ClipboardMap::SetJavaSideNativePtr(Clipboard* clipboard) {
  JNIEnv* env = AttachCurrentThread();
  Java_Clipboard_setNativePtr(env, clipboard_manager_,
                              reinterpret_cast<intptr_t>(clipboard));
}

std::string ClipboardMap::Get(const ClipboardFormatType& format) {
  base::AutoLock lock(lock_);
  UpdateFromAndroidClipboard();
  auto it = map_.find(format);
  return it == map_.end() ? std::string() : it->second;
}

void ClipboardMap::GetPng(ReadPngCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&GetPngData, clipboard_manager_),
      base::BindOnce(&ClipboardMap::DidGetPng, base::Unretained(this),
                     std::move(callback)));
}

void ClipboardMap::DidGetPng(ReadPngCallback callback,
                             std::vector<uint8_t> result) {
  // GetPngData attempts to read from the Java Clipboard, which sometimes is
  // not available (ex. the app is not in focus, such as in unit tests).
  if (!result.empty()) {
    std::move(callback).Run(std::move(result));
    return;
  }

  // Since the The Java Clipboard did not provide a valid bitmap, attempt to
  // read from our in-memory clipboard map if the map is up-to-date.
  if (map_state_ != MapState::kUpToDate) {
    std::move(callback).Run(std::vector<uint8_t>());
    return;
  }
  std::string png_str = g_map.Get().Get(ClipboardFormatType::PngType());
  std::vector<uint8_t> png_data{png_str.begin(), png_str.end()};
  std::move(callback).Run(png_data);
}

const ClipboardSequenceNumberToken& ClipboardMap::GetSequenceNumber() const {
  return sequence_number_;
}

base::Time ClipboardMap::GetLastModifiedTime() const {
  return last_modified_time_;
}

void ClipboardMap::ClearLastModifiedTime() {
  UpdateLastModifiedTime(base::Time());
}

bool ClipboardMap::HasFormat(const ClipboardFormatType& format) {
  base::AutoLock lock(lock_);
  if (map_state_ == MapState::kUpToDate) {
    // If the 'map_' is up to date, we can just check with it.
    // Images can be read if either bitmap or PNG types are available.
    if (format == ClipboardFormatType::PngType() ||
        format == ClipboardFormatType::BitmapType()) {
      return base::Contains(map_, ClipboardFormatType::PngType()) ||
             base::Contains(map_, ClipboardFormatType::BitmapType());
    }
    return base::Contains(map_, format);
  }

  // If the 'map_' is not up to date, we need to check with the system if the
  // formats are supported by Android clipboard.
  // We will not update the map from here since update the map will update both
  // formats and data, but we only need format info here. Also, reading data
  // from the system clipboard will cause clipboard access notification popping
  // up.
  JNIEnv* env = AttachCurrentThread();
  // TODO(crbug.com/1194601): Create a single method for the follow JNI calls.
  if (format == ClipboardFormatType::PlainTextType()) {
    return Java_Clipboard_hasCoercedText(env, clipboard_manager_);
  } else if (format == ClipboardFormatType::HtmlType()) {
    return Java_Clipboard_hasHTMLOrStyledText(env, clipboard_manager_);
  } else if (format == ClipboardFormatType::UrlType()) {
    return Java_Clipboard_hasUrl(env, clipboard_manager_);
  } else if (format == ClipboardFormatType::PngType() ||
             format == ClipboardFormatType::BitmapType()) {
    return Java_Clipboard_hasImage(env, clipboard_manager_);
  }

  // Android unsupported format types, check local only.
  return base::Contains(map_, format);
}

std::vector<ClipboardFormatType> ClipboardMap::GetFormats() {
  base::AutoLock lock(lock_);
  std::vector<ClipboardFormatType> formats;
  formats.reserve(map_.size());

  // Check with Android for Android clipboard supported formats.
  if (map_state_ != MapState::kUpToDate) {
    JNIEnv* env = AttachCurrentThread();
    if (Java_Clipboard_hasCoercedText(env, clipboard_manager_)) {
      formats.push_back(ClipboardFormatType::PlainTextType());
    }
    if (Java_Clipboard_hasHTMLOrStyledText(env, clipboard_manager_)) {
      formats.push_back(ClipboardFormatType::HtmlType());
    }
    if (Java_Clipboard_hasUrl(env, clipboard_manager_)) {
      formats.push_back(ClipboardFormatType::UrlType());
    }
    if (Java_Clipboard_hasImage(env, clipboard_manager_)) {
      formats.push_back(ClipboardFormatType::BitmapType());
      formats.push_back(ClipboardFormatType::PngType());
    }
  }

  // Check local cache, since the formats not supported by Android clipboard are
  // not synced on any other layer.
  for (const auto& it : map_) {
    if (map_state_ != MapState::kUpToDate &&
        (it.first == ClipboardFormatType::PlainTextType() ||
         it.first == ClipboardFormatType::HtmlType() ||
         it.first == ClipboardFormatType::UrlType() ||
         it.first == ClipboardFormatType::BitmapType() ||
         it.first == ClipboardFormatType::PngType())) {
      continue;
    }
    formats.push_back(it.first);
  }

  return formats;
}

void ClipboardMap::OnPrimaryClipboardChanged() {
  sequence_number_ = ClipboardSequenceNumberToken();
  UpdateLastModifiedTime(base::Time::Now());
  map_state_ = MapState::kOutOfDate;
}

void ClipboardMap::OnPrimaryClipTimestampInvalidated(int64_t timestamp_ms) {
  base::Time timestamp = base::Time::FromJavaTime(timestamp_ms);
  if (GetLastModifiedTime() < timestamp) {
    sequence_number_ = ClipboardSequenceNumberToken();
    UpdateLastModifiedTime(timestamp);
    map_state_ = MapState::kOutOfDate;
  }
}

void ClipboardMap::Set(const ClipboardFormatType& format,
                       const std::string& data) {
  base::AutoLock lock(lock_);
  map_[format] = data;
  map_state_ = MapState::kPreparingCommit;
}

void ClipboardMap::CommitToAndroidClipboard() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  if (base::Contains(map_, ClipboardFormatType::HtmlType())) {
    // Android's API for storing HTML content on the clipboard requires a plain-
    // text representation to be available as well.
    if (!base::Contains(map_, ClipboardFormatType::PlainTextType()))
      return;

    ScopedJavaLocalRef<jstring> html =
        ConvertUTF8ToJavaString(env, map_[ClipboardFormatType::HtmlType()]);
    ScopedJavaLocalRef<jstring> text = ConvertUTF8ToJavaString(
        env, map_[ClipboardFormatType::PlainTextType()]);

    DCHECK(html.obj() && text.obj());
    Java_Clipboard_setHTMLText(env, clipboard_manager_, html, text);
  } else if (base::Contains(map_, ClipboardFormatType::PlainTextType())) {
    ScopedJavaLocalRef<jstring> str = ConvertUTF8ToJavaString(
        env, map_[ClipboardFormatType::PlainTextType()]);
    DCHECK(str.obj());
    Java_Clipboard_setText(env, clipboard_manager_, str);
  } else if (base::Contains(map_, ClipboardFormatType::PngType())) {
    // Committing the PNG data to the Android clipboard will create an image
    // with a corresponding URI. Once this has been created, update the local
    // clipboard with this URI.
    ScopedJavaLocalRef<jbyteArray> image_data =
        ToJavaByteArray(env, map_[ClipboardFormatType::PngType()]);
    ScopedJavaLocalRef<jstring> image_extension =
        ConvertUTF8ToJavaString(env, kPngExtension);
    DCHECK(image_data.obj());
    // TODO(crbug.com/1223215) In unit tests, `jimageuri` is empty.
    Java_Clipboard_setImage(env, clipboard_manager_, image_data,
                            image_extension);
    ScopedJavaLocalRef<jstring> jimageuri =
        Java_Clipboard_getImageUriString(env, clipboard_manager_);
    JNI_Clipboard_AddMapEntry(env, &map_, ClipboardFormatType::BitmapType(),
                              jimageuri);
  } else {
    Java_Clipboard_clear(env, clipboard_manager_);
    NOTIMPLEMENTED();
  }
  map_state_ = MapState::kUpToDate;
  sequence_number_ = ClipboardSequenceNumberToken();
  UpdateLastModifiedTime(base::Time::Now());
}

void ClipboardMap::Clear() {
  JNIEnv* env = AttachCurrentThread();
  base::AutoLock lock(lock_);
  map_.clear();
  Java_Clipboard_clear(env, clipboard_manager_);
  map_state_ = MapState::kUpToDate;
  sequence_number_ = ClipboardSequenceNumberToken();
  UpdateLastModifiedTime(base::Time::Now());
}

void ClipboardMap::SetLastModifiedTimeWithoutRunningCallback(base::Time time) {
  last_modified_time_ = time;
}

void ClipboardMap::UpdateLastModifiedTime(base::Time time) {
  last_modified_time_ = time;
  // |modified_cb_| may be null in tests.
  if (modified_cb_)
    modified_cb_.Run(time);
}

void ClipboardMap::UpdateFromAndroidClipboard() {
  DCHECK_NE(MapState::kPreparingCommit, map_state_);
  if (map_state_ == MapState::kUpToDate)
    return;

  // Fetch the current Android clipboard state.
  lock_.AssertAcquired();
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jstring> jtext =
      Java_Clipboard_getCoercedText(env, clipboard_manager_);
  ScopedJavaLocalRef<jstring> jhtml =
      Java_Clipboard_getHTMLText(env, clipboard_manager_);
  ScopedJavaLocalRef<jstring> jurl =
      Java_Clipboard_getUrl(env, clipboard_manager_);
  ScopedJavaLocalRef<jstring> jimageuri =
      Java_Clipboard_getImageUriString(env, clipboard_manager_);

  JNI_Clipboard_AddMapEntry(env, &map_, ClipboardFormatType::PlainTextType(),
                            jtext);
  JNI_Clipboard_AddMapEntry(env, &map_, ClipboardFormatType::HtmlType(), jhtml);
  JNI_Clipboard_AddMapEntry(env, &map_, ClipboardFormatType::UrlType(), jurl);
  JNI_Clipboard_AddMapEntry(env, &map_, ClipboardFormatType::BitmapType(),
                            jimageuri);

  map_state_ = MapState::kUpToDate;
}

}  // namespace

// Clipboard factory method.
// static
Clipboard* Clipboard::Create() {
  return new ClipboardAndroid;
}

// Static method for testing.
void JNI_Clipboard_CleanupForTesting(JNIEnv* env) {
  Clipboard::DestroyClipboardForCurrentThread();
}

// ClipboardAndroid implementation.

void ClipboardAndroid::OnPrimaryClipChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  g_map.Get().OnPrimaryClipboardChanged();
}

void ClipboardAndroid::OnPrimaryClipTimestampInvalidated(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const jlong j_timestamp_ms) {
  g_map.Get().OnPrimaryClipTimestampInvalidated(j_timestamp_ms);
}

int64_t ClipboardAndroid::GetLastModifiedTimeToJavaTime(JNIEnv* env) {
  return GetLastModifiedTime().ToJavaTime();
}

void ClipboardAndroid::SetModifiedCallback(ModifiedCallback cb) {
  g_map.Get().SetModifiedCallback(std::move(cb));
}

void ClipboardAndroid::SetLastModifiedTimeWithoutRunningCallback(
    base::Time time) {
  g_map.Get().SetLastModifiedTimeWithoutRunningCallback(time);
}

ClipboardAndroid::ClipboardAndroid() {
  DCHECK(CalledOnValidThread());
  g_map.Get().SetJavaSideNativePtr(this);
}

ClipboardAndroid::~ClipboardAndroid() {
  DCHECK(CalledOnValidThread());
}

void ClipboardAndroid::OnPreShutdown() {}

// DataTransferEndpoint is not used on this platform.
DataTransferEndpoint* ClipboardAndroid::GetSource(
    ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  return nullptr;
}

const ClipboardSequenceNumberToken& ClipboardAndroid::GetSequenceNumber(
    ClipboardBuffer /* buffer */) const {
  DCHECK(CalledOnValidThread());
  return g_map.Get().GetSequenceNumber();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
bool ClipboardAndroid::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  return g_map.Get().HasFormat(format);
}

void ClipboardAndroid::Clear(ClipboardBuffer buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  g_map.Get().Clear();
}

std::vector<std::u16string> ClipboardAndroid::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  // would be nice to ask the ClipboardMap to enumerate the types it supports,
  // rather than hardcode the list here.
  if (IsFormatAvailable(ClipboardFormatType::PlainTextType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeText));
  if (IsFormatAvailable(ClipboardFormatType::HtmlType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeHTML));
  if (IsFormatAvailable(ClipboardFormatType::SvgType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeSvg));
  // We can read images from either the Android clipboard or the local map.
  if (IsFormatAvailable(ClipboardFormatType::BitmapType(), buffer, data_dst) ||
      IsFormatAvailable(ClipboardFormatType::PngType(), buffer, data_dst)) {
    types.push_back(base::UTF8ToUTF16(kMimeTypeImageURI));
    types.push_back(base::UTF8ToUTF16(kMimeTypePNG));
  }

  // these formats aren't supported by the ClipboardMap currently, but might
  // be one day?
  if (IsFormatAvailable(ClipboardFormatType::RtfType(), buffer, data_dst))
    types.push_back(base::UTF8ToUTF16(kMimeTypeRTF));
  return types;
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  DCHECK(types);

  types->clear();
  *types = GetStandardFormats(buffer, data_dst);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadText(ClipboardBuffer buffer,
                                const DataTransferEndpoint* data_dst,
                                std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  std::string utf8;
  ReadAsciiText(buffer, data_dst, &utf8);
  *result = base::UTF8ToUTF16(utf8);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadAsciiText(ClipboardBuffer buffer,
                                     const DataTransferEndpoint* data_dst,
                                     std::string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kText);
  *result = g_map.Get().Get(ClipboardFormatType::PlainTextType());
}

// |src_url| isn't really used. It is only implemented in Windows.
// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadHTML(ClipboardBuffer buffer,
                                const DataTransferEndpoint* data_dst,
                                std::u16string* markup,
                                std::string* src_url,
                                uint32_t* fragment_start,
                                uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kHtml);
  if (src_url)
    src_url->clear();

  std::string input = g_map.Get().Get(ClipboardFormatType::HtmlType());
  *markup = base::UTF8ToUTF16(input);

  *fragment_start = 0;
  *fragment_end = static_cast<uint32_t>(markup->length());
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadSvg(ClipboardBuffer buffer,
                               const DataTransferEndpoint* data_dst,
                               std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  std::string utf8 = g_map.Get().Get(ClipboardFormatType::SvgType());
  *result = base::UTF8ToUTF16(utf8);
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadRTF(ClipboardBuffer buffer,
                               const DataTransferEndpoint* data_dst,
                               std::string* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadPng(ClipboardBuffer buffer,
                               const DataTransferEndpoint* data_dst,
                               ReadPngCallback callback) const {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  RecordRead(ClipboardFormatMetric::kPng);
  g_map.Get().GetPng(std::move(callback));
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadCustomData(ClipboardBuffer buffer,
                                      const std::u16string& type,
                                      const DataTransferEndpoint* data_dst,
                                      std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadFilenames(ClipboardBuffer buffer,
                                     const DataTransferEndpoint* data_dst,
                                     std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());
  NOTIMPLEMENTED();
}

// 'data_dst' and 'title' are not used. It's only passed to be consistent with
// other platforms.
void ClipboardAndroid::ReadBookmark(const DataTransferEndpoint* data_dst,
                                    std::u16string* title,
                                    std::string* url) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kBookmark);
  *url = g_map.Get().Get(ClipboardFormatType::UrlType());
}

// |data_dst| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::ReadData(const ClipboardFormatType& format,
                                const DataTransferEndpoint* data_dst,
                                std::string* result) const {
  DCHECK(CalledOnValidThread());
  RecordRead(ClipboardFormatMetric::kData);
  *result = g_map.Get().Get(format);
}

base::Time ClipboardAndroid::GetLastModifiedTime() const {
  DCHECK(CalledOnValidThread());
  return g_map.Get().GetLastModifiedTime();
}

void ClipboardAndroid::ClearLastModifiedTime() {
  DCHECK(CalledOnValidThread());
  g_map.Get().ClearLastModifiedTime();
}

// Main entry point used to write several values in the clipboard.
// |data_src| is not used. It's only passed to be consistent with other
// platforms.
void ClipboardAndroid::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(buffer, ClipboardBuffer::kCopyPaste);
  g_map.Get().Clear();

  DispatchPlatformRepresentations(std::move(platform_representations));
  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);

  g_map.Get().CommitToAndroidClipboard();
}

void ClipboardAndroid::WriteText(const char* text_data, size_t text_len) {
  g_map.Get().Set(ClipboardFormatType::PlainTextType(),
                  std::string(text_data, text_len));
}

void ClipboardAndroid::WriteHTML(const char* markup_data,
                                 size_t markup_len,
                                 const char* url_data,
                                 size_t url_len) {
  g_map.Get().Set(ClipboardFormatType::HtmlType(),
                  std::string(markup_data, markup_len));
}

void ClipboardAndroid::WriteUnsanitizedHTML(const char* markup_data,
                                            size_t markup_len,
                                            const char* url_data,
                                            size_t url_len) {
  WriteHTML(markup_data, markup_len, url_data, url_len);
}

void ClipboardAndroid::WriteSvg(const char* markup_data, size_t markup_len) {
  g_map.Get().Set(ClipboardFormatType::SvgType(),
                  std::string(markup_data, markup_len));
}

void ClipboardAndroid::WriteRTF(const char* rtf_data, size_t data_len) {
  NOTIMPLEMENTED();
}

void ClipboardAndroid::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  NOTIMPLEMENTED();
}

// According to other platforms implementations, this really writes the
// URL spec.
void ClipboardAndroid::WriteBookmark(const char* title_data,
                                     size_t title_len,
                                     const char* url_data,
                                     size_t url_len) {
  g_map.Get().Set(ClipboardFormatType::UrlType(),
                  std::string(url_data, url_len));
}

// Write an extra flavor that signifies WebKit was the last to modify the
// pasteboard. This flavor has no data.
void ClipboardAndroid::WriteWebSmartPaste() {
  g_map.Get().Set(ClipboardFormatType::WebKitSmartPasteType(), std::string());
}

// Encoding SkBitmap to PNG data. Then, |g_map| can commit the PNG data to
// Android system clipboard without encode/decode.
void ClipboardAndroid::WriteBitmap(const SkBitmap& sk_bitmap) {
  scoped_refptr<base::RefCountedMemory> image_memory =
      gfx::Image::CreateFrom1xBitmap(sk_bitmap).As1xPNGBytes();
  std::string packed(image_memory->front_as<char>(), image_memory->size());

  g_map.Get().Set(ClipboardFormatType::PngType(), packed);
}

void ClipboardAndroid::WriteData(const ClipboardFormatType& format,
                                 const char* data_data,
                                 size_t data_len) {
  g_map.Get().Set(format, std::string(data_data, data_len));
}

}  // namespace ui
