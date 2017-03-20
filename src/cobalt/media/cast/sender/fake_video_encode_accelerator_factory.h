// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_CAST_SENDER_FAKE_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_
#define COBALT_MEDIA_CAST_SENDER_FAKE_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/single_thread_task_runner.h"
#include "media/cast/cast_config.h"
#include "media/video/fake_video_encode_accelerator.h"

namespace media {
namespace cast {

// Used by test code to create fake VideoEncodeAccelerators.  The test code
// controls when the response callback is invoked.
class FakeVideoEncodeAcceleratorFactory {
 public:
  explicit FakeVideoEncodeAcceleratorFactory(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~FakeVideoEncodeAcceleratorFactory();

  int vea_response_count() const { return vea_response_count_; }
  int shm_response_count() const { return shm_response_count_; }

  // These return the instance last responded.  It is up to the caller to
  // determine whether the pointer is still valid, since this factory does not
  // own these objects anymore.
  media::FakeVideoEncodeAccelerator* last_response_vea() const {
    return static_cast<media::FakeVideoEncodeAccelerator*>(last_response_vea_);
  }
  base::SharedMemory* last_response_shm() const { return last_response_shm_; }

  // Set whether the next created media::FakeVideoEncodeAccelerator will
  // initialize successfully.
  void SetInitializationWillSucceed(bool will_init_succeed);

  // Enable/disable auto-respond mode.  Default is disabled.
  void SetAutoRespond(bool auto_respond);

  // Creates a media::FakeVideoEncodeAccelerator.  If in auto-respond mode,
  // |callback| is run synchronously (i.e., before this method returns).
  void CreateVideoEncodeAccelerator(
      const ReceiveVideoEncodeAcceleratorCallback& callback);

  // Creates shared memory of the requested |size|.  If in auto-respond mode,
  // |callback| is run synchronously (i.e., before this method returns).
  void CreateSharedMemory(size_t size,
                          const ReceiveVideoEncodeMemoryCallback& callback);

  // Runs the |callback| provided to the last call to
  // CreateVideoEncodeAccelerator() with the new VideoEncodeAccelerator
  // instance.
  void RespondWithVideoEncodeAccelerator();

  // Runs the |callback| provided to the last call to
  // CreateSharedMemory() with the new base::SharedMemory instance.
  void RespondWithSharedMemory();

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool will_init_succeed_;
  bool auto_respond_;
  std::unique_ptr<media::VideoEncodeAccelerator> next_response_vea_;
  ReceiveVideoEncodeAcceleratorCallback vea_response_callback_;
  std::unique_ptr<base::SharedMemory> next_response_shm_;
  ReceiveVideoEncodeMemoryCallback shm_response_callback_;
  int vea_response_count_;
  int shm_response_count_;
  media::VideoEncodeAccelerator* last_response_vea_;
  base::SharedMemory* last_response_shm_;

  DISALLOW_COPY_AND_ASSIGN(FakeVideoEncodeAcceleratorFactory);
};

}  // namespace cast
}  // namespace media

#endif  // COBALT_MEDIA_CAST_SENDER_FAKE_VIDEO_ENCODE_ACCELERATOR_FACTORY_H_
