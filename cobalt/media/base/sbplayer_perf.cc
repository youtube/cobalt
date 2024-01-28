// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/base/sbplayer_perf.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cobalt/base/statistics.h"
#include "starboard/common/once.h"
#include "starboard/extension/player_perf.h"
#include "starboard/player.h"
#include "starboard/system.h"

namespace cobalt {
namespace media {

namespace {

// Sampling every 1 second
const base::TimeDelta sampling_interval = base::Seconds(1);

// Measuring for 2 minutes
const int64_t kMaxMeasurementSamples = 120;

class StatisticsWrapper {
 public:
  static StatisticsWrapper* GetInstance();
  base::Statistics<double, int, 1024> cpu_usage{"SbPlayer.CPUUsage"};
  base::Statistics<double, int, 1024> decoded_speed{"SbPlayer.decoded_speed"};
  base::Statistics<double, int, 1024> audio_track_out{
      "SbPlayer.audio_track_out"};
  base::Statistics<double, int, 1024> video_decoder{"SbPlayer.video_decoder"};
  base::Statistics<double, int, 1024> audio_decoder{"SbPlayer.audio_decoder"};
  base::Statistics<double, int, 1024> media_pipeline{"SbPlayer.media_pipeline"};
  base::Statistics<double, int, 1024> player_worker{"SbPlayer.player_worker"};
  base::Statistics<double, int, 1024> main_web_module{
      "SbPlayer.main_web_module"};
  base::Statistics<double, int, 1024> network_module{"SbPlayer.network_module"};
  base::Statistics<double, int, 1024> sbplayer_perf{"SbPlayer.sbplayer_perf"};
};

}  // namespace

SB_ONCE_INITIALIZE_FUNCTION(StatisticsWrapper, StatisticsWrapper::GetInstance);

void SbPlayerPerf::Start() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPerf::StartTask, base::Unretained(this)));
}

void SbPlayerPerf::StartTask() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  start_timestamp_ = Time::Now();
  timer_.Start(FROM_HERE, sampling_interval, this,
               &SbPlayerPerf::GetPlatformIndependentCPUUsage);
}

void SbPlayerPerf::Stop() {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&SbPlayerPerf::StopTask, base::Unretained(this)));
}

void SbPlayerPerf::StopTask() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  timer_.Stop();
}

void SbPlayerPerf::GetPlatformIndependentCPUUsage() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  const StarboardExtensionPlayerPerfApi* player_perf_extension =
      static_cast<const StarboardExtensionPlayerPerfApi*>(
          SbSystemGetExtension(kStarboardExtensionPlayerPerfName));
  if (player_perf_extension &&
      strcmp(player_perf_extension->name, kStarboardExtensionPlayerPerfName) ==
          0 &&
      player_perf_extension->version >= 1) {
    Time timestamp_now = Time::Now();
    base::Percentiles cpu_out_percentiles, decoded_speed_out_percentiles;
    base::Percentiles audio_track_out_out_percentiles,
        video_decoder_out_percentiles;
    base::Percentiles audio_decoder_out_percentiles,
        media_pipeline_out_percentiles;
    base::Percentiles player_worker_out_percentiles,
        sbplayer_perf_out_percentiles, main_web_module_out_percentiles,
        network_module_out_percentiles;
    std::unordered_map<std::string, double> map_cumulative_cpu =
        player_perf_extension->GetPlatformIndependentCPUUsage();
    double cpu_usage_perf = map_cumulative_cpu["system"];

#if SB_API_VERSION >= 15
    SbPlayerInfo info;
#else   // SB_API_VERSION >= 15
    SbPlayerInfo2 info;
#endif  // SB_API_VERSION >= 15
    interface_->GetInfo(player_, &info);
    if (cpu_usage_perf == 0.0 &&
        StatisticsWrapper::GetInstance()->cpu_usage.GetNumberOfSamples() == 0 &&
        decoded_frames_last_ == 0) {
      // The first sample is 0, so skip it
      decoded_frames_last_ = info.total_video_frames;
      timestamp_last_ = timestamp_now;
      return;
    }

    std::string result_str = "CPU:" + std::to_string(cpu_usage_perf) + ";";
    StatisticsWrapper::GetInstance()->cpu_usage.AddSample(cpu_usage_perf, 1);
    for (auto element : map_cumulative_cpu) {
      if (element.second == 0) continue;
      if (element.first.compare("audio_track_out") == 0) {
        StatisticsWrapper::GetInstance()->audio_track_out.AddSample(
            element.second, 1);
      } else if (element.first.compare("video_decoder") == 0) {
        StatisticsWrapper::GetInstance()->video_decoder.AddSample(
            element.second, 1);
      } else if (element.first.compare("audio_decoder") == 0) {
        StatisticsWrapper::GetInstance()->audio_decoder.AddSample(
            element.second, 1);
      } else if (element.first.compare("media_pipeline") == 0) {
        StatisticsWrapper::GetInstance()->media_pipeline.AddSample(
            element.second, 1);
      } else if (element.first.compare("player_worker") == 0) {
        StatisticsWrapper::GetInstance()->player_worker.AddSample(
            element.second, 1);
      } else if (element.first.compare("sbplayer_perf") == 0) {
        StatisticsWrapper::GetInstance()->sbplayer_perf.AddSample(
            element.second, 1);
      } else if (element.first.compare("MainWebModule") == 0) {
        StatisticsWrapper::GetInstance()->main_web_module.AddSample(
            element.second, 1);
      } else if (element.first.compare("NetworkModule") == 0) {
        StatisticsWrapper::GetInstance()->network_module.AddSample(
            element.second, 1);
      }
      result_str += element.first + ":" + std::to_string(element.second) + ";";
    }

    int decoded_frames = info.total_video_frames - decoded_frames_last_;
    TimeDelta interval = timestamp_now - timestamp_last_;
    if (decoded_frames > 0 && interval.InSeconds() > 0) {
      double decoded_speed = static_cast<double>(decoded_frames) /
                             static_cast<double>(interval.InSeconds());
      StatisticsWrapper::GetInstance()->decoded_speed.AddSample(decoded_speed,
                                                                1);
    }
    decoded_frames_last_ = info.total_video_frames;
    timestamp_last_ = timestamp_now;

    result_str +=
        "Total_Frames:" + std::to_string(info.total_video_frames) + ";";
    result_str +=
        "Dropped_Frames:" + std::to_string(info.dropped_video_frames) + ";";
    // LOG(ERROR) << "Brown " << result_str.c_str();

    if (StatisticsWrapper::GetInstance()->cpu_usage.GetNumberOfSamples() %
            kMaxMeasurementSamples ==
        0) {
      StatisticsWrapper::GetInstance()->cpu_usage.GetPercentiles(
          &cpu_out_percentiles);
      StatisticsWrapper::GetInstance()->decoded_speed.GetPercentiles(
          &decoded_speed_out_percentiles);
      StatisticsWrapper::GetInstance()->audio_track_out.GetPercentiles(
          &audio_track_out_out_percentiles);
      StatisticsWrapper::GetInstance()->video_decoder.GetPercentiles(
          &video_decoder_out_percentiles);
      StatisticsWrapper::GetInstance()->audio_decoder.GetPercentiles(
          &audio_decoder_out_percentiles);
      StatisticsWrapper::GetInstance()->media_pipeline.GetPercentiles(
          &media_pipeline_out_percentiles);
      StatisticsWrapper::GetInstance()->player_worker.GetPercentiles(
          &player_worker_out_percentiles);
      StatisticsWrapper::GetInstance()->main_web_module.GetPercentiles(
          &main_web_module_out_percentiles);
      StatisticsWrapper::GetInstance()->network_module.GetPercentiles(
          &network_module_out_percentiles);
      StatisticsWrapper::GetInstance()->sbplayer_perf.GetPercentiles(
          &sbplayer_perf_out_percentiles);

      double dropped_frame_percentage =
          100.0 * static_cast<double>(info.dropped_video_frames) /
          static_cast<double>(info.total_video_frames);
      TimeDelta duration = Time::Now() - start_timestamp_;
      LOG(ERROR)
          << "Brown Brand/model_name:" << sb_system_property_brand_name_ << "_"
          << sb_system_property_model_name_
          << ";;platform_name:" << sb_system_property_platform_name_
          << ";;num_of_processors:" << sb_number_of_processors_
          << ";;duration:" << duration.InSeconds() << ";;audio_codec:"
          << player_perf_extension->GetCurrentMediaAudioCodecName()
          << ";;video_codec:"
          << player_perf_extension->GetCurrentMediaVideoCodecName()
          << ";;should_be_paused:"
          << player_perf_extension->GetCountShouldBePaused()
          << ";;dropped_frames(%):" << dropped_frame_percentage
          << ",dropped_frames:" << info.dropped_video_frames
          << ",total_frames:" << info.total_video_frames
          << ";;audio_underrun_count:"
          << player_perf_extension->GetAudioUnderrunCount()
          << ";;playback_rate:" << info.playback_rate
          << ";;use_neon:" << player_perf_extension->IsSIMDEnabled()
          << ";;force_tunnel_mode:"
          << player_perf_extension->IsForceTunnelMode()
          << ";;Statistics_CPU_usage(%)::whislo:"
          << StatisticsWrapper::GetInstance()->cpu_usage.min()
          << ",q1:" << cpu_out_percentiles.percentile_25th
          << ",med:" << cpu_out_percentiles.median
          << ",q3:" << cpu_out_percentiles.percentile_75th << ",average:"
          << StatisticsWrapper::GetInstance()->cpu_usage.average()
          << ",whishi:" << StatisticsWrapper::GetInstance()->cpu_usage.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()->cpu_usage.GetNumberOfSamples()
          << ";;Statistics_main_web_module(%)::whislo:"
          << StatisticsWrapper::GetInstance()->main_web_module.min()
          << ",q1:" << main_web_module_out_percentiles.percentile_25th
          << ",med:" << main_web_module_out_percentiles.median
          << ",q3:" << main_web_module_out_percentiles.percentile_75th
          << ",average:"
          << StatisticsWrapper::GetInstance()->main_web_module.average()
          << ",whishi:"
          << StatisticsWrapper::GetInstance()->main_web_module.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()
                 ->main_web_module.GetNumberOfSamples()
          << ";;Statistics_network_module(%)::whislo:"
          << StatisticsWrapper::GetInstance()->network_module.min()
          << ",q1:" << network_module_out_percentiles.percentile_25th
          << ",med:" << network_module_out_percentiles.median
          << ",q3:" << network_module_out_percentiles.percentile_75th
          << ",average:"
          << StatisticsWrapper::GetInstance()->network_module.average()
          << ",whishi:"
          << StatisticsWrapper::GetInstance()->network_module.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()
                 ->network_module.GetNumberOfSamples()
          << ";;Statistics_media_pipeline(%)::whislo:"
          << StatisticsWrapper::GetInstance()->media_pipeline.min()
          << ",q1:" << media_pipeline_out_percentiles.percentile_25th
          << ",med:" << media_pipeline_out_percentiles.median
          << ",q3:" << media_pipeline_out_percentiles.percentile_75th
          << ",average:"
          << StatisticsWrapper::GetInstance()->media_pipeline.average()
          << ",whishi:"
          << StatisticsWrapper::GetInstance()->media_pipeline.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()
                 ->media_pipeline.GetNumberOfSamples()
          << ";;Statistics_player_worker(%)::whislo:"
          << StatisticsWrapper::GetInstance()->player_worker.min()
          << ",q1:" << player_worker_out_percentiles.percentile_25th
          << ",med:" << player_worker_out_percentiles.median
          << ",q3:" << player_worker_out_percentiles.percentile_75th
          << ",average:"
          << StatisticsWrapper::GetInstance()->player_worker.average()
          << ",whishi:" << StatisticsWrapper::GetInstance()->player_worker.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()
                 ->player_worker.GetNumberOfSamples()
          << ";;Statistics_video_decoder(%)::whislo:"
          << StatisticsWrapper::GetInstance()->video_decoder.min()
          << ",q1:" << video_decoder_out_percentiles.percentile_25th
          << ",med:" << video_decoder_out_percentiles.median
          << ",q3:" << video_decoder_out_percentiles.percentile_75th
          << ",average:"
          << StatisticsWrapper::GetInstance()->video_decoder.average()
          << ",whishi:" << StatisticsWrapper::GetInstance()->video_decoder.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()
                 ->video_decoder.GetNumberOfSamples()
          << ";;Statistics_audio_decoder(%)::whislo:"
          << StatisticsWrapper::GetInstance()->audio_decoder.min()
          << ",q1:" << audio_decoder_out_percentiles.percentile_25th
          << ",med:" << audio_decoder_out_percentiles.median
          << ",q3:" << audio_decoder_out_percentiles.percentile_75th
          << ",average:"
          << StatisticsWrapper::GetInstance()->audio_decoder.average()
          << ",whishi:" << StatisticsWrapper::GetInstance()->audio_decoder.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()
                 ->audio_decoder.GetNumberOfSamples()
          << ";;Statistics_audio_track_out(%)::whislo:"
          << StatisticsWrapper::GetInstance()->audio_track_out.min()
          << ",q1:" << audio_track_out_out_percentiles.percentile_25th
          << ",med:" << audio_track_out_out_percentiles.median
          << ",q3:" << audio_track_out_out_percentiles.percentile_75th
          << ",average:"
          << StatisticsWrapper::GetInstance()->audio_track_out.average()
          << ",whishi:"
          << StatisticsWrapper::GetInstance()->audio_track_out.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()
                 ->audio_track_out.GetNumberOfSamples()
          << ";;Statistics_sbplayer_perf(%)::whislo:"
          << StatisticsWrapper::GetInstance()->sbplayer_perf.min()
          << ",q1:" << sbplayer_perf_out_percentiles.percentile_25th
          << ",med:" << sbplayer_perf_out_percentiles.median
          << ",q3:" << sbplayer_perf_out_percentiles.percentile_75th
          << ",average:"
          << StatisticsWrapper::GetInstance()->sbplayer_perf.average()
          << ",whishi:" << StatisticsWrapper::GetInstance()->sbplayer_perf.max()
          << ",nsamples:"
          << StatisticsWrapper::GetInstance()
                 ->sbplayer_perf.GetNumberOfSamples();
      /*<< ";;Statistics_decoded_speed(fps)::whislo:"
      << StatisticsWrapper::GetInstance()->decoded_speed.min()
      << ",q1:" << decoded_speed_out_percentiles.percentile_25th
      << ",med:" << decoded_speed_out_percentiles.median
      << ",q3:" << decoded_speed_out_percentiles.percentile_75th
      << ",average:"
      << StatisticsWrapper::GetInstance()->decoded_speed.average()
      << ",whishi:" << StatisticsWrapper::GetInstance()->decoded_speed.max();*/
    }
  }
}

std::string SbPlayerPerf::GetSbSystemProperty(SbSystemPropertyId property_id) {
  char property[1024] = {0};
  std::string result = "";
  if (!SbSystemGetProperty(property_id, property,
                           SB_ARRAY_SIZE_INT(property))) {
    DLOG(FATAL) << "Failed to get kSbSystemPropertyPlatformName.";
    return result;
  }
  result = property;
  return result;
}

SbPlayerPerf::SbPlayerPerf(
    SbPlayerInterface* interface, SbPlayer player,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner),
      interface_(interface),
      player_(player),
      decoded_frames_last_(0),
      sb_number_of_processors_(SbSystemGetNumberOfProcessors()),
      sb_system_property_brand_name_(
          GetSbSystemProperty(kSbSystemPropertyBrandName)),
      sb_system_property_model_name_(
          GetSbSystemProperty(kSbSystemPropertyModelName)),
      sb_system_property_platform_name_(
          GetSbSystemProperty(kSbSystemPropertyPlatformName)) {}

SbPlayerPerf::~SbPlayerPerf() {}

}  // namespace media
}  // namespace cobalt
