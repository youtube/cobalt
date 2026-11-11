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

#include <pthread.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "starboard/benchmark/player/player_benchmark_helper.h"
#include "starboard/common/log.h"
#include "starboard/player.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

using Clock = SbPlayerBenchmarkHelper::Clock;
using WallClock = std::chrono::system_clock;

// Options are read from --endurance_<name>=<value> flags on the test command
// line. The prefix keeps them apart from the flags of the test runner and of
// Google Benchmark.
struct EnduranceOptions {
  int iterations = 20;
  int gap_ms = 0;
  int seeks = 0;
  int concurrent_players = 0;
  int hang_timeout_ms = 30'000;
  std::string video_spec = "vertical_1080p_24_fps_399_av1.dmp";
  std::vector<std::string> video_files = {"vertical_1080p_24_fps_399_av1.dmp"};
  // An empty audio file creates video-only players.
  std::string audio_file = "beneath_the_canopy_aac_stereo.dmp";
  SbPlayerOutputMode output_mode = kSbPlayerOutputModePunchOut;
  std::string output_mode_name = "punch_out";
  // Must be absolute: the working directory of a test app is not writable.
  std::string csv_path;
  // Starboard experimental features for the created players.
  std::map<std::string, int64_t> experimental_features;
};

std::string CollectFlags(std::map<std::string, std::string>* out_flags) {
  constexpr char kPrefix[] = "--endurance_";
  constexpr size_t kPrefixLen = sizeof(kPrefix) - 1;
  // ::testing::internal::GetArgvs() is an internal GoogleTest API, used here
  // (as in benchmark_gtest_wrapper.cc) because starboard_test_main owns main()
  // and this is the only way to see the full test command line.
  for (const std::string& arg : ::testing::internal::GetArgvs()) {
    if (arg.rfind("--endurance", 0) != 0) {
      continue;
    }
    const size_t equals = arg.rfind(kPrefix, 0) == 0 ? arg.find('=', kPrefixLen)
                                                     : std::string::npos;
    if (equals == kPrefixLen || equals == std::string::npos) {
      return "Malformed flag '" + arg +
             "' (expected --endurance_<name>=<value>)";
    }
    const std::string name = arg.substr(kPrefixLen, equals - kPrefixLen);
    if (!out_flags->emplace(name, arg.substr(equals + 1)).second) {
      return "Duplicate flag '--endurance_" + name + "'";
    }
  }
  return "";
}

// Parses a comma-separated list of .dmp clip names into |out_files|. Returns an
// error message if the list or any entry is empty or contains whitespace.
std::string ParseVideoFiles(const std::string& value,
                            std::vector<std::string>* out_files) {
  if (value.empty()) {
    return "--endurance_video must not be empty";
  }
  out_files->clear();
  size_t start = 0;
  while (start <= value.size()) {
    size_t end = value.find(',', start);
    if (end == std::string::npos) {
      end = value.size();
    }
    const std::string entry = value.substr(start, end - start);
    if (entry.empty() || entry.find_first_of(" \t") != std::string::npos) {
      return "--endurance_video has invalid clip entry '" + entry + "'";
    }
    out_files->push_back(entry);
    start = end + 1;
  }
  return "";
}

// Parses "Key1=1,Key2=0" into |out_features|. Returns an error message naming
// the offending or duplicate entry, or an empty string if valid.
std::string ParseExperimentalFeatures(
    const std::string& value,
    std::map<std::string, int64_t>* out_features) {
  if (value.empty()) {
    return "--endurance_experimental_features must not be empty";
  }
  size_t start = 0;
  while (start <= value.size()) {
    size_t end = value.find(',', start);
    if (end == std::string::npos) {
      end = value.size();
    }
    const std::string entry = value.substr(start, end - start);
    const size_t equals = entry.find('=');
    if (equals == 0 || equals == std::string::npos ||
        entry.find_first_of(" \t") != std::string::npos) {
      return "--endurance_experimental_features has malformed entry '" + entry +
             "' (expected Key=<integer>)";
    }
    int64_t feature_value;
    const char* value_begin = entry.data() + equals + 1;
    const char* value_end = entry.data() + entry.size();
    auto [ptr, error] = std::from_chars(value_begin, value_end, feature_value);
    if (error != std::errc() || ptr != value_end || value_begin == value_end) {
      return "--endurance_experimental_features has malformed entry '" + entry +
             "' (expected Key=<integer>)";
    }
    const std::string key = entry.substr(0, equals);
    if (!out_features->emplace(key, feature_value).second) {
      return "--endurance_experimental_features lists duplicate key '" + key +
             "' in '" + entry + "'";
    }
    start = end + 1;
  }
  return "";
}

std::string FormatExperimentalFeatures(
    const std::map<std::string, int64_t>& features) {
  if (features.empty()) {
    return "none";
  }
  std::string result;
  for (const auto& [key, value] : features) {
    result += (result.empty() ? "" : ",") + key + "=" + std::to_string(value);
  }
  return result;
}

bool ParseInt(const std::string& value, int min_value, int* out_value) {
  const char* end = value.data() + value.size();
  auto [ptr, error] = std::from_chars(value.data(), end, *out_value);
  return error == std::errc() && ptr == end && *out_value >= min_value;
}

// Returns an error message, or an empty string if all flags are valid.
std::string ParseOptions(EnduranceOptions* options) {
  std::map<std::string, std::string> flags;
  const std::string flags_error = CollectFlags(&flags);
  if (!flags_error.empty()) {
    return flags_error;
  }
  auto take_flag = [&flags](const char* name) -> std::optional<std::string> {
    auto it = flags.find(name);
    if (it == flags.end()) {
      return std::nullopt;
    }
    std::string value = std::move(it->second);
    flags.erase(it);
    return value;
  };

  if (auto value = take_flag("iterations")) {
    if (!ParseInt(*value, 1, &options->iterations)) {
      return "--endurance_iterations must be a positive integer";
    }
  }
  if (auto value = take_flag("gap_ms")) {
    if (!ParseInt(*value, 0, &options->gap_ms)) {
      return "--endurance_gap_ms must be a non-negative integer";
    }
  }
  if (auto value = take_flag("seeks")) {
    if (!ParseInt(*value, 0, &options->seeks)) {
      return "--endurance_seeks must be a non-negative integer";
    }
  }
  if (auto value = take_flag("concurrent_players")) {
    if (!ParseInt(*value, 0, &options->concurrent_players)) {
      return "--endurance_concurrent_players must be a non-negative integer";
    }
  }
  if (auto value = take_flag("hang_timeout_ms")) {
    if (!ParseInt(*value, 1, &options->hang_timeout_ms)) {
      return "--endurance_hang_timeout_ms must be a positive integer";
    }
  }
  if (auto value = take_flag("video")) {
    options->video_spec = *value;
    const std::string video_error =
        ParseVideoFiles(*value, &options->video_files);
    if (!video_error.empty()) {
      return video_error;
    }
  }
  if (auto value = take_flag("audio")) {
    options->audio_file = *value;
  }
  if (auto value = take_flag("output_mode")) {
    if (*value == "punch_out") {
      options->output_mode = kSbPlayerOutputModePunchOut;
    } else if (*value == "decode_to_texture") {
      options->output_mode = kSbPlayerOutputModeDecodeToTexture;
    } else {
      return "--endurance_output_mode must be punch_out or decode_to_texture";
    }
    options->output_mode_name = *value;
  }
  if (auto value = take_flag("experimental_features")) {
    const std::string features_error =
        ParseExperimentalFeatures(*value, &options->experimental_features);
    if (!features_error.empty()) {
      return features_error;
    }
  }
  options->csv_path = take_flag("csv").value_or("");
  if (!flags.empty()) {
    return "Unknown flag '--endurance_" + flags.begin()->first + "'";
  }
  if (options->csv_path.empty() || options->csv_path[0] != '/') {
    return "--endurance_csv=<absolute path> is required";
  }
  return "";
}

std::string FormatLocalTime(WallClock::time_point time) {
  const std::time_t seconds = WallClock::to_time_t(time);
  const int milliseconds =
      static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                           time.time_since_epoch())
                           .count() %
                       1000);
  std::tm local_time = {};
  localtime_r(&seconds, &local_time);
  char date_time[32];
  std::strftime(date_time, sizeof(date_time), "%Y-%m-%d %H:%M:%S", &local_time);
  char result[40];
  std::snprintf(result, sizeof(result), "%s.%03d", date_time, milliseconds);
  return result;
}

std::string FormatMilliseconds(std::optional<Clock::duration> duration) {
  if (!duration) {
    return "";
  }
  char result[32];
  std::snprintf(result, sizeof(result), "%.3f",
                std::chrono::duration<double, std::milli>(*duration).count());
  return result;
}

std::string QuoteCsvField(const std::string& field) {
  std::string result = "\"";
  for (char c : field) {
    if (c == '"') {
      result += '"';
    }
    result += (c == '\n' || c == '\r') ? ' ' : c;
  }
  return result + "\"";
}

struct IterationResult {
  int iteration = 0;
  WallClock::time_point start_time;
  std::string video;
  std::optional<Clock::duration> gap;
  std::optional<Clock::duration> create;
  std::optional<Clock::duration> create_to_initialized;
  std::optional<Clock::duration> create_to_presenting;
  int seeks_completed = 0;
  std::optional<Clock::duration> seek_median;
  std::optional<Clock::duration> seek_max;
  std::optional<Clock::duration> destroy;
  std::string status = "ok";
  std::optional<SbPlayerBenchmarkHelper::PlayerError> error;
};

int64_t ChooseSeekTargetUs(int64_t duration_us, int seek_index) {
  // Leave a 500 ms tail so post-seek preroll does not immediately hit EOS, and
  // step through 8 spread-out positions with a coprime stride (starting away
  // from 0) so consecutive seeks jump forward and backward across the clip.
  constexpr int64_t kTailMarginUs = 500'000;
  constexpr int kSlots = 8;
  const int64_t max_seek_us = duration_us > kTailMarginUs
                                  ? duration_us - kTailMarginUs
                                  : std::max<int64_t>(0, duration_us / 2);
  const int slot = (seek_index * 3 + 2) % kSlots;
  return (max_seek_us * slot) / (kSlots - 1);
}

void UpdateSeekStats(std::vector<Clock::duration> durations,
                     IterationResult* result) {
  result->seeks_completed = static_cast<int>(durations.size());
  if (durations.empty()) {
    return;
  }
  std::sort(durations.begin(), durations.end());
  const size_t mid = durations.size() / 2;
  result->seek_median = (durations.size() % 2 == 1)
                            ? durations[mid]
                            : (durations[mid - 1] + durations[mid]) / 2;
  result->seek_max = durations.back();
}

// Writes CSV rows from the test thread and the watchdog thread, flushing every
// row so that the rows written so far survive a crash or a killed run.
class CsvWriter {
 public:
  explicit CsvWriter(FILE* file) : file_(file, &std::fclose) {}

  void WriteLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::fputs(line.c_str(), file_.get());
    std::fputc('\n', file_.get());
    std::fflush(file_.get());
  }

  void WriteRow(const IterationResult& result) {
    const long long start_epoch_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            result.start_time.time_since_epoch())
            .count();
    WriteLine(std::to_string(result.iteration) + "," +
              FormatLocalTime(result.start_time) + "," +
              std::to_string(start_epoch_ms) + "," + result.video + "," +
              FormatMilliseconds(result.gap) + "," +
              FormatMilliseconds(result.create) + "," +
              FormatMilliseconds(result.create_to_initialized) + "," +
              FormatMilliseconds(result.create_to_presenting) + "," +
              std::to_string(result.seeks_completed) + "," +
              FormatMilliseconds(result.seek_median) + "," +
              FormatMilliseconds(result.seek_max) + "," +
              FormatMilliseconds(result.destroy) + "," + result.status + "," +
              (result.error ? std::to_string(result.error->error) : "") + "," +
              (result.error ? QuoteCsvField(result.error->message) : ""));
  }

 private:
  std::mutex mutex_;
  std::unique_ptr<FILE, decltype(&std::fclose)> file_;
};

// Calls |on_hang| on its own thread if a step started with Begin() has not
// ended with End() within |timeout|. The blocking SbPlayer calls cannot be
// interrupted, so this only makes a stuck step visible while it is stuck.
class Watchdog {
 public:
  using OnHang =
      std::function<void(const IterationResult& result, const char* step)>;

  Watchdog(Clock::duration timeout, OnHang on_hang)
      : timeout_(timeout),
        on_hang_(std::move(on_hang)),
        thread_([this] { Run(); }) {}

  ~Watchdog() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopped_ = true;
    }
    condition_variable_.notify_all();
    thread_.join();
  }

  void Begin(const IterationResult& result, const char* step) {
    std::lock_guard<std::mutex> lock(mutex_);
    result_ = result;
    step_ = step;
    deadline_ = Clock::now() + timeout_;
    armed_ = true;
    fired_ = false;
    condition_variable_.notify_all();
  }

  // Returns true if the step was reported as hung.
  bool End() {
    std::lock_guard<std::mutex> lock(mutex_);
    armed_ = false;
    return fired_;
  }

 private:
  void Run() {
    pthread_setname_np(pthread_self(), "watchdog");
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stopped_) {
      if (!armed_ || fired_) {
        condition_variable_.wait(lock);
      } else if (Clock::now() < deadline_) {
        condition_variable_.wait_until(lock, deadline_);
      } else {
        fired_ = true;
        on_hang_(result_, step_);
      }
    }
  }

  const Clock::duration timeout_;
  const OnHang on_hang_;

  std::mutex mutex_;
  std::condition_variable condition_variable_;
  bool stopped_ = false;
  bool armed_ = false;
  bool fired_ = false;
  IterationResult result_;
  const char* step_ = "";
  Clock::time_point deadline_;

  // Declared last so that the members above are initialized when it starts.
  std::thread thread_;
};

// Repeatedly creates an SbPlayer, prerolls it until it is presenting,
// optionally seeks it --endurance_seeks times (waiting for presenting after
// each seek), and destroys it, writing a CSV row for each iteration. When
// --endurance_video lists multiple comma-separated clips, iteration i uses
// clip i % count. When --endurance_concurrent_players=K is positive, K paused
// video-only decode-to-texture players are held in kSbPlayerStatePresenting
// across the entire loop so each iteration creates and destroys its decoder
// while K other decoders remain allocated.
//
// A failed iteration is recorded and the loop continues, so that rare failures
// can be counted over long runs. The waits on player callbacks are bounded by
// SbPlayerBenchmarkHelper::kDefaultTimeout. The blocking calls cannot be
// bounded: each of the create, presenting, seek and destroy steps that takes
// longer than --endurance_hang_timeout_ms gets a row with status create_hung,
// presenting_hung, seek_hung or destroy_hung, written while that step is still
// running, so an iteration can have up to four such rows. If the steps return,
// the iteration runs to its end, its final row records its timings with the
// status of the first stuck step, and the test stops with a failure, as the
// player may have been left in an unknown state. The last row of an iteration
// is the authoritative one.
//
// --endurance_experimental_features=Key1=1,Key2=0 sets Starboard experimental
// features, with the keys defined in
// starboard/shared/starboard/experimental_features.h, for every player the
// test creates. Some features have process-wide effects that cannot be undone
// within a process, so run each feature setting in its own invocation of the
// test. Starboard ignores keys it does not know, so the features are logged and
// written to the CSV, and a misspelled key shows only as a missing effect.
//
// The CSV starts with comment lines beginning with '#', which readers need to
// skip, followed by a header row. Players may create their decoders after
// reporting kSbPlayerStateInitialized, so compare create_to_presenting_ms
// rather than create_to_initialized_ms across codecs. All clips are loaded
// before the first iteration, so no iteration includes parsing them.
//
// The test is disabled so that it only runs when requested, e.g. on Android TV:
//   out/android-arm_qa/bin/run_player_benchmarks \
//       --gtest_also_run_disabled_tests \
//       --gtest_filter='SbPlayerEnduranceTest.*' \
//       --endurance_csv=/sdcard/Download/endurance.csv \
//       [--endurance_iterations=20] [--endurance_gap_ms=0] \
//       [--endurance_seeks=0] [--endurance_concurrent_players=0] \
//       [--endurance_video=vertical_1080p_24_fps_399_av1.dmp[,clip2.dmp,...]] \
//       [--endurance_audio=beneath_the_canopy_aac_stereo.dmp] \
//       [--endurance_output_mode=punch_out|decode_to_texture] \
//       [--endurance_hang_timeout_ms=30000] \
//       [--endurance_experimental_features=Key1=1,Key2=0]
TEST(SbPlayerEnduranceTest, DISABLED_CreateDestroy) {
  EnduranceOptions options;
  const std::string error = ParseOptions(&options);
  ASSERT_TRUE(error.empty()) << error;

  SbPlayerBenchmarkHelper helper;
  // Declared after |helper| so they are destroyed before |helper|'s
  // FakeGraphicsContextProvider on every exit path. Constructed before
  // SetExperimentalFeatures() because SbPlayerBenchmarkHelper's constructor
  // resets the calling thread's experimental features to empty.
  std::vector<std::unique_ptr<SbPlayerBenchmarkHelper>> concurrent_helpers;
  concurrent_helpers.reserve(options.concurrent_players);
  for (int k = 0; k < options.concurrent_players; ++k) {
    concurrent_helpers.push_back(std::make_unique<SbPlayerBenchmarkHelper>(
        helper.graphics_context_provider()));
  }

  const std::string features =
      FormatExperimentalFeatures(options.experimental_features);
  if (!options.experimental_features.empty()) {
    ASSERT_TRUE(helper.SetExperimentalFeatures(options.experimental_features))
        << "Experimental features are not supported on this platform";
  }
  SB_LOG(INFO) << "Endurance experimental features: " << features;

  const char* audio_file =
      options.audio_file.empty() ? nullptr : options.audio_file.c_str();
  for (const std::string& video_file : options.video_files) {
    std::string load_error;
    ASSERT_TRUE(
        helper.LoadDmpFiles(audio_file, video_file.c_str(), &load_error))
        << load_error;
    ASSERT_TRUE(helper.IsOutputModeSupportedForDmpFiles(options.output_mode))
        << "Output mode " << options.output_mode_name
        << " is not supported for " << video_file;
  }

  for (int k = 0; k < options.concurrent_players; ++k) {
    const std::string& bg_video =
        options.video_files[k % options.video_files.size()];
    SbPlayerBenchmarkHelper* bg_helper = concurrent_helpers[k].get();
    std::string load_error;
    ASSERT_TRUE(bg_helper->LoadDmpFiles(nullptr, bg_video.c_str(), &load_error))
        << load_error;
    ASSERT_TRUE(bg_helper->IsOutputModeSupportedForDmpFiles(
        kSbPlayerOutputModeDecodeToTexture))
        << "Output mode decode_to_texture is not supported for concurrent "
           "player clip "
        << bg_video;
    ASSERT_EQ(bg_helper->InitializePlayer(nullptr, bg_video.c_str(),
                                          kSbPlayerOutputModeDecodeToTexture),
              SbPlayerBenchmarkHelper::InitResult::kOk)
        << "Failed to initialize concurrent player " << k << " (" << bg_video
        << ")";
    ASSERT_TRUE(bg_helper->FeedUntilPresenting())
        << "Concurrent player " << k << " (" << bg_video
        << ") failed to reach kSbPlayerStatePresenting";
    SbPlayerSetPlaybackRate(bg_helper->GetPlayer(), 0.0);
  }

  FILE* file = std::fopen(options.csv_path.c_str(), "w");
  ASSERT_TRUE(file) << "Cannot open " << options.csv_path << ": "
                    << std::strerror(errno);
  CsvWriter csv(file);

  csv.WriteLine("# start_time: " + FormatLocalTime(WallClock::now()));
  csv.WriteLine("# video: " + options.video_spec);
  csv.WriteLine("# audio: " +
                (audio_file ? options.audio_file : std::string("none")));
  csv.WriteLine("# output_mode: " + options.output_mode_name);
  csv.WriteLine("# iterations: " + std::to_string(options.iterations));
  csv.WriteLine("# requested_gap_ms: " + std::to_string(options.gap_ms));
  csv.WriteLine("# seeks_per_iteration: " + std::to_string(options.seeks));
  csv.WriteLine("# concurrent_players: " +
                std::to_string(options.concurrent_players));
  csv.WriteLine("# experimental_features: " + features);
  csv.WriteLine("# hang_timeout_ms: " +
                std::to_string(options.hang_timeout_ms));
  csv.WriteLine(
      "# gap_ms is measured from the end of the previous iteration's teardown "
      "to this SbPlayerCreate() call.");
  csv.WriteLine(
      "iteration,start_time,start_epoch_ms,video,gap_ms,create_ms,"
      "create_to_initialized_ms,create_to_presenting_ms,seeks_completed,"
      "seek_median_ms,seek_max_ms,destroy_ms,status,error_code,error_message");

  Watchdog watchdog(
      std::chrono::milliseconds(options.hang_timeout_ms),
      [&csv, &options](const IterationResult& result, const char* step) {
        SB_LOG(ERROR) << "Endurance iteration " << result.iteration << ": "
                      << step << " has not returned after "
                      << options.hang_timeout_ms << " ms";
        IterationResult hung_result = result;
        hung_result.status = std::string(step) + "_hung";
        csv.WriteRow(hung_result);
      });

  std::optional<Clock::time_point> last_teardown_end;
  int failed_iterations = 0;

  for (int i = 0; i < options.iterations; ++i) {
    if (i > 0 && options.gap_ms > 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(options.gap_ms));
    }

    const std::string& video_file =
        options.video_files[i % options.video_files.size()];
    IterationResult result;
    result.iteration = i;
    result.start_time = WallClock::now();
    result.video = video_file;
    std::optional<std::string> hung_step;

    SbPlayerBenchmarkHelper::InitializeTimes times;
    watchdog.Begin(result, "create");
    const SbPlayerBenchmarkHelper::InitResult init_result =
        helper.InitializePlayer(audio_file, video_file.c_str(),
                                options.output_mode, &times);
    if (watchdog.End()) {
      hung_step = "create";
    }

    if (last_teardown_end && times.create_start != Clock::time_point()) {
      result.gap = times.create_start - *last_teardown_end;
    }
    if (times.created) {
      result.create = *times.created - times.create_start;
    }
    if (times.initialized) {
      result.create_to_initialized = *times.initialized - times.create_start;
    }

    if (init_result == SbPlayerBenchmarkHelper::InitResult::kOk) {
      Clock::time_point presenting_time;
      watchdog.Begin(result, "presenting");
      const bool presenting = helper.FeedUntilPresenting(&presenting_time);
      if (watchdog.End() && !hung_step) {
        hung_step = "presenting";
      }
      if (presenting) {
        result.create_to_presenting = presenting_time - times.create_start;
        if (!hung_step && options.seeks > 0) {
          const int64_t duration_us = helper.GetDurationUs();
          std::vector<Clock::duration> seek_durations;
          seek_durations.reserve(options.seeks);
          for (int s = 0; s < options.seeks; ++s) {
            const int64_t seek_target_us = ChooseSeekTargetUs(duration_us, s);
            watchdog.Begin(result, "seek");
            const int ticket = helper.PrepareSeek(seek_target_us);
            const Clock::time_point seek_start = Clock::now();
            SbPlayerSeek(helper.GetPlayer(), seek_target_us, ticket);
            Clock::time_point seek_presenting_time;
            const bool seek_ok =
                helper.FeedUntilPresenting(&seek_presenting_time);
            const bool seek_hung = watchdog.End();
            if (seek_hung && !hung_step) {
              hung_step = "seek";
            }
            if (!seek_ok) {
              result.status = "seek_failed";
              break;
            }
            seek_durations.push_back(seek_presenting_time - seek_start);
            UpdateSeekStats(seek_durations, &result);
            if (seek_hung) {
              break;
            }
          }
        }
      } else {
        result.status = "presenting_failed";
      }
    } else {
      if (init_result ==
          SbPlayerBenchmarkHelper::InitResult::kOutputModeNotSupported) {
        result.status = "output_mode_not_supported";
      } else if (times.create_start == Clock::time_point()) {
        result.status = "load_failed";
      } else if (!times.created) {
        result.status = "create_failed";
      } else {
        result.status = "initialize_failed";
      }
    }

    if (SbPlayerIsValid(helper.GetPlayer())) {
      watchdog.Begin(result, "destroy");
      const Clock::time_point destroy_start = Clock::now();
      helper.DestroyPlayer();
      last_teardown_end = Clock::now();
      result.destroy = *last_teardown_end - destroy_start;
      if (watchdog.End() && !hung_step) {
        hung_step = "destroy";
      }
    } else {
      last_teardown_end = Clock::now();
    }

    result.error = helper.GetPlayerError();
    if (result.error && result.status == "ok") {
      result.status = "error";
    }
    if (hung_step) {
      result.status = *hung_step + "_hung";
    }
    if (result.status != "ok") {
      ++failed_iterations;
    }

    csv.WriteRow(result);
    SB_LOG(INFO) << "Endurance iteration " << i << " (" << video_file
                 << "): " << result.status;

    if (hung_step) {
      ADD_FAILURE() << "Iteration " << i << ": " << *hung_step
                    << " took longer than " << options.hang_timeout_ms
                    << " ms, stopping";
      break;
    }
  }

  for (int k = 0; k < options.concurrent_players; ++k) {
    if (const auto bg_error = concurrent_helpers[k]->GetPlayerError()) {
      ADD_FAILURE() << "Concurrent player " << k << " reported error "
                    << bg_error->error << ": " << bg_error->message;
    }
    concurrent_helpers[k]->DestroyPlayer();
  }

  EXPECT_EQ(failed_iterations, 0)
      << failed_iterations << " of " << options.iterations
      << " iterations failed, see " << options.csv_path;
}

}  // namespace
}  // namespace starboard
