// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_vod_rendition.h"

#include "base/task/bind_post_task.h"
#include "media/filters/hls_manifest_demuxer_engine.h"

namespace media {

// Chosen mostly arbitrarily.
constexpr size_t kChunkSize = 1024 * 32;

// A nice round number, chosen to make sure that we get a good average network
// speed calculation.
constexpr size_t kMovingAverageSampleSize = 128;

HlsVodRendition::SegmentInfo::SegmentInfo() {}
HlsVodRendition::SegmentInfo::SegmentInfo(const HlsVodRendition::SegmentInfo&) =
    default;
HlsVodRendition::SegmentInfo::~SegmentInfo() {}

HlsVodRendition::PendingSegment::PendingSegment(HlsDataSourceStream&& stream,
                                                size_t index)
    : stream(std::move(stream)), index(index) {}

HlsVodRendition::~HlsVodRendition() {
  engine_host_->RemoveRole(role_);
}

HlsVodRendition::HlsVodRendition(ManifestDemuxerEngineHost* engine_host,
                                 HlsRenditionHost* rendition_host,
                                 std::string role,
                                 scoped_refptr<hls::MediaPlaylist> playlist,
                                 base::TimeDelta duration)
    : engine_host_(engine_host),
      rendition_host_(rendition_host),
      role_(role),
      segment_duration_upper_limit_(playlist->GetTargetDuration()),
      duration_(duration),
      fetch_time_(MovingAverage(kMovingAverageSampleSize)) {
  base::TimeDelta time;
  for (const auto& segment : playlist->GetSegments()) {
    SegmentInfo info;
    info.index = segments_.size();
    info.segment = segment;
    info.absolute_start = time;
    time += segment->GetDuration();
    info.absolute_end = time;
    segments_.push_back(info);
  }

  fetch_queue_ = segments_.begin();
}

absl::optional<base::TimeDelta> HlsVodRendition::GetDuration() {
  return duration_;
}

void HlsVodRendition::CheckState(
    base::TimeDelta media_time,
    double playback_rate,
    ManifestDemuxer::DelayCallback time_remaining_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (segments_.empty()) {
    std::move(time_remaining_cb).Run(kNoTimestamp);
    return;
  }

  auto ranges = engine_host_->GetBufferedRanges(role_);
  if (ranges.empty()) {
    // Ensure that we don't have a half-fetched stream when there's nothing
    // loaded, since this case implies playback has just started or just
    // finished a seek.
    CHECK(!pending_stream_fetch_.has_value());

    if (fetch_queue_ == segments_.end()) {
      std::move(time_remaining_cb).Run(kNoTimestamp);
      return;
    }

    FetchNext(base::BindOnce(std::move(time_remaining_cb), base::Seconds(0)),
              media_time);
    return;
  }

  // Not being in the last loaded range is an error.
  // TODO(crbug/1266991) This can sometimes happen if the manifest lies about
  // the duration and gets to the end of playback, and a user seeks there.
  if (!ranges.contains(ranges.size() - 1, media_time)) {
    PipelineStatus error = DEMUXER_ERROR_COULD_NOT_PARSE;
    engine_host_->OnError(std::move(error)
                              .WithData("timestamp", media_time)
                              .WithData("range_start", ranges.back().first)
                              .WithData("range_end", ranges.back().second));
    return;
  }

  // Determine if we should delay and clear out old buffered frames.
  auto remaining = ranges.back().second - media_time;
  auto remaining_rate = playback_rate ? playback_rate : 1;
  remaining /= remaining_rate;

  // If the remaining time is large enough, then there is time to clear old
  // data and delay for a bit. "Large enough" is calculated to be at least 3
  // seconds, chosen based on the "slow 3g" connection setting in devtools for
  // a 1080p h264 video stream. "Large enough" must also be much more than the
  // amount of time it would take to fetch the next segment, and the 6x
  // multiplier was again chosen after messing about with the devtools network
  // speed panel. The delay is then chosen such that there is roughly 4x as long
  // to make a fetch request as it should take, based on the running average, or
  // 1 second, whichever is longer.
  // TODO(crbug/1266991) Update the `fetch_time_` moving average when network
  // speed changes.
  if (remaining > base::Seconds(3) && remaining > fetch_time_.Average() * 6) {
    // We have buffered enough to have time to clear old segments and delay.
    base::TimeDelta time_to_clear = ClearOldSegments(media_time);
    auto delay_time = kNoTimestamp;
    if (playback_rate) {
      delay_time = remaining - (fetch_time_.Average() * 10) - time_to_clear -
                   base::Seconds(1);
      if (delay_time < base::TimeDelta()) {
        delay_time = base::TimeDelta();
      }
    }
    std::move(time_remaining_cb).Run(delay_time);
    return;
  }

  // If there is nothing more to fetch, then playback should just continue until
  // the end and stop.
  if (!pending_stream_fetch_.has_value() && fetch_queue_ == segments_.end()) {
    std::move(time_remaining_cb).Run(kNoTimestamp);
    return;
  }

  FetchNext(base::BindOnce(std::move(time_remaining_cb), base::Seconds(0)),
            media_time);
}

bool HlsVodRendition::Seek(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto ranges = engine_host_->GetBufferedRanges(role_);
  if (!ranges.empty()) {
    if (ranges.contains(ranges.size() - 1, seek_time)) {
      // Only if we're in the last loaded range is it ok to keep the same
      // fetch queue and ranges.
      return false;
    }
  }

  pending_stream_fetch_ = absl::nullopt;
  fetch_queue_ =
      std::lower_bound(segments_.begin(), segments_.end(), seek_time,
                       [](const SegmentInfo& segment, base::TimeDelta time) {
                         return segment.absolute_end < time;
                       });

  if (fetch_queue_ != segments_.end()) {
    engine_host_->EvictCodedFrames(role_, base::Seconds(0), 0);

    engine_host_->RemoveAndReset(role_, base::TimeDelta(), duration_,
                                 &parse_offset_);

    engine_host_->SetGroupStartIfParsingAndSequenceMode(
        role_, (*fetch_queue_).absolute_start);
    return true;
  }

  return false;
}

void HlsVodRendition::CancelPendingNetworkRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Nothing to do!
}

base::TimeDelta HlsVodRendition::ClearOldSegments(base::TimeDelta media_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks removal_start = base::TimeTicks::Now();
  auto it =
      std::lower_bound(segments_.begin(), segments_.end(), media_time,
                       [](const SegmentInfo& segment, base::TimeDelta time) {
                         return segment.absolute_end < time;
                       });
  if (it != segments_.end()) {
    if ((*it).absolute_start > base::TimeDelta()) {
      engine_host_->Remove(role_, base::TimeDelta(), (*it).absolute_start);
    }
  }
  return base::TimeTicks::Now() - removal_start;
}

void HlsVodRendition::FetchNext(base::OnceClosure cb, base::TimeDelta time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(pending_stream_fetch_.has_value() || fetch_queue_ != segments_.end());
  if (pending_stream_fetch_.has_value()) {
    FetchMoreDataFromPendingStream(std::move(cb), time);
    return;
  }

  SegmentInfo* segment = &*fetch_queue_;
  std::advance(fetch_queue_, 1);
  LoadSegment(segment, time, std::move(cb));
}

void HlsVodRendition::LoadSegment(SegmentInfo* segment,
                                  base::TimeDelta fetch_required_time,
                                  base::OnceClosure cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(segment);
  rendition_host_->ReadFromUrl(
      segment->segment->GetUri(), true, segment->segment->GetByteRange(),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &HlsVodRendition::OnSegmentData, weak_factory_.GetWeakPtr(),
          std::move(cb), std::move(fetch_required_time), segment->index,
          base::TimeTicks::Now())));
}

void HlsVodRendition::FetchMoreDataFromPendingStream(
    base::OnceClosure cb,
    base::TimeDelta fetch_required_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(pending_stream_fetch_.has_value());
  auto stream = std::move(pending_stream_fetch_->stream);
  const SegmentInfo& segment = segments_[pending_stream_fetch_->index];
  pending_stream_fetch_.reset();
  std::move(stream).ReadChunk(
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &HlsVodRendition::OnSegmentData, weak_factory_.GetWeakPtr(),
          std::move(cb), std::move(fetch_required_time), segment.index,
          base::TimeTicks::Now())),
      kChunkSize);
}

void HlsVodRendition::OnSegmentData(base::OnceClosure cb,
                                    base::TimeDelta required_time,
                                    size_t segment_index,
                                    base::TimeTicks net_req_start,
                                    HlsDataSourceStream::ReadResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.has_value()) {
    // Drop |cb| here, and let the abort handler pick up the pieces.
    return engine_host_->OnError(
        {DEMUXER_ERROR_COULD_NOT_PARSE, std::move(result).error()});
  }
  CHECK_LT(segment_index, segments_.size());

  base::TimeDelta end =
      segments_[segment_index].absolute_start + segment_duration_upper_limit_;

  auto source = std::move(result).value();

  if (!engine_host_->AppendAndParseData(
          role_, base::TimeDelta(), end + base::Seconds(1), &parse_offset_,
          source.AsRawData(), source.BytesInBuffer())) {
    return engine_host_->OnError(DEMUXER_ERROR_COULD_NOT_PARSE);
  }

  auto fetch_duration = base::TimeTicks::Now() - net_req_start;
  // Store the time it took to download this chunk. The time should be scaled
  // for situations where we only have a few bytes left to download.
  auto scaled = (fetch_duration * source.BytesInBuffer()) / kChunkSize;
  fetch_time_.AddSample(scaled);

  if (source.CanReadMore()) {
    source.Flush();
    pending_stream_fetch_.emplace(std::move(source), segment_index);
  }

  // After a seek especially, we will start loading content that comes
  // potentially much earlier than the seek time, and it's possible that the
  // loaded ranges won't yet contain the timestamp that is required to be loaded
  // for the seek to complete. In this case, we just keep fetching until
  // the seek time is loaded.

  auto ranges = engine_host_->GetBufferedRanges(role_);
  if (ranges.size() && ranges.contains(ranges.size() - 1, required_time)) {
    std::move(cb).Run();
    return;
  }

  // If the last range doesn't contain the timestamp, keep parsing until it
  // does. If there is nothing left to download, then we can return.
  if (!pending_stream_fetch_.has_value() && fetch_queue_ == segments_.end()) {
    std::move(cb).Run();
    return;
  }

  FetchNext(std::move(cb), required_time);
}

}  // namespace media
