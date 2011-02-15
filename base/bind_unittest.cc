// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"

#if defined(BASE_CALLBACK_H_)
// We explicitly do not want to include callback.h so people are not tempted
// to use bind.h in a headerfile for getting the Callback types.
#error "base/bind.h should avoid pulling in callback.h by default."
#endif

#include "base/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;

namespace base {
namespace {

class NoRef {
 public:
  NoRef() {}

  MOCK_METHOD0(VoidMethod0, void(void));
  MOCK_CONST_METHOD0(VoidConstMethod0, void(void));

  MOCK_METHOD0(IntMethod0, int(void));
  MOCK_CONST_METHOD0(IntConstMethod0, int(void));

 private:
  // Particularly important in this test to ensure no copies are made.
  DISALLOW_COPY_AND_ASSIGN(NoRef);
};

class HasRef : public NoRef {
 public:
  HasRef() {}

  MOCK_CONST_METHOD0(AddRef, void(void));
  MOCK_CONST_METHOD0(Release, bool(void));

 private:
  // Particularly important in this test to ensure no copies are made.
  DISALLOW_COPY_AND_ASSIGN(HasRef);
};

static const int kParentValue = 1;
static const int kChildValue = 2;

class Parent {
 public:
  void AddRef(void) const {}
  void Release(void) const {}
  virtual void VirtualSet() { value = kParentValue; }
  void NonVirtualSet() { value = kParentValue; }
  int value;
};

class Child : public Parent {
 public:
  virtual void VirtualSet() { value = kChildValue; }
  void NonVirtualSet() { value = kChildValue; }
};

class NoRefParent {
 public:
  virtual void VirtualSet() { value = kParentValue; }
  void NonVirtualSet() { value = kParentValue; }
  int value;
};

class NoRefChild : public NoRefParent {
  virtual void VirtualSet() { value = kChildValue; }
  void NonVirtualSet() { value = kChildValue; }
};

// Used for probing the number of copies that occur if a type must be coerced
// during argument forwarding in the Run() methods.
struct DerivedCopyCounter {
  DerivedCopyCounter(int* copies, int* assigns)
      : copies_(copies), assigns_(assigns) {
  }
  int* copies_;
  int* assigns_;
};

// Used for probing the number of copies in an argument.
class CopyCounter {
 public:
  CopyCounter(int* copies, int* assigns)
      : copies_(copies), assigns_(assigns) {
  }

  CopyCounter(const CopyCounter& other)
      : copies_(other.copies_),
        assigns_(other.assigns_) {
    (*copies_)++;
  }

  // Probing for copies from coerscion.
  CopyCounter(const DerivedCopyCounter& other)
      : copies_(other.copies_),
        assigns_(other.assigns_) {
    (*copies_)++;
  }

  const CopyCounter& operator=(const CopyCounter& rhs) {
    copies_ = rhs.copies_;
    assigns_ = rhs.assigns_;

    if (assigns_) {
      (*assigns_)++;
    }

    return *this;
  }

  int copies() const {
    return *copies_;
  }

  int assigns() const {
    return *assigns_;
  }

 private:
  int* copies_;
  int* assigns_;
};

// Some test functions that we can Bind to.
template <typename T>
T PolymorphicIdentity(T t) {
  return t;
}

template <typename T>
void VoidPolymorphic1(T t) {
}

int Identity(int n) {
  return n;
}

int ArrayGet(const int array[], int n) {
  return array[n];
}

int Sum(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

const char* CStringIdentity(const char* s) {
  return s;
}

int GetCopies(const CopyCounter& counter) {
  return counter.copies();
}

int UnwrapNoRefParent(NoRefParent p) {
  return p.value;
}

int UnwrapNoRefParentPtr(NoRefParent* p) {
  return p->value;
}

int UnwrapNoRefParentConstRef(const NoRefParent& p) {
  return p.value;
}

// Only useful in no-compile tests.
int UnwrapNoRefParentRef(Parent& p) {
  return p.value;
}

class BindTest : public ::testing::Test {
 public:
  BindTest() {
    const_has_ref_ptr_ = &has_ref_;
    const_no_ref_ptr_ = &no_ref_;
    static_func_mock_ptr = &static_func_mock_;
  }

  virtual ~BindTest() {
  }

  static void VoidFunc0(void) {
    static_func_mock_ptr->VoidMethod0();
  }

  static int IntFunc0(void) { return static_func_mock_ptr->IntMethod0(); }

 protected:
  StrictMock<NoRef> no_ref_;
  StrictMock<HasRef> has_ref_;
  const HasRef* const_has_ref_ptr_;
  const NoRef* const_no_ref_ptr_;
  StrictMock<NoRef> static_func_mock_;

  // Used by the static functions to perform expectations.
  static StrictMock<NoRef>* static_func_mock_ptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(BindTest);
};

StrictMock<NoRef>* BindTest::static_func_mock_ptr;

// Ensure we can create unbound callbacks. We need this to be able to store
// them in class members that can be initialized later.
TEST_F(BindTest, DefaultConstruction) {
  Callback<void(void)> c0;
  Callback<void(int)> c1;
  Callback<void(int,int)> c2;
  Callback<void(int,int,int)> c3;
  Callback<void(int,int,int,int)> c4;
  Callback<void(int,int,int,int,int)> c5;
  Callback<void(int,int,int,int,int,int)> c6;
}

// Sanity check that we can instantiate a callback for each arity.
TEST_F(BindTest, ArityTest) {
  Callback<int(void)> c0 = Bind(&Sum, 32, 16, 8, 4, 2, 1);
  EXPECT_EQ(63, c0.Run());

  Callback<int(int)> c1 = Bind(&Sum, 32, 16, 8, 4, 2);
  EXPECT_EQ(75, c1.Run(13));

  Callback<int(int,int)> c2 = Bind(&Sum, 32, 16, 8, 4);
  EXPECT_EQ(85, c2.Run(13, 12));

  Callback<int(int,int,int)> c3 = Bind(&Sum, 32, 16, 8);
  EXPECT_EQ(92, c3.Run(13, 12, 11));

  Callback<int(int,int,int,int)> c4 = Bind(&Sum, 32, 16);
  EXPECT_EQ(94, c4.Run(13, 12, 11, 10));

  Callback<int(int,int,int,int,int)> c5 = Bind(&Sum, 32);
  EXPECT_EQ(87, c5.Run(13, 12, 11, 10, 9));

  Callback<int(int,int,int,int,int,int)> c6 = Bind(&Sum);
  EXPECT_EQ(69, c6.Run(13, 12, 11, 10, 9, 14));
}

// Function type support.
//   - Normal function.
//   - Method bound to non-const object.
//   - Const method bound to non-const object.
//   - Const method bound to const object.
//   - Derived classes can be used with pointers to non-virtual base functions.
//   - Derived classes can be used with pointers to virtual base functions (and
//     preserve virtual dispatch).
TEST_F(BindTest, FunctionTypeSupport) {
  EXPECT_CALL(static_func_mock_, VoidMethod0());
  EXPECT_CALL(has_ref_, AddRef()).Times(3);
  EXPECT_CALL(has_ref_, Release()).Times(3);
  EXPECT_CALL(has_ref_, VoidMethod0());
  EXPECT_CALL(has_ref_, VoidConstMethod0()).Times(2);

  Closure normal_cb = Bind(&VoidFunc0);
  Closure method_cb = Bind(&HasRef::VoidMethod0, &has_ref_);
  Closure const_method_nonconst_obj_cb = Bind(&HasRef::VoidConstMethod0,
                                              &has_ref_);
  Closure const_method_const_obj_cb = Bind(&HasRef::VoidConstMethod0,
                                           const_has_ref_ptr_);
  normal_cb.Run();
  method_cb.Run();
  const_method_nonconst_obj_cb.Run();
  const_method_const_obj_cb.Run();

  Child child;
  child.value = 0;
  Closure virtual_set_cb = Bind(&Parent::VirtualSet, &child);
  virtual_set_cb.Run();
  EXPECT_EQ(kChildValue, child.value);

  child.value = 0;
  Closure non_virtual_set_cb = Bind(&Parent::NonVirtualSet, &child);
  non_virtual_set_cb.Run();
  EXPECT_EQ(kParentValue, child.value);
}

// Return value support.
//   - Function with return value.
//   - Method with return value.
//   - Const method with return value.
TEST_F(BindTest, ReturnValues) {
  EXPECT_CALL(static_func_mock_, IntMethod0()).WillOnce(Return(1337));
  EXPECT_CALL(has_ref_, AddRef()).Times(3);
  EXPECT_CALL(has_ref_, Release()).Times(3);
  EXPECT_CALL(has_ref_, IntMethod0()).WillOnce(Return(31337));
  EXPECT_CALL(has_ref_, IntConstMethod0())
      .WillOnce(Return(41337))
      .WillOnce(Return(51337));

  Callback<int(void)> normal_cb = Bind(&IntFunc0);
  Callback<int(void)> method_cb = Bind(&HasRef::IntMethod0, &has_ref_);
  Callback<int(void)> const_method_nonconst_obj_cb =
      Bind(&HasRef::IntConstMethod0, &has_ref_);
  Callback<int(void)> const_method_const_obj_cb =
      Bind(&HasRef::IntConstMethod0, const_has_ref_ptr_);
  EXPECT_EQ(1337, normal_cb.Run());
  EXPECT_EQ(31337, method_cb.Run());
  EXPECT_EQ(41337, const_method_nonconst_obj_cb.Run());
  EXPECT_EQ(51337, const_method_const_obj_cb.Run());
}

// Argument binding tests.
//   - Argument binding to primitive.
//   - Argument binding to primitive pointer.
//   - Argument binding to a literal integer.
//   - Argument binding to a literal string.
//   - Argument binding with template function.
//   - Argument binding to an object.
//   - Argument gets type converted.
//   - Pointer argument gets converted.
//   - Const Reference forces conversion.
TEST_F(BindTest, ArgumentBinding) {
  int n = 2;

  Callback<int(void)> bind_primitive_cb = Bind(&Identity, n);
  EXPECT_EQ(n, bind_primitive_cb.Run());

  Callback<int*(void)> bind_primitive_pointer_cb =
      Bind(&PolymorphicIdentity<int*>, &n);
  EXPECT_EQ(&n, bind_primitive_pointer_cb.Run());

  Callback<int(void)> bind_int_literal_cb = Bind(&Identity, 3);
  EXPECT_EQ(3, bind_int_literal_cb.Run());

  Callback<const char*(void)> bind_string_literal_cb =
      Bind(&CStringIdentity, "hi");
  EXPECT_STREQ("hi", bind_string_literal_cb.Run());

  Callback<int(void)> bind_template_function_cb =
      Bind(&PolymorphicIdentity<int>, 4);
  EXPECT_EQ(4, bind_template_function_cb.Run());

  NoRefParent p;
  p.value = 5;
  Callback<int(void)> bind_object_cb = Bind(&UnwrapNoRefParent, p);
  EXPECT_EQ(5, bind_object_cb.Run());

  NoRefChild c;
  c.value = 6;
  Callback<int(void)> bind_promotes_cb = Bind(&UnwrapNoRefParent, c);
  EXPECT_EQ(6, bind_promotes_cb.Run());

  c.value = 7;
  Callback<int(void)> bind_pointer_promotes_cb =
      Bind(&UnwrapNoRefParentPtr, &c);
  EXPECT_EQ(7, bind_pointer_promotes_cb.Run());

  c.value = 8;
  Callback<int(void)> bind_const_reference_promotes_cb =
      Bind(&UnwrapNoRefParentConstRef, c);
  EXPECT_EQ(8, bind_const_reference_promotes_cb.Run());
}

// Functions that take reference parameters.
//  - Forced reference parameter type still stores a copy.
//  - Forced const reference parameter type still stores a copy.
TEST_F(BindTest, ReferenceArgumentBinding) {
  int n = 1;
  int& ref_n = n;
  const int& const_ref_n = n;

  Callback<int(void)> ref_copies_cb = Bind(&Identity, ref_n);
  EXPECT_EQ(n, ref_copies_cb.Run());
  n++;
  EXPECT_EQ(n - 1, ref_copies_cb.Run());

  Callback<int(void)> const_ref_copies_cb = Bind(&Identity, const_ref_n);
  EXPECT_EQ(n, const_ref_copies_cb.Run());
  n++;
  EXPECT_EQ(n - 1, const_ref_copies_cb.Run());
}

// Check that we can pass in arrays and have them be stored as a pointer.
//  - Array of values stores a pointer.
//  - Array of const values stores a pointer.
TEST_F(BindTest, ArrayArgumentBinding) {
  int array[4] = {1, 1, 1, 1};
  const int (*const_array_ptr)[4] = &array;

  Callback<int(void)> array_cb = Bind(&ArrayGet, array, 1);
  EXPECT_EQ(1, array_cb.Run());

  Callback<int(void)> const_array_cb = Bind(&ArrayGet, *const_array_ptr, 1);
  EXPECT_EQ(1, const_array_cb.Run());

  array[1] = 3;
  EXPECT_EQ(3, array_cb.Run());
  EXPECT_EQ(3, const_array_cb.Run());
}

// Verify SupportsAddRefAndRelease correctly introspects the class type for
// AddRef() and Release().
TEST_F(BindTest, SupportsAddRefAndRelease) {
  EXPECT_TRUE(internal::SupportsAddRefAndRelease<HasRef>::value);
  EXPECT_FALSE(internal::SupportsAddRefAndRelease<NoRef>::value);

  // StrictMock<T> is a derived class of T.  So, we use StrictMock<HasRef> and
  // StrictMock<NoRef> to test that SupportsAddRefAndRelease works over
  // inheritance.
  EXPECT_TRUE(internal::SupportsAddRefAndRelease<StrictMock<HasRef> >::value);
  EXPECT_FALSE(internal::SupportsAddRefAndRelease<StrictMock<NoRef> >::value);
}

// Unretained() wrapper support.
//   - Method bound to Unretained() non-object.
//   - Const method bound to Unretained() non-const object.
//   - Const method bound to Unretained() const object.
TEST_F(BindTest, Unretained) {
  EXPECT_CALL(no_ref_, VoidMethod0());
  EXPECT_CALL(no_ref_, VoidConstMethod0()).Times(2);

  Callback<void(void)> method_cb =
      Bind(&NoRef::VoidMethod0, Unretained(&no_ref_));
  method_cb.Run();

  Callback<void(void)> const_method_cb =
      Bind(&NoRef::VoidConstMethod0, Unretained(&no_ref_));
  const_method_cb.Run();

  Callback<void(void)> const_method_const_ptr_cb =
      Bind(&NoRef::VoidConstMethod0, Unretained(const_no_ref_ptr_));
  const_method_const_ptr_cb.Run();
}

// ConstRef() wrapper support.
//   - Binding w/o ConstRef takes a copy.
//   - Binding a ConstRef takes a reference.
//   - Binding ConstRef to a function ConstRef does not copy on invoke.
TEST_F(BindTest, ConstRef) {
  int n = 1;

  Callback<int(void)> copy_cb = Bind(&Identity, n);
  Callback<int(void)> const_ref_cb = Bind(&Identity, ConstRef(n));
  EXPECT_EQ(n, copy_cb.Run());
  EXPECT_EQ(n, const_ref_cb.Run());
  n++;
  EXPECT_EQ(n - 1, copy_cb.Run());
  EXPECT_EQ(n, const_ref_cb.Run());

  int copies = 0;
  int assigns = 0;
  CopyCounter counter(&copies, &assigns);
  Callback<int(void)> all_const_ref_cb =
      Bind(&GetCopies, ConstRef(counter));
  EXPECT_EQ(0, all_const_ref_cb.Run());
  EXPECT_EQ(0, copies);
  EXPECT_EQ(0, assigns);
}

// Argument Copy-constructor usage for non-reference parameters.
//   - Bound arguments are only copied once.
//   - Forwarded arguments are only copied once.
//   - Forwarded arguments with coerscions are only copied twice (once for the
//     coerscion, and one for the final dispatch).
TEST_F(BindTest, ArgumentCopies) {
  int copies = 0;
  int assigns = 0;

  CopyCounter counter(&copies, &assigns);

  Callback<void(void)> copy_cb =
      Bind(&VoidPolymorphic1<CopyCounter>, counter);
  EXPECT_GE(1, copies);
  EXPECT_EQ(0, assigns);

  copies = 0;
  assigns = 0;
  Callback<void(CopyCounter)> forward_cb =
      Bind(&VoidPolymorphic1<CopyCounter>);
  forward_cb.Run(counter);
  EXPECT_GE(1, copies);
  EXPECT_EQ(0, assigns);

  copies = 0;
  assigns = 0;
  DerivedCopyCounter dervied(&copies, &assigns);
  Callback<void(CopyCounter)> coerce_cb =
      Bind(&VoidPolymorphic1<CopyCounter>);
  coerce_cb.Run(dervied);
  EXPECT_GE(2, copies);
  EXPECT_EQ(0, assigns);
}

// Callback construction and assignment tests.
//   - Construction from an InvokerStorageHolder should not cause ref/deref.
//   - Assignment from other callback should only cause one ref
//
// TODO(ajwong): Is there actually a way to test this?

// No-compile tests. These should not compile. If they do, we are allowing
// error-prone, or incorrect behavior in the callback system.  Uncomment the
// tests to check.
TEST_F(BindTest, NoCompile) {
  // - Method bound to const-object.
  //
  // Only const methods should be allowed to work with const objects.
  //
  // Callback<void(void)> method_to_const_cb =
  //     Bind(&HasRef::VoidMethod0, const_has_ref_ptr_);
  // method_to_const_cb.Run();

  // - Method bound to non-refcounted object.
  // - Const Method bound to non-refcounted object.
  //
  // We require refcounts unless you have Unretained().
  //
  // Callback<void(void)> no_ref_cb =
  //     Bind(&NoRef::VoidMethod0, &no_ref_);
  // no_ref_cb.Run();
  // Callback<void(void)> no_ref_const_cb =
  //     Bind(&NoRef::VoidConstMethod0, &no_ref_);
  // no_ref_const_cb.Run();

  // - Unretained() used with a refcounted object.
  //
  // If the object supports refcounts, unretaining it in the callback is a
  // memory management contract break.
  // Callback<void(void)> unretained_cb =
  //     Bind(&HasRef::VoidConstMethod0, Unretained(&has_ref_));
  // unretained_cb.Run();

  // - Const argument used with non-const pointer parameter of same type.
  // - Const argument used with non-const pointer parameter of super type.
  //
  // This is just a const-correctness check.
  //
  // const Parent* const_parent_ptr;
  // const Child* const_child_ptr;
  // Callback<Parent*(void)> pointer_same_cb =
  //     Bind(&PolymorphicIdentity<Parent*>, const_parent_ptr);
  // pointer_same_cb.Run();
  // Callback<Parent*(void)> pointer_super_cb =
  //     Bind(&PolymorphicIdentity<Parent*>, const_child_ptr);
  // pointer_super_cb.Run();

  // - Construction of Callback<A> from Callback<B> if A is supertype of B.
  //   Specific example: Callback<void(void)> a; Callback<int(void)> b; a = b;
  //
  // While this is technically safe, most people aren't used to it when coding
  // C++ so if this is happening, it is almost certainly an error.
  //
  // Callback<int(void)> cb_a0 = Bind(&Identity, 1);
  // Callback<void(void)> cb_b0 = cb_a0;

  // - Assignment of Callback<A> from Callback<B> if A is supertype of B.
  // See explanation above.
  //
  // Callback<int(void)> cb_a1 = Bind(&Identity, 1);
  // Callback<void(void)> cb_b1;
  // cb_a1 = cb_b1;

  // - Functions with reference parameters, unsupported.
  //
  // First, non-const reference parameters are disallowed by the Google
  // style guide. Seconds, since we are doing argument forwarding it becomes
  // very tricky to avoid copies, maintain const correctness, and not
  // accidentally have the function be modifying a temporary, or a copy.
  //
  // NoRefParent p;
  // Callback<int(Parent&)> ref_arg_cb = Bind(&UnwrapNoRefParentRef);
  // ref_arg_cb.Run(p);
  // Callback<int(void)> ref_cb = Bind(&UnwrapNoRefParentRef, p);
  // ref_cb.Run();

  // - A method should not be bindable with an array of objects.
  //
  // This is likely not wanted behavior. We specifically check for it though
  // because it is possible, depending on how you implement prebinding, to
  // implicitly convert an array type to a pointer type.
  //
  // HasRef p[10];
  // Callback<void(void)> method_bound_to_array_cb =
  //    Bind(&HasRef::VoidConstMethod0, p);
  // method_bound_to_array_cb.Run();

  // - Refcounted types should not be bound as a raw pointer.
  // HasRef for_raw_ptr;
  // Callback<void(void)> ref_count_as_raw_ptr =
  //     Bind(&VoidPolymorphic1<HasRef*>, &for_raw_ptr);
  // ASSERT_EQ(&for_raw_ptr, ref_count_as_raw_ptr.Run());

}

}  // namespace
}  // namespace base
