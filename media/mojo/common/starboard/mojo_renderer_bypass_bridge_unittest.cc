// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/mojo/common/starboard/mojo_renderer_bypass_bridge.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
#include "media/base/demuxer_stream.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

using ::testing::_;
using ::testing::NiceMock;

class MojoRendererBypassBridgeTest : public testing::Test {
 protected:
  MojoRendererBypassBridgeTest()
      : bridge_(base::MakeRefCounted<MojoRendererBypassBridge>(
            task_environment_.GetMainThreadTaskRunner(),
            time_update_cb_.Get(),
            statistics_update_cb_.Get())) {}

  base::test::TaskEnvironment task_environment_;
  base::MockRepeatingCallback<
      void(base::TimeDelta, base::TimeDelta, base::TimeTicks)>
      time_update_cb_;
  base::MockRepeatingCallback<void(const PipelineStatistics&)>
      statistics_update_cb_;
  scoped_refptr<MojoRendererBypassBridge> bridge_;
};

TEST_F(MojoRendererBypassBridgeTest, RegistryRegisterUnregister) {
  uint32_t id = 42;
  BypassBridgeRegistry::Register(id, bridge_);
  EXPECT_EQ(BypassBridgeRegistry::Get(id), bridge_);
  BypassBridgeRegistry::Unregister(id);
  EXPECT_EQ(BypassBridgeRegistry::Get(id), nullptr);
}

TEST_F(MojoRendererBypassBridgeTest, ReadNormal) {
  NiceMock<MockDemuxerStream> audio_stream(DemuxerStream::AUDIO);
  bridge_->SetStreams(&audio_stream, nullptr);

  DemuxerStream::ReadCB read_cb;
  EXPECT_CALL(audio_stream, OnRead(_))
      .WillOnce(
          [&read_cb](DemuxerStream::ReadCB& cb) { read_cb = std::move(cb); });

  base::MockCallback<DemuxerStream::ReadCB> client_read_cb;
  bridge_->Read(DemuxerStream::AUDIO, 1, client_read_cb.Get());

  ASSERT_TRUE(read_cb);

  // Expect client callback to be called.
  EXPECT_CALL(client_read_cb, Run(DemuxerStream::kOk, _));
  std::move(read_cb).Run(DemuxerStream::kOk, {});

  task_environment_.RunUntilIdle();
}

TEST_F(MojoRendererBypassBridgeTest, CallbackDestroyedWithoutRunning) {
  NiceMock<MockDemuxerStream> audio_stream(DemuxerStream::AUDIO);
  bridge_->SetStreams(&audio_stream, nullptr);

  DemuxerStream::ReadCB read_cb;
  EXPECT_CALL(audio_stream, OnRead(_))
      .WillOnce(
          [&read_cb](DemuxerStream::ReadCB& cb) { read_cb = std::move(cb); });

  base::MockCallback<DemuxerStream::ReadCB> client_read_cb;
  bridge_->Read(DemuxerStream::AUDIO, 1, client_read_cb.Get());

  ASSERT_TRUE(read_cb);

  // Destroy the callback without running it.
  read_cb.Reset();

  // Invalidate should not hang because ScopedClosureRunner should have
  // decremented the counter when read_cb was destroyed.
  bridge_->Invalidate();
}

TEST_F(MojoRendererBypassBridgeTest, InvalidateWaitsForRead) {
  NiceMock<MockDemuxerStream> audio_stream(DemuxerStream::AUDIO);
  bridge_->SetStreams(&audio_stream, nullptr);

  DemuxerStream::ReadCB read_cb;
  EXPECT_CALL(audio_stream, OnRead(_))
      .WillOnce(
          [&read_cb](DemuxerStream::ReadCB& cb) { read_cb = std::move(cb); });

  base::MockCallback<DemuxerStream::ReadCB> client_read_cb;
  bridge_->Read(DemuxerStream::AUDIO, 1, client_read_cb.Get());

  ASSERT_TRUE(read_cb);

  base::WaitableEvent invalidate_started(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent invalidate_finished(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<MojoRendererBypassBridge> bridge,
             base::WaitableEvent* started, base::WaitableEvent* finished) {
            base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync;
            started->Signal();
            bridge->Invalidate();
            finished->Signal();
          },
          bridge_, &invalidate_started, &invalidate_finished));

  invalidate_started.Wait();
  EXPECT_FALSE(invalidate_finished.IsSignaled());

  EXPECT_CALL(client_read_cb, Run(DemuxerStream::kOk, _));
  std::move(read_cb).Run(DemuxerStream::kOk, {});
  invalidate_finished.Wait();
}

TEST_F(MojoRendererBypassBridgeTest, GetMimeType) {
  NiceMock<MockDemuxerStream> audio_stream(DemuxerStream::AUDIO);
  NiceMock<MockDemuxerStream> video_stream(DemuxerStream::VIDEO);
  audio_stream.set_mime_type("audio/mp4");
  video_stream.set_mime_type("video/mp4");

  bridge_->SetStreams(&audio_stream, &video_stream);

  EXPECT_EQ(bridge_->GetMimeType(DemuxerStream::AUDIO), "audio/mp4");
  EXPECT_EQ(bridge_->GetMimeType(DemuxerStream::VIDEO), "video/mp4");

  bridge_->Invalidate();
  EXPECT_EQ(bridge_->GetMimeType(DemuxerStream::AUDIO), "");
  EXPECT_EQ(bridge_->GetMimeType(DemuxerStream::VIDEO), "");
}

TEST_F(MojoRendererBypassBridgeTest, EnableBitstreamConverter) {
  NiceMock<MockDemuxerStream> audio_stream(DemuxerStream::AUDIO);
  NiceMock<MockDemuxerStream> video_stream(DemuxerStream::VIDEO);

  bridge_->SetStreams(&audio_stream, &video_stream);

  EXPECT_CALL(audio_stream, EnableBitstreamConverter()).Times(1);
  EXPECT_CALL(video_stream, EnableBitstreamConverter()).Times(1);

  bridge_->EnableBitstreamConverter(DemuxerStream::AUDIO);
  bridge_->EnableBitstreamConverter(DemuxerStream::VIDEO);

  bridge_->Invalidate();
  // Should be a no-op when inactive.
  bridge_->EnableBitstreamConverter(DemuxerStream::AUDIO);
}

TEST_F(MojoRendererBypassBridgeTest, UnknownStreamType) {
  NiceMock<MockDemuxerStream> audio_stream(DemuxerStream::AUDIO);
  NiceMock<MockDemuxerStream> video_stream(DemuxerStream::VIDEO);
  audio_stream.set_mime_type("audio/mp4");
  video_stream.set_mime_type("video/mp4");

  bridge_->SetStreams(&audio_stream, &video_stream);

  EXPECT_CALL(audio_stream, EnableBitstreamConverter()).Times(0);
  EXPECT_CALL(video_stream, EnableBitstreamConverter()).Times(0);

  EXPECT_EQ(bridge_->GetMimeType(DemuxerStream::UNKNOWN), "");
  bridge_->EnableBitstreamConverter(DemuxerStream::UNKNOWN);

  bridge_->Invalidate();
}

}  // namespace
}  // namespace media
