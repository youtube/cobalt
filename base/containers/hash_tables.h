#ifndef BASE_CONTAINERS_HASH_MAP_H_
#define BASE_CONTAINERS_HASH_MAP_H_

#ifndef USE_HACKY_COBALT_CHANGES
#error "TODO: Remove these"
#endif

#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "starboard/configuration.h"

// namespace std {
// template <typename T>
// struct hash<T*> {
//   std::size_t operator()(T* value) const {
//     return std::hash<uintptr_t>()(
//         reinterpret_cast<uintptr_t>(value));
//   }
// };
// }  // namespace std

namespace base {
template <class K,
          class V,
          class Hash = std::hash<K>,
          class KeyEqual = std::equal_to<K>>
using hash_map = std::unordered_map<K, V, Hash, KeyEqual>;
template <class K, class Hash = std::hash<K>, class KeyEqual = std::equal_to<K>>
using hash_set = std::unordered_set<K, Hash, KeyEqual>;
}
#endif
