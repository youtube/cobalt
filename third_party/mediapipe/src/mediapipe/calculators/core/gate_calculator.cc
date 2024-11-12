// Copyright 2019-2020 The MediaPipe Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "mediapipe/calculators/core/gate_calculator.pb.h"
#include "mediapipe/framework/calculator_framework.h"
#include "mediapipe/framework/port/ret_check.h"
#include "mediapipe/framework/port/status.h"
#include "mediapipe/util/header_util.h"

namespace mediapipe {

namespace {

constexpr char kStateChangeTag[] = "STATE_CHANGE";
constexpr char kDisallowTag[] = "DISALLOW";
constexpr char kAllowTag[] = "ALLOW";

std::string ToString(GateCalculatorOptions::GateState state) {
  switch (state) {
    case GateCalculatorOptions::UNSPECIFIED:
      return "UNSPECIFIED";
    case GateCalculatorOptions::GATE_UNINITIALIZED:
      return "UNINITIALIZED";
    case GateCalculatorOptions::GATE_ALLOW:
      return "ALLOW";
    case GateCalculatorOptions::GATE_DISALLOW:
      return "DISALLOW";
  }
  DLOG(FATAL) << "Unknown GateState";
  return "UNKNOWN";
}
}  // namespace

// Controls whether or not the input packets are passed further along the graph.
// Takes multiple data input streams and either an ALLOW or a DISALLOW control
// input stream. It outputs an output stream for each input stream that is not
// ALLOW or DISALLOW as well as an optional STATE_CHANGE stream which downstream
// calculators can use to respond to state-change events.
//
// If the current ALLOW packet is set to true, the input packets are passed to
// their corresponding output stream unchanged. If the ALLOW packet is set to
// false, the current input packet is NOT passed to the output stream. If using
// DISALLOW, the behavior is opposite of ALLOW.
//
// By default, an empty packet in the ALLOW or DISALLOW input stream indicates
// disallowing the corresponding packets in other input streams. The behavior
// can be inverted with a calculator option.
//
// ALLOW or DISALLOW can also be specified as an input side packet. The rules
// for evaluation remain the same as above.
//
// ALLOW/DISALLOW inputs must be specified either using input stream or via
// input side packet but not both. If neither is specified, the behavior is then
// determined by the "allow" field in the calculator options.
//
// Intended to be used with the default input stream handler, which synchronizes
// all data input streams with the ALLOW/DISALLOW control input stream.
//
// Example config:
// node {
//   calculator: "GateCalculator"
//   input_side_packet: "ALLOW:allow" or "DISALLOW:disallow"
//   input_stream: "input_stream0"
//   input_stream: "input_stream1"
//   input_stream: "input_streamN"
//   input_stream: "ALLOW:allow" or "DISALLOW:disallow"
//   output_stream: "STATE_CHANGE:state_change"
//   output_stream: "output_stream0"
//   output_stream: "output_stream1"
//   output_stream: "output_streamN"
// }
class GateCalculator : public CalculatorBase {
 public:
  GateCalculator() {}

  static absl::Status CheckAndInitAllowDisallowInputs(CalculatorContract* cc) {
    bool input_via_side_packet = cc->InputSidePackets().HasTag(kAllowTag) ||
                                 cc->InputSidePackets().HasTag(kDisallowTag);
    bool input_via_stream =
        cc->Inputs().HasTag(kAllowTag) || cc->Inputs().HasTag(kDisallowTag);

    // Only one of input_side_packet or input_stream may specify
    // ALLOW/DISALLOW input.
    if (input_via_side_packet) {
      RET_CHECK(!input_via_stream);
      RET_CHECK(cc->InputSidePackets().HasTag(kAllowTag) ^
                cc->InputSidePackets().HasTag(kDisallowTag));

      if (cc->InputSidePackets().HasTag(kAllowTag)) {
        cc->InputSidePackets().Tag(kAllowTag).Set<bool>().Optional();
      } else {
        cc->InputSidePackets().Tag(kDisallowTag).Set<bool>().Optional();
      }
    }
    if (input_via_stream) {
      RET_CHECK(!input_via_side_packet);
      RET_CHECK(cc->Inputs().HasTag(kAllowTag) ^
                cc->Inputs().HasTag(kDisallowTag));

      if (cc->Inputs().HasTag(kAllowTag)) {
        cc->Inputs().Tag(kAllowTag).Set<bool>();
      } else {
        cc->Inputs().Tag(kDisallowTag).Set<bool>();
      }
    }
    return absl::OkStatus();
  }

  static absl::Status GetContract(CalculatorContract* cc) {
    RET_CHECK_OK(CheckAndInitAllowDisallowInputs(cc));

    const int num_data_streams = cc->Inputs().NumEntries("");
    RET_CHECK_EQ(cc->Outputs().NumEntries(""), num_data_streams)
        << "Number of data output streams must match with data input streams.";

    for (int i = 0; i < num_data_streams; ++i) {
      cc->Inputs().Get("", i).SetAny();
      cc->Outputs().Get("", i).SetSameAs(&cc->Inputs().Get("", i));
    }

    if (cc->Outputs().HasTag(kStateChangeTag)) {
      cc->Outputs().Tag(kStateChangeTag).Set<bool>();
    }

    return absl::OkStatus();
  }

  absl::Status Open(CalculatorContext* cc) final {
    if (cc->InputSidePackets().HasTag(kAllowTag)) {
      use_side_packet_for_allow_disallow_ = true;
      allow_by_side_packet_decision_ =
          cc->InputSidePackets().Tag(kAllowTag).Get<bool>();
    } else if (cc->InputSidePackets().HasTag(kDisallowTag)) {
      use_side_packet_for_allow_disallow_ = true;
      allow_by_side_packet_decision_ =
          !cc->InputSidePackets().Tag(kDisallowTag).Get<bool>();
    }

    cc->SetOffset(TimestampDiff(0));
    num_data_streams_ = cc->Inputs().NumEntries("");

    const auto& options = cc->Options<::mediapipe::GateCalculatorOptions>();
    last_gate_state_ = options.initial_gate_state();

    RET_CHECK_OK(CopyInputHeadersToOutputs(cc->Inputs(), &cc->Outputs()));

    empty_packets_as_allow_ = options.empty_packets_as_allow();

    if (!use_side_packet_for_allow_disallow_ &&
        !cc->Inputs().HasTag(kAllowTag) && !cc->Inputs().HasTag(kDisallowTag)) {
      use_option_for_allow_disallow_ = true;
      allow_by_option_decision_ = options.allow();
    }

    return absl::OkStatus();
  }

  absl::Status Process(CalculatorContext* cc) final {
    bool allow = empty_packets_as_allow_;
    if (use_option_for_allow_disallow_) {
      allow = allow_by_option_decision_;
    } else if (use_side_packet_for_allow_disallow_) {
      allow = allow_by_side_packet_decision_;
    } else {
      if (cc->Inputs().HasTag(kAllowTag) &&
          !cc->Inputs().Tag(kAllowTag).IsEmpty()) {
        allow = cc->Inputs().Tag(kAllowTag).Get<bool>();
      }
      if (cc->Inputs().HasTag(kDisallowTag) &&
          !cc->Inputs().Tag(kDisallowTag).IsEmpty()) {
        allow = !cc->Inputs().Tag(kDisallowTag).Get<bool>();
      }
    }
    const GateCalculatorOptions::GateState new_gate_state =
        allow ? GateCalculatorOptions::GATE_ALLOW
              : GateCalculatorOptions::GATE_DISALLOW;

    if (cc->Outputs().HasTag(kStateChangeTag)) {
      if (last_gate_state_ != GateCalculatorOptions::GATE_UNINITIALIZED &&
          last_gate_state_ != new_gate_state) {
        VLOG(2) << "State transition in " << cc->NodeName() << " @ "
                << cc->InputTimestamp().Value() << " from "
                << ToString(last_gate_state_) << " to "
                << ToString(new_gate_state);
        cc->Outputs()
            .Tag(kStateChangeTag)
            .AddPacket(MakePacket<bool>(allow).At(cc->InputTimestamp()));
      }
    }
    last_gate_state_ = new_gate_state;

    if (!allow) {
      // Close the output streams if the gate will be permanently closed.
      // Prevents buffering in calculators whose parents do no use SetOffset.
      for (int i = 0; i < num_data_streams_; ++i) {
        if (!cc->Outputs().Get("", i).IsClosed() &&
            use_side_packet_for_allow_disallow_) {
          cc->Outputs().Get("", i).Close();
        }
      }
      return absl::OkStatus();
    }

    // Process data streams.
    for (int i = 0; i < num_data_streams_; ++i) {
      if (!cc->Inputs().Get("", i).IsEmpty()) {
        cc->Outputs().Get("", i).AddPacket(cc->Inputs().Get("", i).Value());
      }
    }

    return absl::OkStatus();
  }

 private:
  GateCalculatorOptions::GateState last_gate_state_ =
      GateCalculatorOptions::GATE_UNINITIALIZED;
  int num_data_streams_;
  bool empty_packets_as_allow_;
  bool use_side_packet_for_allow_disallow_ = false;
  bool allow_by_side_packet_decision_;
  bool use_option_for_allow_disallow_ = false;
  bool allow_by_option_decision_;
};
REGISTER_CALCULATOR(GateCalculator);

}  // namespace mediapipe
