// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_builder.h"

#include <string>
#include <utility>

#include "base/atomicops.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "components/metrics/call_stack_profile_encoding.h"

namespace metrics {

namespace {

// Only used by child processes.
base::LazyInstance<ChildCallStackProfileCollector>::Leaky
    g_child_call_stack_profile_collector = LAZY_INSTANCE_INITIALIZER;

base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
GetBrowserProcessReceiverCallbackInstance() {
  static base::NoDestructor<
      base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>>
      instance;
  return *instance;
}

// Identifies an unknown module.
const size_t kUnknownModuleIndex = static_cast<size_t>(-1);

// This global variables holds the current system state and is recorded with
// every captured sample, done on a separate thread which is why updates to
// this must be atomic. A PostTask to move the the updates to that thread
// would skew the timing and a lock could result in deadlock if the thread
// making a change was also being profiled and got stopped.
static base::subtle::Atomic32 g_process_milestones = 0;

void ChangeAtomicFlags(base::subtle::Atomic32* flags,
                       base::subtle::Atomic32 set,
                       base::subtle::Atomic32 clear) {
  DCHECK(set != 0 || clear != 0);
  DCHECK_EQ(0, set & clear);

  base::subtle::Atomic32 bits = base::subtle::NoBarrier_Load(flags);
  while (true) {
    base::subtle::Atomic32 existing = base::subtle::NoBarrier_CompareAndSwap(
        flags, bits, (bits | set) & ~clear);
    if (existing == bits)
      break;
    bits = existing;
  }
}

// Provide a mapping from the C++ "enum" definition of various process mile-
// stones to the equivalent protobuf "enum" definition. This table-lookup
// conversion allows for the implementation to evolve and still be compatible
// with the protobuf -- even if there are ever more than 32 defined proto
// values, though never more than 32 could be in-use in a given C++ version
// of the code.
const ProcessPhase kProtoPhases[CallStackProfileBuilder::MILESTONES_MAX_VALUE] =
    {
        ProcessPhase::MAIN_LOOP_START,
        ProcessPhase::MAIN_NAVIGATION_START,
        ProcessPhase::MAIN_NAVIGATION_FINISHED,
        ProcessPhase::FIRST_NONEMPTY_PAINT,

        ProcessPhase::SHUTDOWN_START,
};

// These functions are used to encode protobufs. --------------------------

// The protobuf expects the MD5 checksum prefix of the module name.
uint64_t HashModuleFilename(const base::FilePath& filename) {
  const base::FilePath::StringType basename = filename.BaseName().value();
  // Copy the bytes in basename into a string buffer.
  size_t basename_length_in_bytes =
      basename.size() * sizeof(base::FilePath::CharType);
  std::string name_bytes(basename_length_in_bytes, '\0');
  memcpy(&name_bytes[0], &basename[0], basename_length_in_bytes);
  return base::HashMetricName(name_bytes);
}

// Transcode |sample| into |proto_sample|, using base addresses in |modules| to
// compute module instruction pointer offsets.
void CopySampleToProto(const CallStackProfileBuilder::Sample& sample,
                       const std::vector<base::ModuleCache::Module>& modules,
                       CallStackProfile::Sample* proto_sample) {
  for (const auto& frame : sample.frames) {
    CallStackProfile::Location* location = proto_sample->add_frame();
    // A frame may not have a valid module. If so, we can't compute the
    // instruction pointer offset, and we don't want to send bare pointers,
    // so leave call_stack_entry empty.
    if (frame.module_index == kUnknownModuleIndex)
      continue;
    int64_t module_offset =
        reinterpret_cast<const char*>(frame.instruction_pointer) -
        reinterpret_cast<const char*>(modules[frame.module_index].base_address);
    DCHECK_GE(module_offset, 0);
    location->set_address(static_cast<uint64_t>(module_offset));
    location->set_module_id_index(frame.module_index);
  }
}

// Transcode Sample annotations into protobuf fields. The C++ code uses a
// bit- field with each bit corresponding to an entry in an enumeration
// while the protobuf uses a repeated field of individual values. Conversion
// tables allow for arbitrary mapping, though no more than 32 in any given
// version of the code.
void CopyAnnotationsToProto(uint32_t new_milestones,
                            CallStackProfile::Sample* sample_proto) {
  for (size_t bit = 0; new_milestones != 0 && bit < sizeof(new_milestones) * 8;
       ++bit) {
    const uint32_t flag = 1U << bit;
    if (new_milestones & flag) {
      if (bit >= base::size(kProtoPhases)) {
        NOTREACHED();
        continue;
      }
      sample_proto->add_process_phase(kProtoPhases[bit]);
      new_milestones ^= flag;  // Bit is set so XOR will clear it.
    }
  }
}

}  // namespace

// CallStackProfileBuilder::Frame ---------------------------------------------

CallStackProfileBuilder::Frame::Frame(uintptr_t instruction_pointer,
                                      size_t module_index)
    : instruction_pointer(instruction_pointer), module_index(module_index) {}

CallStackProfileBuilder::Frame::~Frame() = default;

CallStackProfileBuilder::Frame::Frame()
    : instruction_pointer(0), module_index(kUnknownModuleIndex) {}

// CallStackProfileBuilder::Sample --------------------------------------------

CallStackProfileBuilder::Sample::Sample() = default;

CallStackProfileBuilder::Sample::Sample(const Sample& sample) = default;

CallStackProfileBuilder::Sample::~Sample() = default;

CallStackProfileBuilder::Sample::Sample(const Frame& frame) {
  frames.push_back(std::move(frame));
}

CallStackProfileBuilder::Sample::Sample(const std::vector<Frame>& frames)
    : frames(frames) {}

CallStackProfileBuilder::CallStackProfileBuilder(
    const CallStackProfileParams& profile_params,
    base::OnceClosure completed_callback)
    : profile_params_(profile_params),
      profile_start_time_(base::TimeTicks::Now()) {
  completed_callback_ = std::move(completed_callback);
}

CallStackProfileBuilder::~CallStackProfileBuilder() = default;

void CallStackProfileBuilder::RecordAnnotations() {
  // The code inside this method must not do anything that could acquire a
  // mutex, including allocating memory (which includes LOG messages) because
  // that mutex could be held by a stopped thread, thus resulting in deadlock.
  sample_.process_milestones =
      base::subtle::NoBarrier_Load(&g_process_milestones);
}

void CallStackProfileBuilder::OnSampleCompleted(
    std::vector<base::StackSamplingProfiler::Frame> frames) {
  // Assemble sample_ from |frames| first.
  for (const auto& frame : frames) {
    const base::ModuleCache::Module& module(frame.module);
    if (!module.is_valid) {
      sample_.frames.emplace_back(frame.instruction_pointer,
                                  kUnknownModuleIndex);
      continue;
    }

    // Dedup modules and cache them in modules_.
    auto loc = module_index_.find(module.base_address);
    if (loc == module_index_.end()) {
      modules_.push_back(module);
      size_t index = modules_.size() - 1;
      loc = module_index_.insert(std::make_pair(module.base_address, index))
                .first;
    }
    sample_.frames.emplace_back(frame.instruction_pointer, loc->second);
  }

  // Write CallStackProfile::Sample protocol buffer message based on sample_.
  int existing_sample_index = -1;
  auto location = sample_index_.find(sample_);
  if (location != sample_index_.end())
    existing_sample_index = location->second;

  if (existing_sample_index != -1) {
    CallStackProfile::Sample* sample_proto =
        proto_profile_.mutable_deprecated_sample(existing_sample_index);
    sample_proto->set_count(sample_proto->count() + 1);
    return;
  }

  CallStackProfile::Sample* sample_proto =
      proto_profile_.add_deprecated_sample();
  CopySampleToProto(sample_, modules_, sample_proto);
  sample_proto->set_count(1);
  CopyAnnotationsToProto(sample_.process_milestones & ~milestones_,
                         sample_proto);
  milestones_ = sample_.process_milestones;

  sample_index_.insert(std::make_pair(
      sample_, static_cast<int>(proto_profile_.deprecated_sample_size()) - 1));

  sample_ = Sample();
}

// Build a SampledProfile in the protocol buffer message format from the
// collected sampling data. The message is then passed to
// CallStackProfileMetricsProvider or ChildCallStackProfileCollector.

// A SampledProfile message (third_party/metrics_proto/sampled_profile.proto)
// contains a CallStackProfile message
// (third_party/metrics_proto/call_stack_profile.proto) and associated profile
// parameters (process/thread/trigger event). A CallStackProfile message
// contains a set of Sample messages and ModuleIdentifier messages, and other
// sampling information. One Sample corresponds to a single recorded stack, and
// the ModuleIdentifiers record those modules associated with the recorded stack
// frames.
void CallStackProfileBuilder::OnProfileCompleted(
    base::TimeDelta profile_duration,
    base::TimeDelta sampling_period) {
  proto_profile_.set_profile_duration_ms(profile_duration.InMilliseconds());
  proto_profile_.set_sampling_period_ms(sampling_period.InMilliseconds());

  for (const auto& module : modules_) {
    CallStackProfile::ModuleIdentifier* module_id =
        proto_profile_.add_module_id();
    module_id->set_build_id(module.id);
    module_id->set_name_md5_prefix(HashModuleFilename(module.filename));
  }

  // Clear the caches etc.
  modules_.clear();
  module_index_.clear();
  sample_index_.clear();

  // Assemble the SampledProfile protocol buffer message and run the associated
  // callback to pass it.
  SampledProfile sampled_profile;
  CallStackProfile* proto_profile =
      sampled_profile.mutable_call_stack_profile();
  *proto_profile = std::move(proto_profile_);

  sampled_profile.set_process(
      ToExecutionContextProcess(profile_params_.process));
  sampled_profile.set_thread(ToExecutionContextThread(profile_params_.thread));
  sampled_profile.set_trigger_event(
      ToSampledProfileTriggerEvent(profile_params_.trigger));

  PassProfilesToMetricsProvider(std::move(sampled_profile));

  // Run the completed callback if there is one.
  if (!completed_callback_.is_null())
    std::move(completed_callback_).Run();
}

// static
void CallStackProfileBuilder::SetBrowserProcessReceiverCallback(
    const base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
        callback) {
  GetBrowserProcessReceiverCallbackInstance() = callback;
}

void CallStackProfileBuilder::PassProfilesToMetricsProvider(
    SampledProfile sampled_profile) {
  if (profile_params_.process == CallStackProfileParams::BROWSER_PROCESS) {
    GetBrowserProcessReceiverCallbackInstance().Run(profile_start_time_,
                                                    std::move(sampled_profile));
  } else {
    g_child_call_stack_profile_collector.Get()
        .ChildCallStackProfileCollector::Collect(profile_start_time_,
                                                 std::move(sampled_profile));
  }
}

// static
void CallStackProfileBuilder::SetProcessMilestone(int milestone) {
  DCHECK_LE(0, milestone);
  DCHECK_GT(static_cast<int>(sizeof(g_process_milestones) * 8), milestone);
  DCHECK_EQ(0, base::subtle::NoBarrier_Load(&g_process_milestones) &
                   (1 << milestone));
  ChangeAtomicFlags(&g_process_milestones, 1 << milestone, 0);
}

// static
void CallStackProfileBuilder::SetParentProfileCollectorForChildProcess(
    metrics::mojom::CallStackProfileCollectorPtr browser_interface) {
  g_child_call_stack_profile_collector.Get().SetParentProfileCollector(
      std::move(browser_interface));
}

// These operators permit types to be compared and used in a map of Samples.

bool operator==(const CallStackProfileBuilder::Sample& a,
                const CallStackProfileBuilder::Sample& b) {
  return a.process_milestones == b.process_milestones && a.frames == b.frames;
}

bool operator!=(const CallStackProfileBuilder::Sample& a,
                const CallStackProfileBuilder::Sample& b) {
  return !(a == b);
}

bool operator<(const CallStackProfileBuilder::Sample& a,
               const CallStackProfileBuilder::Sample& b) {
  if (a.process_milestones != b.process_milestones)
    return a.process_milestones < b.process_milestones;

  return a.frames < b.frames;
}

bool operator==(const CallStackProfileBuilder::Frame& a,
                const CallStackProfileBuilder::Frame& b) {
  return a.instruction_pointer == b.instruction_pointer &&
         a.module_index == b.module_index;
}

bool operator<(const CallStackProfileBuilder::Frame& a,
               const CallStackProfileBuilder::Frame& b) {
  if (a.module_index != b.module_index)
    return a.module_index < b.module_index;

  return a.instruction_pointer < b.instruction_pointer;
}

}  // namespace metrics
