#ifndef Cpp17OnCpp14_DEFINED
#define Cpp17OnCpp14_DEFINED

#ifdef SKIA_STRUCTURED_BINDINGS_BACKPORT
#include <tuple>
#include <type_traits>
#include <utility>

// Helpers for converting types to tuples.
template <typename T>
struct CoercerToTuple;

template <typename... Types>
struct CoercerToTuple<std::tuple<Types...>> {
  static std::tuple<Types...>&& Coerce(std::tuple<Types...>&& tuple) {
    return std::move(tuple);
  }
  static std::tuple<Types...> Coerce(std::tuple<Types...>& tuple) {
    return tuple;
  }
};

template <typename T1, typename T2>
struct CoercerToTuple<std::pair<T1, T2>> {
  static auto Coerce(std::pair<T1, T2>&& t) {
    return std::make_tuple(t.first, t.second);
  }
  static auto Coerce(const std::pair<T1, T2>& t) {
    return std::make_tuple(t.first, t.second);
  }
};

template <typename T>
auto CoerceToTuple(T&& t) {
  return CoercerToTuple<T>::Coerce(std::forward<T>(t));
}

template <typename T>
auto CoerceToTuple(T& t) {
  return CoercerToTuple<T>::Coerce(t);
}

template <typename T>
auto CoerceToTuple(const T& t) {
  return CoercerToTuple<T>::Coerce(t);
}

// Macro.

#define STRUCTURED_BINDING_2(v0, v1, maybe_tuple)  \
  auto tuple##v0##v1 = CoerceToTuple(maybe_tuple); \
  auto& v0 = std::get<0>(tuple##v0##v1);           \
  auto& v1 = std::get<1>(tuple##v0##v1)

#define STRUCTURED_BINDING_3(v0, v1, v2, maybe_tuple)  \
  auto tuple##v0##v1##v2 = CoerceToTuple(maybe_tuple); \
  auto& v0 = std::get<0>(tuple##v0##v1##v2);           \
  auto& v1 = std::get<1>(tuple##v0##v1##v2);           \
  auto& v2 = std::get<2>(tuple##v0##v1##v2)

#define STRUCTURED_BINDING_4(v0, v1, v2, v3, maybe_tuple)  \
  auto tuple##v0##v1##v2##v3 = CoerceToTuple(maybe_tuple); \
  auto& v0 = std::get<0>(tuple##v0##v1##v2##v3);           \
  auto& v1 = std::get<1>(tuple##v0##v1##v2##v3);           \
  auto& v2 = std::get<2>(tuple##v0##v1##v2##v3);           \
  auto& v3 = std::get<3>(tuple##v0##v1##v2##v3)

#define STRUCTURED_BINDING_5(v0, v1, v2, v3, v4, maybe_tuple)  \
  auto tuple##v0##v1##v2##v3##v4 = CoerceToTuple(maybe_tuple); \
  auto& v0 = std::get<0>(tuple##v0##v1##v2##v3##v4);           \
  auto& v1 = std::get<1>(tuple##v0##v1##v2##v3##v4);           \
  auto& v2 = std::get<2>(tuple##v0##v1##v2##v3##v4);           \
  auto& v3 = std::get<3>(tuple##v0##v1##v2##v3##v4);           \
  auto& v4 = std::get<4>(tuple##v0##v1##v2##v3##v4)

#endif  // SKIA_STRUCTURED_BINDINGS_BACKPORT

#endif  // Cpp17OnCpp14_DEFINED
