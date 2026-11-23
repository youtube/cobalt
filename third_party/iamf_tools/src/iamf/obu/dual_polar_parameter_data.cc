#include "iamf/obu/dual_polar_parameter_data.h"

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

absl::Status DualPolarParameterData::ReadAndValidate(ReadBitBuffer& rb) {
  return absl::UnimplementedError("ReadAndValidate is not implemented yet.");
}

absl::Status DualPolarParameterData::Write(WriteBitBuffer& wb) const {
  return absl::UnimplementedError("Write is not implemented yet.");
}

void DualPolarParameterData::Print() const {
  ABSL_LOG(INFO) << "PolarParameterData printing is not implemented yet:";
}

}  // namespace iamf_tools