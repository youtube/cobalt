// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.example.jni_generator;

import android.graphics.Rect;

// This class serves as a reference test for the bindings generator, and as example documentation
// for how to use the jni generator.
// The C++ counter-part is sample_for_tests.cc.
// jni_generator.gyp has a jni_generator_tests target that will:
//   * Generate a header file for the JNI bindings based on this file.
//   * Compile sample_for_tests.cc using the generated header file.
//   * link a native executable to prove the generated header + cc file are self-contained.
// All comments are informational only, and are ignored by the jni generator.
//
// This JNINamespace annotation indicates that all native methods should be
// generated inside this namespace, including the native class that this
// object binds to.
@JNINamespace("base::android")
class SampleForTests {
  // Classes can store their C++ pointer counter part as an int that is normally initialized by
  // calling out a nativeInit() function.
  int nativePtr;

  // You can define methods and attributes on the java class just like any other.
  // Methods without the @CalledByNative annotation won't be exposed to JNI.
  public SampleForTests() {
  }

  public void startExample() {
      // Calls native code and holds a pointer to the C++ class.
      nativePtr = nativeInit("myParam");
  }

  public void doStuff() {
      // This will call CPPClass::Method() using nativePtr as a pointer to the object. This must be
      // done to:
      // * avoid leaks.
      // * using finalizers are not allowed to destroy the cpp class.
      nativeMethod(nativePtr);
  }

  public void finishExample() {
      // We're done, so let's destroy nativePtr object.
      nativeDestroy(nativePtr);
  }

  // -----------------------------------------------------------------------------------------------
  // The following methods demonstrate exporting Java methods for invocation from C++ code.
  // Java functions are mapping into C global functions by prefixing the method name with
  // "Java_<Class>_"
  // This is triggered by the @CalledByNative annotation; the methods may be named as you wish.

  // Exported to C++ as:
  //   Java_Example_javaMethod(JNIEnv* env, jobject obj, jint foo, jint bar)
  // Typically the C++ code would have obtained the jobject via the Init() call described above.
  @CalledByNative
  public int javaMethod(int foo,
                        int bar) {
      return 0;
  }

  // Exported to C++ as Java_Example_staticJavaMethod(JNIEnv* env)
  // Note no jobject argument, as it is static.
  @CalledByNative
  public static boolean staticJavaMethod() {
      return true;
  }

  // No prefix, so this method is package private. It will still be exported.
  @CalledByNative
  void packagePrivateJavaMethod() {}

  // Note the "Unchecked" suffix. By default, @CalledByNative will always generate bindings that
  // call CheckException(). With "@CalledByNativeUnchecked", the client C++ code is responsible to
  // call ClearException() and act as appropriate.
  // See more details at the "@CalledByNativeUnchecked" annotation.
  @CalledByNativeUnchecked
  void methodThatThrowsException() throws Exception {}

  // The generator is not confused by inline comments:
  // @CalledByNative void thisShouldNotAppearInTheOutput();
  // @CalledByNativeUnchecked public static void neitherShouldThis(int foo);

  /**
   * The generator is not confused by block comments:
   * @CalledByNative void thisShouldNotAppearInTheOutputEither();
   * @CalledByNativeUnchecked public static void andDefinitelyNotThis(int foo);
   */

  // String constants that look like comments don't confuse the generator:
  private String arrgh = "*/*";

  //------------------------------------------------------------------------------------------------
  // Java fields which are accessed from C++ code only must be annotated with @AccessedByNative to
  // prevent them being eliminated when unreferenced code is stripped.
  @AccessedByNative
  private int javaField;

  //------------------------------------------------------------------------------------------------
  // The following methods demonstrate declaring methods to call into C++ from Java.
  // The generator detects the "native" and "static" keywords, the type and name of the first
  // parameter, and the "native" prefix to the function name to determine the C++ function
  // signatures. Besides these constraints the methods can be freely named.

  // This declares a C++ function which the application code must implement:
  //   static jint Init(JNIEnv* env, jobject obj);
  // The jobject parameter refers back to this java side object instance.
  // The implementation must return the pointer to the C++ object cast to jint.
  // The caller of this method should store it, and supply it as a the nativeCPPClass param to
  // subsequent native method calls (see the methods below that take an "int native..." as first
  // param).
  private native int nativeInit();

  // This defines a function binding to the associated C++ class member function. The name is
  // derived from |nativeDestroy| and |nativeCPPClass| to arrive at CPPClass::Destroy() (i.e. native
  // prefixes stripped).
  // The |nativeCPPClass| is automatically cast to type CPPClass* in order to obtain the object on
  // which to invoke the member function.
  private native void nativeDestroy(int nativeCPPClass);

  // This declares a C++ function which the application code must implement:
  //   static jdouble GetDoubleFunction(JNIEnv* env, jobject obj);
  // The jobject parameter refers back to this java side object instance.
  private native double nativeGetDoubleFunction();

  // Similar to nativeGetDoubleFunction(), but here the C++ side will receive a jclass rather than
  // jobject param, as the function is declared static.
  private static native float nativeGetFloatFunction();

  // This function takes a non-POD datatype. We have a list mapping them to their full classpath in
  // jni_generator.py JavaParamToJni. If you require a new datatype, make sure you add to that
  // function.
  private native void nativeSetNonPODDatatype(Rect rect);

  // This declares a C++ function which the application code must implement:
  //   static ScopedJavaLocalRef<jobject> GetNonPODDatatype(JNIEnv* env, jobject obj);
  // The jobject parameter refers back to this java side object instance.
  // Note that it returns a ScopedJavaLocalRef<jobject> so that you don' have to worry about
  // deleting the JNI local reference.  This is similar with Strings and arrays.
  private native Object nativeGetNonPODDatatype();

  // Similar to nativeDestroy above, this will cast nativeCPPClass into pointer of CPPClass type and
  // call its Method member function.
  private native int nativeMethod(int nativeCPPClass);

  // Similar to nativeMethod above, but here the C++ fully qualified class name is taken from the
  // annotation rather than parameter name, which can thus be chosen freely.
  @NativeClassQualifiedName("CPPClass::InnerClass")
  private native double nativeMethodOtherP0(int nativePtr);

  // An inner class has some special attributes for annotation.
  class InnerClass {
    @CalledByNative("InnerClass")
    public float JavaInnerMethod() {
    }

    @CalledByNative("InnerClass")
    public static void javaInnerFunction() {
    }

    @NativeCall("InnerClass")
    private static native int nativeInnerFunction();

    @NativeCall("InnerClass")
    private static native String nativeInnerMethod(int nativeCPPClass);

  }
}
