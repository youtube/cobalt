#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_FLOW_CONTROL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_FLOW_CONTROL_H_

#include <functional>
#include <iosfwd>
#include <memory>

namespace starboard::shared::starboard::media {

class DecoderFlowControl {
 public:
  using StateChangedCB = std::function<void()>;

  struct State {
    int decoding_frames = 0;
    int decoded_frames = 0;
    int decoding_frames_high_water_mark = 0;
    int decoded_frames_high_water_mark = 0;
    int total_frames_high_water_mark = 0;
    int64_t min_decoding_time_us = 0;
    int64_t max_decoding_time_us = 0;
    int64_t avg_decoding_time_us = 0;

    int total_frames() const { return decoding_frames + decoded_frames; }
  };

  static std::unique_ptr<DecoderFlowControl> CreateNoOp();
  static std::unique_ptr<DecoderFlowControl> CreateThrottling(
      int max_frames,
      int64_t log_interval_us,
      StateChangedCB state_changed_cb);

  virtual ~DecoderFlowControl() = default;

  virtual bool AddFrame(int64_t presentation_time_us) = 0;
  virtual bool SetFrameDecoded(int64_t presentation_time_us) = 0;
  virtual bool ReleaseFrameAt(int64_t release_us) = 0;

  virtual State GetCurrentState() const = 0;
  virtual bool CanAcceptMore() = 0;
};

std::ostream& operator<<(std::ostream& os,
                         const DecoderFlowControl::State& status);

}  // namespace starboard::shared::starboard::media

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_DECODER_FLOW_CONTROL_H_
