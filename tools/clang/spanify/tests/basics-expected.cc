// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdlib.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"

class A {
 public:
  A() = default;
  // Expected rewrite:
  // A(base::span<int> ptr) : member(ptr) {}
  A(base::span<int> ptr) : member(ptr) {}

  // Expected rewrite:
  // void set(base::span<int> ptr);
  void set(base::span<int> ptr);

  // Expected rewrite:
  // base::span<int> get(int i);
  base::span<int> get(int i);

 private:
  // Expected rewrite:
  // base::span<int> member;
  base::span<int> member;
};

// Expected rewrite:
// void set(base::span<int> ptr)
void A::set(base::span<int> ptr) {
  member = ptr;
}

// Expected rewrite:
// base::span<int> get(int i)
base::span<int> A::get(int i) {
  return member;
}

int something_else() {
  // Expected rewrite:
  // base::span<int> ptr = {};
  base::span<int> ptr = {};
  A a(ptr);
  // Expected rewrite:
  // base::span<int> temp = a.get(1);
  base::span<int> temp = a.get(1);

  // Expected rewrite:
  //  int* temp2 = a.get(1234).data();
  int* temp2 = a.get(1234).data();

  // Leads temp's type to be rewritten along with other necessary changes.
  return temp[1];
}

int offset(int n = 0) {
  return n;
}

void fct() {
  auto buf = std::vector<char>(1, 1);
  // Expected rewrite:
  // base::span<char> expected_data = buf;
  base::span<char> expected_data = buf;
  for (std::size_t ii = 0; ii < 1; ++ii) {
    // buffer usage on expected_data
    expected_data[ii] = ii * 3;
  }
  int index = buf.size() - 1;
  // Expected rewrite:
  // char* temp = expected_data++.data();
  char* temp = (expected_data++).data();
  // Expected rewrite:
  // temp = (++expected_data).data();
  temp = (++expected_data).data();
  // Expected rewrite:
  // temp = (expected_data + 1).data();
  temp = (expected_data + 1).data();
  // Expected rewrite:
  // temp = (expected_data + index).data();
  temp = (expected_data + index).data();
  // Expected rewrite:
  // temp = (expected_data + offset()).data();
  temp = (expected_data + offset()).data();
  // Expected rewrite:
  // temp = (expected_data + offset(2)).data();
  temp = (expected_data + offset(2)).data();
  // Expected rewrite:
  // temp = (expected_data + offset(index)).data();
  temp = (expected_data + offset(index)).data();

  // Expected rewrite:
  // base::span<char> ptr = &buf[0];
  base::span<char> ptr = &buf[0];
  ptr[1] = 'x';
}

// cast expressions
namespace cast_exprs {
// Expected rewrite:
// void modify(base::span<int> buf)
void modify(base::span<int> buf) {
  // expected rewrite:
  // base::span<char> temp = reinterpret_cast<char*>(buf);
  base::span<char> temp = reinterpret_cast<char*>(buf);
  temp[2] = 'c';
}

void fct() {
  int buf[10];

  // Expected rewrite:
  // base::span<char> as_char;
  base::span<char> as_char = reinterpret_cast<char*>(buf);

  as_char[4] = 'c';

  // Expected rewrite:
  // base::span<int> ptr;
  base::span<int> ptr = buf;

  // Usage as a buffer, ptr should become a span.
  ptr[2] = 3;

  modify(ptr);
}

}  // namespace cast_exprs

namespace malloc_tests {

const char* get() {
  return nullptr;
}

void fct() {
  // Expected rewrite:
  // base::span<int> buf = malloc(4*sizeof(int));
  base::span<char> buf = (char*)malloc(4 * sizeof(int));
  // Leads buf to be rewritten.
  buf++;

  const char* buf2 = nullptr;
  (void)buf2[0];
  (void)get()[0];
}
}  // namespace malloc_tests

namespace function_params_and_return {
// TODO: Wrong rewrite generated here.
// Rewrites to: const base::span<int>
// Should rewrite to: base::span<const int>
const base::span<int> get_buf();

// TODO: Wrong rewrite generated here.
// Expected rewrite:
// base::span<const int> get_buf() {
const base::span<int> get_buf() {
  static std::vector<int> buf;
  return &buf[0];
}

void f() {
  // Expected rewrite:
  // base::span<const int> buf = get_buf();
  base::span<const int> buf = get_buf();
  (void)buf[0];
}
}  // namespace function_params_and_return

namespace templated_stuff {
// Expected rewrite:
// base::span<T> get_buffer()
template <typename T>
base::span<T> get_buffer() {
  static T buf[10];
  return buf;
}

void fct() {
  // Expected rewrite:
  // base::span<int> buf = get_buffer<int>();
  base::span<int> buf = get_buffer<int>();
  buf[1] = 1;

  // Expected rewrite:
  // base::span<char> buf2 = get_buffer<char>();
  base::span<char> buf2 = get_buffer<char>();
  buf2[1] = 'a';

  // Expected rewrite:
  // base::span<unsigned short> buf2 = get_buffer<uint16_t>();
  base::span<unsigned short> buf3 = get_buffer<uint16_t>();
  buf3[1] = 256;

  // Expected rewrite:
  // memcpy(buf2.data(), buf.data(), 10
  memcpy(buf2.data(), buf.data(), 10);

  // Expected rewrite:
  // memcpy((buf2++).data(), (++buf).data(), 10)
  memcpy((buf2++).data(), (++buf).data(), 10);

  int index = 11;
  // Expected rewrite:
  // memcpy((buf2 + 1).data(), (buf + index).data(), 10)
  memcpy((buf2 + 1).data(), (buf + index).data(), 10);

  // Expected rewrite:
  // int i = (buf++)[0];
  int i = (buf++)[0];
  // i = (++buf)[0]
  i = (++buf)[0];
  // i = ((buf + index)[0])
  i = ((buf + index)[0]);
  // i = buf[0];
  i = buf[0];
}

}  // namespace templated_stuff

namespace buffers_into_arrays {
void fct() {
  // Expected rewrite:
  // auto buf = std::to_array<int>({1, 2, 3, 4});
  auto buf = std::to_array<int>({1, 2, 3, 4});
  int index = 0;
  buf[index] = 11;
}
}  // namespace buffers_into_arrays

namespace namespace_stuff {
namespace ns1 {
struct B {
  int b = 0;
};
}  // namespace ns1

void fct() {
  // Expected rewrite:
  // base::span<ns1::B> ptr = new ns1::B[32];
  base::span<ns1::B> buf = new ns1::B[32];
  // However since there is no viable conversion from `B*` into
  // `base::span<ns1::B>`, the rewrite will cause compile error.
  buf[0].b = -1;
}
}  // namespace namespace_stuff
