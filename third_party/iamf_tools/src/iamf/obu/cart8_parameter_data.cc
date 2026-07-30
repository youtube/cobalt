#include "iamf/obu/cart8_parameter_data.h"

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

absl::Status Cart8ParameterData::ReadAndValidate(ReadBitBuffer& rb) {
  return absl::UnimplementedError("ReadAndValidate is not implemented yet.");
}

absl::Status Cart8ParameterData::Write(WriteBitBuffer& wb) const {
  return absl::UnimplementedError("Write is not implemented yet.");
}

void Cart8ParameterData::Print() const {
  ABSL_LOG(INFO) << "Cart8ParameterData printing is not implemented yet:";
}

}  // namespace iamf_tools
