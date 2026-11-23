#include "iamf/cli/codec/opus_utils.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "include/opus_defines.h"

namespace iamf_tools {

absl::Status OpusErrorCodeToAbslStatus(int opus_error_code,
                                       absl::string_view error_message) {
  absl::StatusCode status_code;
  switch (opus_error_code) {
    case OPUS_OK:
      return absl::OkStatus();
    case OPUS_BAD_ARG:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case OPUS_BUFFER_TOO_SMALL:
    case OPUS_INVALID_STATE:
      status_code = absl::StatusCode::kFailedPrecondition;
      break;
    case OPUS_INTERNAL_ERROR:
      status_code = absl::StatusCode::kInternal;
      break;
    case OPUS_INVALID_PACKET:
      status_code = absl::StatusCode::kDataLoss;
      break;
    case OPUS_UNIMPLEMENTED:
      status_code = absl::StatusCode::kUnimplemented;
      break;
    case OPUS_ALLOC_FAIL:
      status_code = absl::StatusCode::kResourceExhausted;
      break;
    default:
      status_code = absl::StatusCode::kUnknown;
      break;
  }
  return absl::Status(
      status_code,
      absl::StrCat(error_message, " opus_error_code= ", opus_error_code));
}

}  // namespace iamf_tools
