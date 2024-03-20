#ifndef BASE_OPTIONAL_H_
#define BASE_OPTIONAL_H_

#ifndef USE_HACKY_COBALT_CHANGES
#error "Remove stubs"
#endif

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
template <typename T>
using Optional = absl::optional<T>;

constexpr auto nullopt = absl::nullopt;
}

#endif  // BASE_OPTIONAL_H_
