/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_PARSER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_PARSER_H_

<<<<<<< HEAD
#include <cstdint>
=======
#include <vector>
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#include "perfetto/protozero/field.h"
#include "src/trace_processor/storage/trace_storage.h"

<<<<<<< HEAD
namespace perfetto::trace_processor {

class ArgsTracker;
=======
namespace perfetto {
namespace trace_processor {

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
class TraceProcessorContext;

class AndroidProbesParser {
 public:
  using ConstBytes = protozero::ConstBytes;

  explicit AndroidProbesParser(TraceProcessorContext*);

  void ParseBatteryCounters(int64_t ts, ConstBytes);
<<<<<<< HEAD
  void ParsePowerRails(int64_t ts, uint64_t trace_packet_ts, ConstBytes);
=======
  void ParsePowerRails(int64_t ts, ConstBytes);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  void ParseEnergyBreakdown(int64_t ts, ConstBytes);
  void ParseEntityStateResidency(int64_t ts, ConstBytes);
  void ParseAndroidLogPacket(ConstBytes);
  void ParseAndroidLogEvent(ConstBytes);
  void ParseAndroidLogStats(ConstBytes);
  void ParseStatsdMetadata(ConstBytes);
  void ParseInitialDisplayState(int64_t ts, ConstBytes);
  void ParseAndroidSystemProperty(int64_t ts, ConstBytes);
  void ParseAndroidGameIntervention(ConstBytes);
<<<<<<< HEAD
  void ParseBtTraceEvent(int64_t ts, ConstBytes);

 private:
  TraceProcessorContext* const context_;
  std::unique_ptr<ArgsTracker> power_rails_args_tracker_;

  const StringId battery_status_id_;
  const StringId plug_type_id_;
  const StringId rail_packet_timestamp_id_;
  const StringId energy_consumer_id_;
  const StringId consumer_type_id_;
  const StringId ordinal_id_;
  const StringId bt_trace_event_id_;
  const StringId bt_packet_type_id_;
  const StringId bt_count_id_;
  const StringId bt_length_id_;
  const StringId bt_op_code_id_;
  const StringId bt_event_code_id_;
  const StringId bt_subevent_code_id_;
  const StringId bt_handle_id_;
};
}  // namespace perfetto::trace_processor
=======

 private:
  TraceProcessorContext* const context_;

  const StringId batt_charge_id_;
  const StringId batt_capacity_id_;
  const StringId batt_current_id_;
  const StringId batt_current_avg_id_;
  const StringId screen_state_id_;
  const StringId device_state_id_;
  const StringId battery_status_id_;
  const StringId plug_type_id_;
};
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_PROTO_ANDROID_PROBES_PARSER_H_
