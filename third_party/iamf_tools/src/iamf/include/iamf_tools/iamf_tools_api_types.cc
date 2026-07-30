#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

#include <ostream>
#include <string>

namespace iamf_tools {
namespace api {

IamfStatus::IamfStatus(const std::string& error_message)
    : success(false), error_message(error_message) {}

IamfStatus IamfStatus::OkStatus() { return IamfStatus(); }

IamfStatus IamfStatus::ErrorStatus(const std::string& error_message) {
  return IamfStatus(error_message);
}

std::ostream& operator<<(std::ostream& os, const IamfStatus& status) {
  if (status.ok()) {
    os << "Success\n";
  } else {
    os << "Failure: " << status.error_message << "\n";
  }
  return os;
}

}  // namespace api
}  // namespace iamf_tools
