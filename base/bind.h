#ifndef BASE_BIND_H_
#define BASE_BIND_H_

#ifndef COBALT_PENDING_CLEAN_UP
#error "TODO: Remove these"
#endif

#include <utility>

#include "base/functional/bind.h"

namespace base {
template <typename Functor, typename... Args>
inline auto Bind(Functor&& functor, Args&&... args)
    -> decltype(BindRepeating(std::forward<Functor>(functor), std::forward<Args>(args)...)) {
    return BindRepeating(std::forward<Functor>(functor), std::forward<Args>(args)...);
}
}

#endif
