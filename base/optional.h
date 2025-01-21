#ifndef BASE_OPTIONAL_H_
#define BASE_OPTIONAL_H_

#ifndef COBALT_PENDING_CLEAN_UP
#error "Remove stubs"
#endif

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
template <typename T>
using Optional = absl::optional<T>;

constexpr auto nullopt = absl::nullopt;
}

#endif  // BASE_OPTIONAL_H_
