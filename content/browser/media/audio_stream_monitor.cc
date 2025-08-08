// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_monitor.h"

#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/invalidate_type.h"

namespace content {

namespace {

AudioStreamMonitor* GetMonitorForRenderFrame(int render_process_id,
                                             int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  WebContentsImpl* const web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(
          RenderFrameHost::FromID(render_process_id, render_frame_id)));
  return web_contents ? web_contents->audio_stream_monitor() : nullptr;
}

}  // namespace

AudioStreamMonitor::AudibleClientRegistration::AudibleClientRegistration(
    GlobalRenderFrameHostId host_id,
    AudioStreamMonitor* audio_stream_monitor)
    : host_id_(host_id), audio_stream_monitor_(audio_stream_monitor) {
  audio_stream_monitor_->AddAudibleClient(host_id_);
}

AudioStreamMonitor::AudibleClientRegistration::~AudibleClientRegistration() {
  audio_stream_monitor_->RemoveAudibleClient(host_id_);
}

bool AudioStreamMonitor::StreamID::operator<(const StreamID& other) const {
  return std::tie(render_process_id, render_frame_id, stream_id) <
         std::tie(other.render_process_id, other.render_frame_id,
                  other.stream_id);
}

bool AudioStreamMonitor::StreamID::operator==(const StreamID& other) const {
  return std::tie(render_process_id, render_frame_id, stream_id) ==
         std::tie(other.render_process_id, other.render_frame_id,
                  other.stream_id);
}

AudioStreamMonitor::AudioStreamMonitor(WebContents* contents)
    : WebContentsObserver(contents),
      web_contents_(contents),
      clock_(base::DefaultTickClock::GetInstance()) {
  DCHECK(web_contents_);
}

AudioStreamMonitor::~AudioStreamMonitor() {
  DCHECK(audible_clients_.empty());
}

bool AudioStreamMonitor::WasRecentlyAudible() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return indicator_is_on_;
}

bool AudioStreamMonitor::IsCurrentlyAudible() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return is_audible_;
}

void AudioStreamMonitor::RenderProcessGone(int render_process_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Note: It's possible for the RenderProcessHost and WebContents (and thus
  // this class) to survive the death of the render process and subsequently be
  // reused. During this period GetMonitorForRenderFrame() will be unable to
  // lookup the WebContents using the now-dead |render_frame_id|. We must thus
  // have this secondary mechanism for clearing stale streams.
  // Streams must be removed locally before calling UpdateStreams() in order to
  // avoid removing streams from the process twice, since RenderProcessHost
  // removes the streams on its own when the renderer process is gone.
  base::EraseIf(streams_,
                [render_process_id](const std::pair<StreamID, bool>& entry) {
                  return entry.first.render_process_id == render_process_id;
                });
  UpdateStreams();
}

std::unique_ptr<AudioStreamMonitor::AudibleClientRegistration>
AudioStreamMonitor::RegisterAudibleClient(GlobalRenderFrameHostId host_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_unique<AudibleClientRegistration>(host_id, this);
}

void AudioStreamMonitor::AddAudibleClient(GlobalRenderFrameHostId host_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  audible_clients_[host_id]++;
  UpdateStreams();
}

void AudioStreamMonitor::RemoveAudibleClient(GlobalRenderFrameHostId host_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!audible_clients_.empty());

  auto it = audible_clients_.find(host_id);
  CHECK(it != audible_clients_.end());
  CHECK_GT(it->second, 0u);
  it->second--;

  UpdateStreams();

  if (it->second == 0) {
    audible_clients_.erase(it);
  }
}

// static
void AudioStreamMonitor::StartMonitoringStream(int render_process_id,
                                               int render_frame_id,
                                               int stream_id) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const StreamID& sid) {
            if (AudioStreamMonitor* monitor = GetMonitorForRenderFrame(
                    sid.render_process_id, sid.render_frame_id)) {
              monitor->StartMonitoringStreamOnUIThread(sid);
            }
          },
          StreamID{render_process_id, render_frame_id, stream_id}));
}

// static
void AudioStreamMonitor::StopMonitoringStream(int render_process_id,
                                              int render_frame_id,
                                              int stream_id) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const StreamID& sid) {
            if (AudioStreamMonitor* monitor = GetMonitorForRenderFrame(
                    sid.render_process_id, sid.render_frame_id)) {
              monitor->StopMonitoringStreamOnUIThread(sid);
            }
          },
          StreamID{render_process_id, render_frame_id, stream_id}));
}

// static
void AudioStreamMonitor::UpdateStreamAudibleState(int render_process_id,
                                                  int render_frame_id,
                                                  int stream_id,
                                                  bool is_audible) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const StreamID& sid, bool is_audible) {
            if (AudioStreamMonitor* monitor = GetMonitorForRenderFrame(
                    sid.render_process_id, sid.render_frame_id)) {
              monitor->UpdateStreamAudibleStateOnUIThread(sid, is_audible);
            }
          },
          StreamID{render_process_id, render_frame_id, stream_id}, is_audible));
}

void AudioStreamMonitor::StartMonitoringStreamOnUIThread(const StreamID& sid) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(streams_.find(sid) == streams_.end());
  streams_[sid] = false;
}

void AudioStreamMonitor::StopMonitoringStreamOnUIThread(const StreamID& sid) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = streams_.find(sid);
  if (it == streams_.end())
    return;

  // First set the state of stream to silent in order to correctly update the
  // frame state.
  streams_[sid] = false;
  UpdateStreams();
  streams_.erase(it);
}

void AudioStreamMonitor::UpdateStreamAudibleStateOnUIThread(const StreamID& sid,
                                                            bool is_audible) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = streams_.find(sid);
  if (it == streams_.end())
    return;

  it->second = is_audible;
  UpdateStreams();
}

void AudioStreamMonitor::UpdateStreams() {
  bool was_audible = is_audible_;
  is_audible_ = false;

  // Determine whether a RenderFrameHost is audible based on
  // stream and non-stream client states.
  base::flat_map<GlobalRenderFrameHostId, bool> audible_frames;
  audible_frames.reserve(streams_.size() + audible_clients_.size());

  for (auto& kv : streams_) {
    const bool is_stream_audible = kv.second;
    is_audible_ |= is_stream_audible;
    GlobalRenderFrameHostId host_id(kv.first.render_process_id,
                                    kv.first.render_frame_id);
    audible_frames[host_id] |= is_stream_audible;
  }

  for (auto& kv : audible_clients_) {
    const bool is_client_audible = kv.second > 0;
    is_audible_ |= is_client_audible;
    audible_frames[kv.first] |= is_client_audible;
  }

  if (was_audible && !is_audible_) {
    last_became_silent_time_ = clock_->NowTicks();
  }

  // Update RenderFrameHostImpl audible state if state has changed.
  for (const auto& kv : audible_frames) {
    auto* render_frame_host_impl =
        static_cast<RenderFrameHostImpl*>(RenderFrameHost::FromID(kv.first));

    // RenderFrameHostImpl may be null in some tests.
    if (!render_frame_host_impl) {
      continue;
    }

    bool is_frame_audible = kv.second;
    if (is_frame_audible != render_frame_host_impl->is_audible()) {
      render_frame_host_impl->OnAudibleStateChanged(is_frame_audible);
    }
  }

  if (is_audible_ != was_audible) {
    MaybeToggle();
    web_contents_->OnAudioStateChanged();
  }
}

void AudioStreamMonitor::MaybeToggle() {
  const base::TimeTicks off_time =
      last_became_silent_time_ + base::Milliseconds(kHoldOnMilliseconds);
  const base::TimeTicks now = clock_->NowTicks();
  const bool should_stop_timer = is_audible_ || now >= off_time;
  const bool should_indicator_be_on = is_audible_ || !should_stop_timer;

  if (should_indicator_be_on != indicator_is_on_) {
    indicator_is_on_ = should_indicator_be_on;
    web_contents_->NotifyNavigationStateChanged(INVALIDATE_TYPE_AUDIO);
  }

  if (should_stop_timer) {
    off_timer_.Stop();
  } else if (!off_timer_.IsRunning()) {
    off_timer_.Start(FROM_HERE, off_time - now,
                     base::BindOnce(&AudioStreamMonitor::MaybeToggle,
                                    base::Unretained(this)));
  }
}

void AudioStreamMonitor::RenderFrameDeleted(
    RenderFrameHost* render_frame_host) {
  int render_process_id = render_frame_host->GetProcess()->GetID();
  int render_frame_id = render_frame_host->GetRoutingID();

  // It is possible for a frame to be deleted before notifications about its
  // streams are received. Explicitly clear these streams.
  base::EraseIf(streams_, [render_process_id, render_frame_id](
                              const std::pair<StreamID, bool>& entry) {
    return entry.first.render_process_id == render_process_id &&
           entry.first.render_frame_id == render_frame_id;
  });
  UpdateStreams();
}

}  // namespace content
