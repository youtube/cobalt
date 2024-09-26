// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct MySubStruct {
  MySubStruct(const int& num);
  int member;
  virtual int get_member() const { return member; }
  virtual ~MySubStruct() = default;

  struct inner_s {
    inner_s(const inner_s&);
    // Expected rewrite: const raw_ref<int> inner_int;
    int& inner_int;
  };
};

// This was added due to a bug in the matcher that was doing erroneous rewrites
// here. No rewrite expected.
MySubStruct::inner_s::inner_s(const inner_s&) = default;

struct MyStruct {
  MyStruct(MySubStruct& s) : ref(s) {}

  const MySubStruct& get() const;
  // Expected rewrite: const raw_ref<MySubStruct> ref;
  MySubStruct& ref;
};

template <class T>
struct MyTemplatedStruct {
  MyTemplatedStruct(T& t) : ref(t) {}

  // Expected rewrite: ref->member
  void setSubmember(int n) { ref.member = n; }

  // Expected rewrite: return *ref;
  T& get() { return ref; }

  const T& get() const;
  // Expected rewrite: return *ref;
  T get_by_value() const { return ref; }

  // Expected rewrite: ref->get_member();
  int getInt() { return ref.get_member(); }

  // Expected rewrite: const raw_ref<T> ref;
  T& ref;
};

template <class T>
void func(T& ref, const T& ref2) {}

int main() {
  MySubStruct sub{1};
  MyStruct s(sub);
  // Expected rewrite: s.ref->member;
  s.ref.member = 11;
  // Expected rewrite: s.ref->get_member();
  s.ref.get_member();

  // Expected rewrite: MyStruct* s2 = new MyStruct(*s.ref);
  MyStruct* s2 = new MyStruct(s.ref);

  MyTemplatedStruct<MySubStruct> my_template_inst(sub);
  my_template_inst.setSubmember(1234);

  // Expected rewrites:
  // func(*my_template_inst.ref, my_template_inst.get());
  func(my_template_inst.ref, my_template_inst.get());
  // func(my_template_inst.get(), *my_template_inst.ref);
  func(my_template_inst.get(), my_template_inst.ref);
  // func(*my_template_inst.ref, *my_template_inst.ref);
  func(my_template_inst.ref, my_template_inst.ref);

  // Expected rewrite: MySubStruct* ptr = &*s.ref;
  MySubStruct* ptr = &s.ref;

  // Expected rewrite:
  //  auto &ref = *s.ref, &ref2 = *s.ref;
  auto &ref = s.ref, &ref2 = s.ref;

  int a = 0;
  int b = 0;

  struct {
    // Expected rewrite: const raw_ref<int> in;
    int& in;
    // Expected rewrite: const raw_ref<int> out;
    int& out;
    // Expected rewrite:
    // report_lists[]{{raw_ref(a), raw_ref(b)}, {raw_ref(a), raw_ref(b)}};
  } report_lists[]{{a, b}, {a, b}};

  // Reference members of nested anonymous structs declared in a
  // function body are rewritten because they can end up on the heap.
  struct A {
    struct {
      int& i;
    } member;
  };

  // Expected rewrite:  A obj{raw_ref(a)};
  A obj{a};
  obj.member.i++;

  // Expected rewrite:  A obj{.member = {raw_ref(a)}};
  A obj2{.member = {a}};
  obj2.member.i++;

  static struct {
    // Expected rewrite: const raw_ref<int> member;
    int& member;
    // Expected rewrite: st{raw_ref(a)};
  } st{a};

  struct Temp {
    Temp(int& ref) : member(ref) {}
    // Expected rewrite: const raw_ref<int> member;
    int& member;
  };
  // No need to add raw_ref() around `a` here because the constructor will be
  // called.
  Temp tmp{a};
  tmp.member++;

  struct StringRefObj {
    const std::string& member;
  };

  // No need to add raw_ref() around "abcde" here because it results in a
  // temporary string object. A manual fix need to be applied.
  StringRefObj r{"abcde"};
  (void)r;
}

struct B {
  struct {
    // Expected rewrite: const raw_ref<int> i;
    int& i;
  } member;
};

template <typename T>
const T& MyTemplatedStruct<T>::get() const {
  // Expected rewrite: return *ref;
  return ref;
}

const MySubStruct& MyStruct::get() const {
  // Expected rewrite: return *ref;
  return ref;
}

struct key_compare {
  bool operator()(const int& a, const int& b) const { return a < b; }
};

struct KeyValueCompare {
  // The key comparison object must outlive this class.
  explicit KeyValueCompare(const key_compare& comp) : comp_(comp) {}

  bool operator()(const int& lhs, const int& rhs) const {
    // Expected rewrite: return (*comp_)(lhs, rhs);
    return comp_(lhs, rhs);
  }

  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    // Expected rewrite: return (*comp_)(lhs, rhs);
    return comp_(lhs, rhs);
  }

 private:
  const key_compare& comp_;
};

struct KeyValueCompare2 {
  // The key comparison object must outlive this class.
  explicit KeyValueCompare2(const key_compare& comp) : comp_(comp) {}

  template <typename T, typename U>
  bool operator()(const T& lhs, const U& rhs) const {
    // Expected rewrite: return (*comp_)(lhs, rhs);
    return comp_(lhs, rhs);
  }

 private:
  const key_compare& comp_;
};

template <typename T>
void doSomething(T& t) {
  (void)t;
}

template <typename T>
struct VectorMemberRef {
  void iterate() {
    for (T& t : v) {
      doSomething(t);
    }
  }

  T get_first() { return v[0]; }

  std::vector<T>& v;
};