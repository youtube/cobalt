// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "media/base/filters.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace {

media::FFmpegURLProtocol* ToProtocol(void* data) {
  return reinterpret_cast<media::FFmpegURLProtocol*>(data);
}

// FFmpeg protocol interface.
int OpenContext(URLContext* h, const char* filename, int flags) {
  media::FFmpegURLProtocol* protocol;
  media::FFmpegGlue::GetInstance()->GetProtocol(filename, &protocol);
  if (!protocol)
    return AVERROR_IO;

  h->priv_data = protocol;
  h->flags = URL_RDONLY;
  h->is_streamed = protocol->IsStreaming();
  return 0;
}

int ReadContext(URLContext* h, unsigned char* buf, int size) {
  media::FFmpegURLProtocol* protocol = ToProtocol(h->priv_data);
  int result = protocol->Read(size, buf);
  if (result < 0)
    result = AVERROR_IO;
  return result;
}

#if LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(52, 68, 0)
int WriteContext(URLContext* h, const unsigned char* buf, int size) {
#else
int WriteContext(URLContext* h, unsigned char* buf, int size) {
#endif
  // We don't support writing.
  return AVERROR_IO;
}

int64 SeekContext(URLContext* h, int64 offset, int whence) {
  media::FFmpegURLProtocol* protocol = ToProtocol(h->priv_data);
  int64 new_offset = AVERROR_IO;
  switch (whence) {
    case SEEK_SET:
      if (protocol->SetPosition(offset))
        protocol->GetPosition(&new_offset);
      break;

    case SEEK_CUR:
      int64 pos;
      if (!protocol->GetPosition(&pos))
        break;
      if (protocol->SetPosition(pos + offset))
        protocol->GetPosition(&new_offset);
      break;

    case SEEK_END:
      int64 size;
      if (!protocol->GetSize(&size))
        break;
      if (protocol->SetPosition(size + offset))
        protocol->GetPosition(&new_offset);
      break;

    case AVSEEK_SIZE:
      protocol->GetSize(&new_offset);
      break;

    default:
      NOTREACHED();
  }
  if (new_offset < 0)
    new_offset = AVERROR_IO;
  return new_offset;
}

int CloseContext(URLContext* h) {
  h->priv_data = NULL;
  return 0;
}

int LockManagerOperation(void** lock, enum AVLockOp op) {
  switch (op) {
    case AV_LOCK_CREATE:
      *lock = new base::Lock();
      if (!*lock)
        return 1;
      return 0;

    case AV_LOCK_OBTAIN:
      static_cast<base::Lock*>(*lock)->Acquire();
      return 0;

    case AV_LOCK_RELEASE:
      static_cast<base::Lock*>(*lock)->Release();
      return 0;

    case AV_LOCK_DESTROY:
      delete static_cast<base::Lock*>(*lock);
      *lock = NULL;
      return 0;
  }
  return 1;
}

}  // namespace

//------------------------------------------------------------------------------

namespace media {

// Use the HTTP protocol to avoid any file path separator issues.
static const char kProtocol[] = "http";

// Fill out our FFmpeg protocol definition.
static URLProtocol kFFmpegURLProtocol = {
  kProtocol,
  &OpenContext,
  &ReadContext,
  &WriteContext,
  &SeekContext,
  &CloseContext,
};

FFmpegGlue::FFmpegGlue() {
  // Before doing anything disable logging as it interferes with layout tests.
  av_log_set_level(AV_LOG_QUIET);

  // Register our protocol glue code with FFmpeg.
  avcodec_init();
  av_register_protocol2(&kFFmpegURLProtocol, sizeof(kFFmpegURLProtocol));
  av_lockmgr_register(&LockManagerOperation);

  // Now register the rest of FFmpeg.
  av_register_all();
}

FFmpegGlue::~FFmpegGlue() {
  av_lockmgr_register(NULL);
}

// static
FFmpegGlue* FFmpegGlue::GetInstance() {
  return Singleton<FFmpegGlue>::get();
}

std::string FFmpegGlue::AddProtocol(FFmpegURLProtocol* protocol) {
  base::AutoLock auto_lock(lock_);
  std::string key = GetProtocolKey(protocol);
  if (protocols_.find(key) == protocols_.end()) {
    protocols_[key] = protocol;
  }
  return key;
}

void FFmpegGlue::RemoveProtocol(FFmpegURLProtocol* protocol) {
  base::AutoLock auto_lock(lock_);
  for (ProtocolMap::iterator cur, iter = protocols_.begin();
       iter != protocols_.end();) {
    cur = iter;
    iter++;

    if (cur->second == protocol)
      protocols_.erase(cur);
  }
}

void FFmpegGlue::GetProtocol(const std::string& key,
                             FFmpegURLProtocol** protocol) {
  base::AutoLock auto_lock(lock_);
  ProtocolMap::iterator iter = protocols_.find(key);
  if (iter == protocols_.end()) {
    *protocol = NULL;
    return;
  }
  *protocol = iter->second;
}

std::string FFmpegGlue::GetProtocolKey(FFmpegURLProtocol* protocol) {
  // Use the FFmpegURLProtocol's memory address to generate the unique string.
  // This also has the nice property that adding the same FFmpegURLProtocol
  // reference will not generate duplicate entries.
  return StringPrintf("%s://%p", kProtocol, static_cast<void*>(protocol));
}

}  // namespace media
