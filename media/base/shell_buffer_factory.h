/*
 * Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef MEDIA_BASE_SHELL_BUFFER_FACTORY_H_
#define MEDIA_BASE_SHELL_BUFFER_FACTORY_H_

#include <list>
#include <map>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/decoder_buffer.h"

namespace media {

// Singleton instance class for the management and recycling of decoder buffers.
// To minimize heap churn each buffer is reused and the object tries to re-use
// buffers before creating new ones. Since video decoder buffers are so much
// larger on average than audio ones we manage audio and video buffers with
// separate functions.

// These constants are used by the demuxer to determine if an AU should even
// be downloaded
static const int ShellVideoDecoderBufferMaxSize = 256 * 1024;
static const int ShellAudioDecoderBufferMaxSize = 16 * 1024;

class MEDIA_EXPORT ShellBufferFactory {
 public:
  static void Initialize();
  static scoped_refptr<DecoderBuffer> GetVideoDecoderBuffer();
  static void ReturnVideoDecoderBuffer(scoped_refptr<DecoderBuffer>
      video_buffer);
  static scoped_refptr<DecoderBuffer> GetAudioDecoderBuffer();
  static void ReturnAudioDecoderBuffer(scoped_refptr<DecoderBuffer>
      audio_buffer);
  static void Terminate();

 private:
  ShellBufferFactory();
  ~ShellBufferFactory();

  static ShellBufferFactory* instance_;
  uint32 buffer_handle_counter_;
  base::Lock lock_;
  typedef std::list<scoped_refptr<DecoderBuffer> > DecoderBufferList;
  DecoderBufferList free_audio_buffers_list_;
  DecoderBufferList free_video_buffers_list_;
  typedef std::map<uint32, scoped_refptr<DecoderBuffer> > DecoderBufferMap;
  DecoderBufferMap allocated_audio_buffers_map_;
  DecoderBufferMap allocated_video_buffers_map_;
};

}  // namespace media

#endif  // MEDIA_BASE_SHELL_BUFFER_FACTORY_H_
