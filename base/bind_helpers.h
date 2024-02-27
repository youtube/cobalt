#ifndef BASE_BIND_HELPERS_
#define BASE_BIND_HELPERS_

#ifndef USE_HACKY_COBALT_CHANGES
#error "TODO: Remove these"
#endif

#include "base/functional/callback_helpers.h"

namespace base {
class BASE_EXPORT DoNothing {
 public:
  template <typename... Args>
  operator RepeatingCallback<void(Args...)>() const {
    return Repeatedly<Args...>();
  }
  template <typename... Args>
  operator OnceCallback<void(Args...)>() const {
    return Once<Args...>();
  }
  // Explicit way of specifying a specific callback type when the compiler can't
  // deduce it.
  template <typename... Args>
  static RepeatingCallback<void(Args...)> Repeatedly() {
    return BindRepeating([](Args... /*args*/) {});
  }
  template <typename... Args>
  static OnceCallback<void(Args...)> Once() {
    return BindOnce([](Args... /*args*/) {});
  }
};
}

#endif 
