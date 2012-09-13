#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for jni_generator.py.

This test suite contains various tests for the JNI generator.
It exercises the low-level parser all the way up to the
code generator and ensures the output matches a golden
file.
"""

import difflib
import os
import sys
import unittest
import jni_generator
from jni_generator import CalledByNative, NativeMethod, Param


class TestGenerator(unittest.TestCase):
  def assertObjEquals(self, first, second):
    dict_first = first.__dict__
    dict_second = second.__dict__
    self.assertEquals(dict_first.keys(), dict_second.keys())
    for key, value in dict_first.iteritems():
      if (type(value) is list and len(value) and
          isinstance(type(value[0]), object)):
        self.assertListEquals(value, second.__getattribute__(key))
      else:
        actual = second.__getattribute__(key)
        self.assertEquals(value, actual,
                          'Key ' + key + ': ' + str(value) + '!=' + str(actual))

  def assertListEquals(self, first, second):
    self.assertEquals(len(first), len(second))
    for i in xrange(len(first)):
      if isinstance(first[i], object):
        self.assertObjEquals(first[i], second[i])
      else:
        self.assertEquals(first[i], second[i])

  def assertTextEquals(self, golden_text, generated_text):
    stripped_golden = [l.strip() for l in golden_text.split('\n')]
    stripped_generated = [l.strip() for l in generated_text.split('\n')]
    if stripped_golden != stripped_generated:
      print self.id()
      for line in difflib.context_diff(stripped_golden, stripped_generated):
        print line
      print '\n\nGenerated'
      print '=' * 80
      print generated_text
      print '=' * 80
      self.fail('Golden text mismatch')

  # TODO(bulach): Detangle these tests from knowing about classes from Content.
  def testNatives(self):
    test_data = """"
    private native int nativeInit();
    private native void nativeDestroy(int nativeChromeBrowserProvider);
    private native long nativeAddBookmark(
            int nativeChromeBrowserProvider,
            String url, String title, boolean isFolder, long parentId);
    private static native String nativeGetDomainAndRegistry(String url);
    private static native void nativeCreateHistoricalTabFromState(
            byte[] state, int tab_index);
    private native byte[] nativeGetStateAsByteArray(ContentViewCore view);
    private static native String[] nativeGetAutofillProfileGUIDs();
    private native void nativeSetRecognitionResults(
            int sessionId, String[] results);
    private native long nativeAddBookmarkFromAPI(
            int nativeChromeBrowserProvider,
            String url, Long created, Boolean isBookmark,
            Long date, byte[] favicon, String title, Integer visits);
    native int nativeFindAll(String find);
    private static native BookmarkNode nativeGetDefaultBookmarkFolder();
    private native SQLiteCursor nativeQueryBookmarkFromAPI(
            int nativeChromeBrowserProvider,
            String[] projection, String selection,
            String[] selectionArgs, String sortOrder);
    private native void nativeGotOrientation(
            int nativeDataFetcherImplAndroid,
            double alpha, double beta, double gamma);
    """
    natives = jni_generator.ExtractNatives(test_data)
    golden_natives = [
        NativeMethod(return_type='int', static=False,
                     name='Init',
                     params=[],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='void', static=False, name='Destroy',
                     params=[Param(datatype='int',
                                   name='nativeChromeBrowserProvider')],
                     java_class_name=None,
                     type='method',
                     p0_type='ChromeBrowserProvider'),
        NativeMethod(return_type='long', static=False, name='AddBookmark',
                     params=[Param(datatype='int',
                                   name='nativeChromeBrowserProvider'),
                             Param(datatype='String',
                                   name='url'),
                             Param(datatype='String',
                                   name='title'),
                             Param(datatype='boolean',
                                   name='isFolder'),
                             Param(datatype='long',
                                   name='parentId')],
                     java_class_name=None,
                     type='method',
                     p0_type='ChromeBrowserProvider'),
        NativeMethod(return_type='String', static=True,
                     name='GetDomainAndRegistry',
                     params=[Param(datatype='String',
                                   name='url')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='void', static=True,
                     name='CreateHistoricalTabFromState',
                     params=[Param(datatype='byte[]',
                                   name='state'),
                             Param(datatype='int',
                                   name='tab_index')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='byte[]', static=False,
                     name='GetStateAsByteArray',
                     params=[Param(datatype='ContentViewCore', name='view')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='String[]', static=True,
                     name='GetAutofillProfileGUIDs', params=[],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='void', static=False,
                     name='SetRecognitionResults',
                     params=[Param(datatype='int', name='sessionId'),
                             Param(datatype='String[]', name='results')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='long', static=False,
                     name='AddBookmarkFromAPI',
                     params=[Param(datatype='int',
                                   name='nativeChromeBrowserProvider'),
                             Param(datatype='String',
                                   name='url'),
                             Param(datatype='Long',
                                   name='created'),
                             Param(datatype='Boolean',
                                   name='isBookmark'),
                             Param(datatype='Long',
                                   name='date'),
                             Param(datatype='byte[]',
                                   name='favicon'),
                             Param(datatype='String',
                                   name='title'),
                             Param(datatype='Integer',
                                   name='visits')],
                     java_class_name=None,
                     type='method',
                     p0_type='ChromeBrowserProvider'),
        NativeMethod(return_type='int', static=False,
                     name='FindAll',
                     params=[Param(datatype='String',
                                   name='find')],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='BookmarkNode', static=True,
                     name='GetDefaultBookmarkFolder',
                     params=[],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='SQLiteCursor',
                     static=False,
                     name='QueryBookmarkFromAPI',
                     params=[Param(datatype='int',
                                   name='nativeChromeBrowserProvider'),
                             Param(datatype='String[]',
                                   name='projection'),
                             Param(datatype='String',
                                   name='selection'),
                             Param(datatype='String[]',
                                   name='selectionArgs'),
                             Param(datatype='String',
                                   name='sortOrder'),
                            ],
                     java_class_name=None,
                     type='method',
                     p0_type='ChromeBrowserProvider'),
        NativeMethod(return_type='void', static=False,
                     name='GotOrientation',
                     params=[Param(datatype='int',
                                   name='nativeDataFetcherImplAndroid'),
                             Param(datatype='double',
                                   name='alpha'),
                             Param(datatype='double',
                                   name='beta'),
                             Param(datatype='double',
                                   name='gamma'),
                            ],
                     java_class_name=None,
                     type='method',
                     p0_type='content::DataFetcherImplAndroid'),
    ]
    self.assertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [])
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kTestJniClassPath[] = "org/chromium/TestJni";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

static jstring GetDomainAndRegistry(JNIEnv* env, jclass clazz,
    jstring url);

static void CreateHistoricalTabFromState(JNIEnv* env, jclass clazz,
    jbyteArray state,
    jint tab_index);

static jbyteArray GetStateAsByteArray(JNIEnv* env, jobject obj,
    jobject view);

static jobjectArray GetAutofillProfileGUIDs(JNIEnv* env, jclass clazz);

static void SetRecognitionResults(JNIEnv* env, jobject obj,
    jint sessionId,
    jobjectArray results);

static jint FindAll(JNIEnv* env, jobject obj,
    jstring find);

static jobject GetDefaultBookmarkFolder(JNIEnv* env, jclass clazz);

// Step 2: method stubs.
static void Destroy(JNIEnv* env, jobject obj,
    jint nativeChromeBrowserProvider) {
  DCHECK(nativeChromeBrowserProvider) << "Destroy";
  ChromeBrowserProvider* native =
      reinterpret_cast<ChromeBrowserProvider*>(nativeChromeBrowserProvider);
  return native->Destroy(env, obj);
}

static jlong AddBookmark(JNIEnv* env, jobject obj,
    jint nativeChromeBrowserProvider,
    jstring url,
    jstring title,
    jboolean isFolder,
    jlong parentId) {
  DCHECK(nativeChromeBrowserProvider) << "AddBookmark";
  ChromeBrowserProvider* native =
      reinterpret_cast<ChromeBrowserProvider*>(nativeChromeBrowserProvider);
  return native->AddBookmark(env, obj, url, title, isFolder, parentId);
}

static jlong AddBookmarkFromAPI(JNIEnv* env, jobject obj,
    jint nativeChromeBrowserProvider,
    jstring url,
    jobject created,
    jobject isBookmark,
    jobject date,
    jbyteArray favicon,
    jstring title,
    jobject visits) {
  DCHECK(nativeChromeBrowserProvider) << "AddBookmarkFromAPI";
  ChromeBrowserProvider* native =
      reinterpret_cast<ChromeBrowserProvider*>(nativeChromeBrowserProvider);
  return native->AddBookmarkFromAPI(env, obj, url, created, isBookmark, date,
      favicon, title, visits);
}

static jobject QueryBookmarkFromAPI(JNIEnv* env, jobject obj,
    jint nativeChromeBrowserProvider,
    jobjectArray projection,
    jstring selection,
    jobjectArray selectionArgs,
    jstring sortOrder) {
  DCHECK(nativeChromeBrowserProvider) << "QueryBookmarkFromAPI";
  ChromeBrowserProvider* native =
      reinterpret_cast<ChromeBrowserProvider*>(nativeChromeBrowserProvider);
  return native->QueryBookmarkFromAPI(env, obj, projection, selection,
      selectionArgs, sortOrder).Release();
}

static void GotOrientation(JNIEnv* env, jobject obj,
    jint nativeDataFetcherImplAndroid,
    jdouble alpha,
    jdouble beta,
    jdouble gamma) {
  DCHECK(nativeDataFetcherImplAndroid) << "GotOrientation";
  DataFetcherImplAndroid* native =
    reinterpret_cast<DataFetcherImplAndroid*>(nativeDataFetcherImplAndroid);
  return native->GotOrientation(env, obj, alpha, beta, gamma);
}

// Step 3: GetMethodIDs and RegisterNatives.
static void GetMethodIDsImpl(JNIEnv* env) {
  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, kTestJniClassPath)));
}

static bool RegisterNativesImpl(JNIEnv* env) {
  GetMethodIDsImpl(env);

  static const JNINativeMethod kMethodsTestJni[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
    { "nativeDestroy",
"("
"I"
")"
"V", reinterpret_cast<void*>(Destroy) },
    { "nativeAddBookmark",
"("
"I"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Z"
"J"
")"
"J", reinterpret_cast<void*>(AddBookmark) },
    { "nativeGetDomainAndRegistry",
"("
"Ljava/lang/String;"
")"
"Ljava/lang/String;", reinterpret_cast<void*>(GetDomainAndRegistry) },
    { "nativeCreateHistoricalTabFromState",
"("
"[B"
"I"
")"
"V", reinterpret_cast<void*>(CreateHistoricalTabFromState) },
    { "nativeGetStateAsByteArray",
"("
"Lorg/chromium/content/browser/ContentViewCore;"
")"
"[B", reinterpret_cast<void*>(GetStateAsByteArray) },
    { "nativeGetAutofillProfileGUIDs",
"("
")"
"[Ljava/lang/String;", reinterpret_cast<void*>(GetAutofillProfileGUIDs) },
    { "nativeSetRecognitionResults",
"("
"I"
"[Ljava/lang/String;"
")"
"V", reinterpret_cast<void*>(SetRecognitionResults) },
    { "nativeAddBookmarkFromAPI",
"("
"I"
"Ljava/lang/String;"
"Ljava/lang/Long;"
"Ljava/lang/Boolean;"
"Ljava/lang/Long;"
"[B"
"Ljava/lang/String;"
"Ljava/lang/Integer;"
")"
"J", reinterpret_cast<void*>(AddBookmarkFromAPI) },
    { "nativeFindAll",
"("
"Ljava/lang/String;"
")"
"I", reinterpret_cast<void*>(FindAll) },
    { "nativeGetDefaultBookmarkFolder",
"("
")"
"Lorg/chromium/chrome/browser/ChromeBrowserProvider$BookmarkNode;",
    reinterpret_cast<void*>(GetDefaultBookmarkFolder) },
    { "nativeQueryBookmarkFromAPI",
"("
"I"
"[Ljava/lang/String;"
"Ljava/lang/String;"
"[Ljava/lang/String;"
"Ljava/lang/String;"
")"
"Lcom/google/android/apps/chrome/database/SQLiteCursor;",
    reinterpret_cast<void*>(QueryBookmarkFromAPI) },
    { "nativeGotOrientation",
"("
"I"
"D"
"D"
"D"
")"
"V", reinterpret_cast<void*>(GotOrientation) },
  };
  const int kMethodsTestJniSize = arraysize(kMethodsTestJni);

  if (env->RegisterNatives(g_TestJni_clazz,
                           kMethodsTestJni,
                           kMethodsTestJniSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testInnerClassNatives(self):
    test_data = """
    class MyInnerClass {
      @NativeCall("MyInnerClass")
      private native int nativeInit();
    }
    """
    natives = jni_generator.ExtractNatives(test_data)
    golden_natives = [
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name='MyInnerClass',
                     type='function')
    ]
    self.assertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [])
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kTestJniClassPath[] = "org/chromium/TestJni";
const char kMyInnerClassClassPath[] = "org/chromium/TestJni$MyInnerClass";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

// Step 2: method stubs.

// Step 3: GetMethodIDs and RegisterNatives.
static void GetMethodIDsImpl(JNIEnv* env) {
  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, kTestJniClassPath)));
}

static bool RegisterNativesImpl(JNIEnv* env) {
  GetMethodIDsImpl(env);

  static const JNINativeMethod kMethodsMyInnerClass[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsMyInnerClassSize = arraysize(kMethodsMyInnerClass);

  if (env->RegisterNatives(g_MyInnerClass_clazz,
                           kMethodsMyInnerClass,
                           kMethodsMyInnerClassSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testInnerClassNativesMultiple(self):
    test_data = """
    class MyInnerClass {
      @NativeCall("MyInnerClass")
      private native int nativeInit();
    }
    class MyOtherInnerClass {
      @NativeCall("MyOtherInnerClass")
      private native int nativeInit();
    }
    """
    natives = jni_generator.ExtractNatives(test_data)
    golden_natives = [
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name='MyInnerClass',
                     type='function'),
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name='MyOtherInnerClass',
                     type='function')
    ]
    self.assertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [])
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kMyOtherInnerClassClassPath[] =
    "org/chromium/TestJni$MyOtherInnerClass";
const char kTestJniClassPath[] = "org/chromium/TestJni";
const char kMyInnerClassClassPath[] = "org/chromium/TestJni$MyInnerClass";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

static jint Init(JNIEnv* env, jobject obj);

// Step 2: method stubs.

// Step 3: GetMethodIDs and RegisterNatives.
static void GetMethodIDsImpl(JNIEnv* env) {
  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, kTestJniClassPath)));
}

static bool RegisterNativesImpl(JNIEnv* env) {
  GetMethodIDsImpl(env);

  static const JNINativeMethod kMethodsMyOtherInnerClass[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsMyOtherInnerClassSize =
      arraysize(kMethodsMyOtherInnerClass);

  if (env->RegisterNatives(g_MyOtherInnerClass_clazz,
                           kMethodsMyOtherInnerClass,
                           kMethodsMyOtherInnerClassSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  static const JNINativeMethod kMethodsMyInnerClass[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsMyInnerClassSize = arraysize(kMethodsMyInnerClass);

  if (env->RegisterNatives(g_MyInnerClass_clazz,
                           kMethodsMyInnerClass,
                           kMethodsMyInnerClassSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testInnerClassNativesBothInnerAndOuter(self):
    test_data = """
    class MyOuterClass {
      private native int nativeInit();
      class MyOtherInnerClass {
        @NativeCall("MyOtherInnerClass")
        private native int nativeInit();
      }
    }
    """
    natives = jni_generator.ExtractNatives(test_data)
    golden_natives = [
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name=None,
                     type='function'),
        NativeMethod(return_type='int', static=False,
                     name='Init', params=[],
                     java_class_name='MyOtherInnerClass',
                     type='function')
    ]
    self.assertListEquals(golden_natives, natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             natives, [])
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kMyOtherInnerClassClassPath[] =
    "org/chromium/TestJni$MyOtherInnerClass";
const char kTestJniClassPath[] = "org/chromium/TestJni";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
}  // namespace

static jint Init(JNIEnv* env, jobject obj);

static jint Init(JNIEnv* env, jobject obj);

// Step 2: method stubs.

// Step 3: GetMethodIDs and RegisterNatives.
static void GetMethodIDsImpl(JNIEnv* env) {
  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, kTestJniClassPath)));
}

static bool RegisterNativesImpl(JNIEnv* env) {
  GetMethodIDsImpl(env);

  static const JNINativeMethod kMethodsMyOtherInnerClass[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsMyOtherInnerClassSize =
      arraysize(kMethodsMyOtherInnerClass);

  if (env->RegisterNatives(g_MyOtherInnerClass_clazz,
                           kMethodsMyOtherInnerClass,
                           kMethodsMyOtherInnerClassSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  static const JNINativeMethod kMethodsTestJni[] = {
    { "nativeInit",
"("
")"
"I", reinterpret_cast<void*>(Init) },
  };
  const int kMethodsTestJniSize = arraysize(kMethodsTestJni);

  if (env->RegisterNatives(g_TestJni_clazz,
                           kMethodsTestJni,
                           kMethodsTestJniSize) < 0) {
    LOG(ERROR) << "RegisterNatives failed in " << __FILE__;
    return false;
  }

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testCalledByNatives(self):
    test_data = """"
    @CalledByNative
    NativeInfoBar showConfirmInfoBar(int nativeInfoBar, String buttonOk,
                                     String buttonCancel, String title,
                                     Bitmap icon) {
        InfoBar infobar = new ConfirmInfoBar(nativeInfoBar, mContext,
                                             buttonOk, buttonCancel,
                                             title, icon);
        return infobar;
    }
    @CalledByNative
    NativeInfoBar showAutoLoginInfoBar(int nativeInfoBar,
                                       String realm, String account,
                                       String args) {
        AutoLoginInfoBar infobar = new AutoLoginInfoBar(nativeInfoBar, mContext,
                realm, account, args);
        if (infobar.displayedAccountCount() == 0)
            infobar = null;
        return infobar;
    }
    @CalledByNative("InfoBar")
    void dismiss();
    @SuppressWarnings("unused")
    @CalledByNative
    private static boolean shouldShowAutoLogin(ContentViewCore contentView,
            String realm, String account, String args) {
        AccountManagerContainer accountManagerContainer =
            new AccountManagerContainer((Activity)contentView.getContext(),
            realm, account, args);
        String[] logins = accountManagerContainer.getAccountLogins(null);
        return logins.length != 0;
    }
    @CalledByNative
    static InputStream openUrl(String url) {
        return null;
    }
    @CalledByNative
    private void activateHardwareAcceleration(final boolean activated,
            final int iPid, final int iType,
            final int iPrimaryID, final int iSecondaryID) {
      if (!activated) {
          return
      }
    }
    @CalledByNativeUnchecked
    private void uncheckedCall(int iParam);
    """
    called_by_natives = jni_generator.ExtractCalledByNatives(test_data)
    golden_called_by_natives = [
        CalledByNative(
            return_type='NativeInfoBar',
            system_class=False,
            static=False,
            name='showConfirmInfoBar',
            method_id_var_name='showConfirmInfoBar',
            java_class_name='',
            params=[Param(datatype='int', name='nativeInfoBar'),
                    Param(datatype='String', name='buttonOk'),
                    Param(datatype='String', name='buttonCancel'),
                    Param(datatype='String', name='title'),
                    Param(datatype='Bitmap', name='icon')],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='NativeInfoBar',
            system_class=False,
            static=False,
            name='showAutoLoginInfoBar',
            method_id_var_name='showAutoLoginInfoBar',
            java_class_name='',
            params=[Param(datatype='int', name='nativeInfoBar'),
                    Param(datatype='String', name='realm'),
                    Param(datatype='String', name='account'),
                    Param(datatype='String', name='args')],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='dismiss',
            method_id_var_name='dismiss',
            java_class_name='InfoBar',
            params=[],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='boolean',
            system_class=False,
            static=True,
            name='shouldShowAutoLogin',
            method_id_var_name='shouldShowAutoLogin',
            java_class_name='',
            params=[Param(datatype='ContentViewCore', name='contentView'),
                    Param(datatype='String', name='realm'),
                    Param(datatype='String', name='account'),
                    Param(datatype='String', name='args')],
            env_call=('Boolean', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='InputStream',
            system_class=False,
            static=True,
            name='openUrl',
            method_id_var_name='openUrl',
            java_class_name='',
            params=[Param(datatype='String', name='url')],
            env_call=('Object', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='activateHardwareAcceleration',
            method_id_var_name='activateHardwareAcceleration',
            java_class_name='',
            params=[Param(datatype='boolean', name='activated'),
                    Param(datatype='int', name='iPid'),
                    Param(datatype='int', name='iType'),
                    Param(datatype='int', name='iPrimaryID'),
                    Param(datatype='int', name='iSecondaryID'),
                   ],
            env_call=('Void', ''),
            unchecked=False,
        ),
        CalledByNative(
            return_type='void',
            system_class=False,
            static=False,
            name='uncheckedCall',
            method_id_var_name='uncheckedCall',
            java_class_name='',
            params=[Param(datatype='int', name='iParam')],
            env_call=('Void', ''),
            unchecked=True,
        ),
    ]
    self.assertListEquals(golden_called_by_natives, called_by_natives)
    h = jni_generator.InlHeaderFileGenerator('', 'org/chromium/TestJni',
                                             [], called_by_natives)
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     org/chromium/TestJni

#ifndef org_chromium_TestJni_JNI
#define org_chromium_TestJni_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kTestJniClassPath[] = "org/chromium/TestJni";
const char kInfoBarClassPath[] = "org/chromium/TestJni$InfoBar";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_TestJni_clazz = NULL;
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_InfoBar_clazz = NULL;
}  // namespace

// Step 2: method stubs.

static jmethodID g_TestJni_showConfirmInfoBar = 0;
static ScopedJavaLocalRef<jobject> Java_TestJni_showConfirmInfoBar(JNIEnv* env,
    jobject obj, jint nativeInfoBar,
    jstring buttonOk,
    jstring buttonCancel,
    jstring title,
    jobject icon) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  DCHECK(g_TestJni_showConfirmInfoBar);
  jobject ret =
    env->CallObjectMethod(obj,
      g_TestJni_showConfirmInfoBar, nativeInfoBar, buttonOk, buttonCancel,
          title, icon);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static jmethodID g_TestJni_showAutoLoginInfoBar = 0;
static ScopedJavaLocalRef<jobject> Java_TestJni_showAutoLoginInfoBar(JNIEnv*
    env, jobject obj, jint nativeInfoBar,
    jstring realm,
    jstring account,
    jstring args) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  DCHECK(g_TestJni_showAutoLoginInfoBar);
  jobject ret =
    env->CallObjectMethod(obj,
      g_TestJni_showAutoLoginInfoBar, nativeInfoBar, realm, account, args);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static jmethodID g_InfoBar_dismiss = 0;
static void Java_InfoBar_dismiss(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InfoBar_clazz);
  DCHECK(g_InfoBar_dismiss);

  env->CallVoidMethod(obj,
      g_InfoBar_dismiss);
  base::android::CheckException(env);

}

static jmethodID g_TestJni_shouldShowAutoLogin = 0;
static jboolean Java_TestJni_shouldShowAutoLogin(JNIEnv* env, jobject
    contentView,
    jstring realm,
    jstring account,
    jstring args) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  DCHECK(g_TestJni_shouldShowAutoLogin);
  jboolean ret =
    env->CallStaticBooleanMethod(g_TestJni_clazz,
      g_TestJni_shouldShowAutoLogin, contentView, realm, account, args);
  base::android::CheckException(env);
  return ret;
}

static jmethodID g_TestJni_openUrl = 0;
static ScopedJavaLocalRef<jobject> Java_TestJni_openUrl(JNIEnv* env, jstring
    url) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  DCHECK(g_TestJni_openUrl);
  jobject ret =
    env->CallStaticObjectMethod(g_TestJni_clazz,
      g_TestJni_openUrl, url);
  base::android::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static jmethodID g_TestJni_activateHardwareAcceleration = 0;
static void Java_TestJni_activateHardwareAcceleration(JNIEnv* env, jobject obj,
    jboolean activated,
    jint iPid,
    jint iType,
    jint iPrimaryID,
    jint iSecondaryID) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  DCHECK(g_TestJni_activateHardwareAcceleration);

  env->CallVoidMethod(obj,
      g_TestJni_activateHardwareAcceleration, activated, iPid, iType,
          iPrimaryID, iSecondaryID);
  base::android::CheckException(env);

}

static jmethodID g_TestJni_uncheckedCall = 0;
static void Java_TestJni_uncheckedCall(JNIEnv* env, jobject obj, jint iParam) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_TestJni_clazz);
  DCHECK(g_TestJni_uncheckedCall);

  env->CallVoidMethod(obj,
      g_TestJni_uncheckedCall, iParam);

}

// Step 3: GetMethodIDs and RegisterNatives.
static void GetMethodIDsImpl(JNIEnv* env) {
  g_TestJni_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, kTestJniClassPath)));
  g_InfoBar_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, kInfoBarClassPath)));
  g_TestJni_showConfirmInfoBar =
      base::android::GetMethodID(
          env, g_TestJni_clazz,
          "showConfirmInfoBar",

"("
"I"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Landroid/graphics/Bitmap;"
")"
"Lcom/google/android/apps/chrome/infobar/InfoBarContainer$NativeInfoBar;");

  g_TestJni_showAutoLoginInfoBar =
      base::android::GetMethodID(
          env, g_TestJni_clazz,
          "showAutoLoginInfoBar",

"("
"I"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Ljava/lang/String;"
")"
"Lcom/google/android/apps/chrome/infobar/InfoBarContainer$NativeInfoBar;");

  g_InfoBar_dismiss =
      base::android::GetMethodID(
          env, g_InfoBar_clazz,
          "dismiss",

"("
")"
"V");

  g_TestJni_shouldShowAutoLogin =
      base::android::GetStaticMethodID(
          env, g_TestJni_clazz,
          "shouldShowAutoLogin",

"("
"Lorg/chromium/content/browser/ContentViewCore;"
"Ljava/lang/String;"
"Ljava/lang/String;"
"Ljava/lang/String;"
")"
"Z");

  g_TestJni_openUrl =
      base::android::GetStaticMethodID(
          env, g_TestJni_clazz,
          "openUrl",

"("
"Ljava/lang/String;"
")"
"Ljava/io/InputStream;");

  g_TestJni_activateHardwareAcceleration =
      base::android::GetMethodID(
          env, g_TestJni_clazz,
          "activateHardwareAcceleration",

"("
"Z"
"I"
"I"
"I"
"I"
")"
"V");

  g_TestJni_uncheckedCall =
      base::android::GetMethodID(
          env, g_TestJni_clazz,
          "uncheckedCall",

"("
"I"
")"
"V");

}

static bool RegisterNativesImpl(JNIEnv* env) {
  GetMethodIDsImpl(env);

  return true;
}

#endif  // org_chromium_TestJni_JNI
"""
    self.assertTextEquals(golden_content, h.GetContent())

  def testCalledByNativeParseError(self):
    try:
      jni_generator.ExtractCalledByNatives("""
@CalledByNative
public static int foo(); // This one is fine

@CalledByNative
scooby doo
""")
      self.fail('Expected a ParseError')
    except jni_generator.ParseError, e:
      self.assertEquals(('@CalledByNative', 'scooby doo'), e.context_lines)

  def testFullyQualifiedClassName(self):
    contents = """
// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.BuildInfo;
"""
    self.assertEquals('org/chromium/content/browser/Foo',
                      jni_generator.ExtractFullyQualifiedJavaClassName(
                          'org/chromium/content/browser/Foo.java', contents))
    self.assertEquals('org/chromium/content/browser/Foo',
                      jni_generator.ExtractFullyQualifiedJavaClassName(
                          'frameworks/Foo.java', contents))
    self.assertRaises(SyntaxError,
                      jni_generator.ExtractFullyQualifiedJavaClassName,
                      'com/foo/Bar', 'no PACKAGE line')

  def testMethodNameMangling(self):
    self.assertEquals('close_pqV',
                      jni_generator.GetMangledMethodName('close', '()V'))
    self.assertEquals('read_paBIIqI',
                      jni_generator.GetMangledMethodName('read', '([BII)I'))
    self.assertEquals('open_pLjava_lang_StringxqLjava_io_InputStreamx',
                      jni_generator.GetMangledMethodName(
                          'open',
                          '(Ljava/lang/String;)Ljava/io/InputStream;'))

  def testFromJavaP(self):
    contents = """
public abstract class java.io.InputStream extends java.lang.Object
      implements java.io.Closeable{
    public java.io.InputStream();
    public int available()       throws java.io.IOException;
    public void close()       throws java.io.IOException;
    public void mark(int);
    public boolean markSupported();
    public abstract int read()       throws java.io.IOException;
    public int read(byte[])       throws java.io.IOException;
    public int read(byte[], int, int)       throws java.io.IOException;
    public synchronized void reset()       throws java.io.IOException;
    public long skip(long)       throws java.io.IOException;
}
"""
    jni_from_javap = jni_generator.JNIFromJavaP(contents.split('\n'), None)
    self.assertEquals(9, len(jni_from_javap.called_by_natives))
    golden_content = """\
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator_tests.py
// For
//     java/io/InputStream

#ifndef java_io_InputStream_JNI
#define java_io_InputStream_JNI

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/logging.h"

using base::android::ScopedJavaLocalRef;

// Step 1: forward declarations.
namespace {
const char kInputStreamClassPath[] = "java/io/InputStream";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_InputStream_clazz = NULL;
}  // namespace

namespace JNI_InputStream {

// Step 2: method stubs.

static jmethodID g_InputStream_available = 0;
static jint Java_InputStream_available(JNIEnv* env, jobject obj) __attribute__
    ((unused));
static jint Java_InputStream_available(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_available);
  jint ret =
    env->CallIntMethod(obj,
      g_InputStream_available);
  base::android::CheckException(env);
  return ret;
}

static jmethodID g_InputStream_close = 0;
static void Java_InputStream_close(JNIEnv* env, jobject obj) __attribute__
    ((unused));
static void Java_InputStream_close(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_close);

  env->CallVoidMethod(obj,
      g_InputStream_close);
  base::android::CheckException(env);

}

static jmethodID g_InputStream_mark = 0;
static void Java_InputStream_mark(JNIEnv* env, jobject obj, jint p0)
    __attribute__ ((unused));
static void Java_InputStream_mark(JNIEnv* env, jobject obj, jint p0) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_mark);

  env->CallVoidMethod(obj,
      g_InputStream_mark, p0);
  base::android::CheckException(env);

}

static jmethodID g_InputStream_markSupported = 0;
static jboolean Java_InputStream_markSupported(JNIEnv* env, jobject obj)
    __attribute__ ((unused));
static jboolean Java_InputStream_markSupported(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_markSupported);
  jboolean ret =
    env->CallBooleanMethod(obj,
      g_InputStream_markSupported);
  base::android::CheckException(env);
  return ret;
}

static jmethodID g_InputStream_read_pqI = 0;
static jint Java_InputStream_read(JNIEnv* env, jobject obj) __attribute__
    ((unused));
static jint Java_InputStream_read(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_read_pqI);
  jint ret =
    env->CallIntMethod(obj,
      g_InputStream_read_pqI);
  base::android::CheckException(env);
  return ret;
}

static jmethodID g_InputStream_read_paBqI = 0;
static jint Java_InputStream_read(JNIEnv* env, jobject obj, jbyteArray p0)
    __attribute__ ((unused));
static jint Java_InputStream_read(JNIEnv* env, jobject obj, jbyteArray p0) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_read_paBqI);
  jint ret =
    env->CallIntMethod(obj,
      g_InputStream_read_paBqI, p0);
  base::android::CheckException(env);
  return ret;
}

static jmethodID g_InputStream_read_paBIIqI = 0;
static jint Java_InputStream_read(JNIEnv* env, jobject obj, jbyteArray p0,
    jint p1,
    jint p2) __attribute__ ((unused));
static jint Java_InputStream_read(JNIEnv* env, jobject obj, jbyteArray p0,
    jint p1,
    jint p2) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_read_paBIIqI);
  jint ret =
    env->CallIntMethod(obj,
      g_InputStream_read_paBIIqI, p0, p1, p2);
  base::android::CheckException(env);
  return ret;
}

static jmethodID g_InputStream_reset = 0;
static void Java_InputStream_reset(JNIEnv* env, jobject obj) __attribute__
    ((unused));
static void Java_InputStream_reset(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_reset);

  env->CallVoidMethod(obj,
      g_InputStream_reset);
  base::android::CheckException(env);

}

static jmethodID g_InputStream_skip = 0;
static jlong Java_InputStream_skip(JNIEnv* env, jobject obj, jlong p0)
    __attribute__ ((unused));
static jlong Java_InputStream_skip(JNIEnv* env, jobject obj, jlong p0) {
  /* Must call RegisterNativesImpl()  */
  DCHECK(g_InputStream_clazz);
  DCHECK(g_InputStream_skip);
  jlong ret =
    env->CallLongMethod(obj,
      g_InputStream_skip, p0);
  base::android::CheckException(env);
  return ret;
}

// Step 3: GetMethodIDs and RegisterNatives.
static void GetMethodIDsImpl(JNIEnv* env) {
  g_InputStream_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetUnscopedClass(env, kInputStreamClassPath)));
  g_InputStream_available =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "available",

"("
")"
"I");

  g_InputStream_close =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "close",

"("
")"
"V");

  g_InputStream_mark =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "mark",

"("
"I"
")"
"V");

  g_InputStream_markSupported =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "markSupported",

"("
")"
"Z");

  g_InputStream_read_pqI =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "read",

"("
")"
"I");

  g_InputStream_read_paBqI =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "read",

"("
"[B"
")"
"I");

  g_InputStream_read_paBIIqI =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "read",

"("
"[B"
"I"
"I"
")"
"I");

  g_InputStream_reset =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "reset",

"("
")"
"V");

  g_InputStream_skip =
      base::android::GetMethodID(
          env, g_InputStream_clazz,
          "skip",

"("
"J"
")"
"J");

}

static bool RegisterNativesImpl(JNIEnv* env) {
  GetMethodIDsImpl(env);

  return true;
}
}  // namespace JNI_InputStream

#endif  // java_io_InputStream_JNI
"""
    self.assertTextEquals(golden_content, jni_from_javap.GetContent())

  def testREForNatives(self):
    # We should not match "native SyncSetupFlow" inside the comment.
    test_data = """
    /**
     * Invoked when the setup process is complete so we can disconnect from the
     * native-side SyncSetupFlowHandler.
     */
    public void destroy() {
        Log.v(TAG, "Destroying native SyncSetupFlow");
        if (mNativeSyncSetupFlow != 0) {
            nativeSyncSetupEnded(mNativeSyncSetupFlow);
            mNativeSyncSetupFlow = 0;
        }
    }
    private native void nativeSyncSetupEnded(
        int nativeAndroidSyncSetupFlowHandler);
    """
    jni_from_java = jni_generator.JNIFromJavaSource(test_data, 'foo/bar')

  def testRaisesOnUnknownDatatype(self):
    test_data = """
    class MyInnerClass {
      private native int nativeInit(AnUnknownDatatype p0);
    }
    """
    self.assertRaises(SyntaxError,
                      jni_generator.JNIFromJavaSource,
                      test_data, 'foo/bar')

  def testRaisesOnNonJNIMethod(self):
    test_data = """
    class MyInnerClass {
      private int Foo(int p0) {
      }
    }
    """
    self.assertRaises(SyntaxError,
                      jni_generator.JNIFromJavaSource,
                      test_data, 'foo/bar')

  def testJniSelfDocumentingExample(self):
    script_dir = os.path.dirname(sys.argv[0])
    content = file(os.path.join(script_dir, 'SampleForTests.java')).read()
    golden_content = file(os.path.join(script_dir,
                                       'golden_sample_for_tests_jni.h')).read()
    jni_from_java = jni_generator.JNIFromJavaSource(
        content, 'org/chromium/example/jni_generator/SampleForTests')
    self.assertTextEquals(golden_content, jni_from_java.GetContent())

  def testNoWrappingPreprocessorLines(self):
    test_data = """
    package com.google.lookhowextremelylongiam.snarf.icankeepthisupallday;

    class ReallyLongClassNamesAreAllTheRage {
        private static native int nativeTest();
    }
    """
    jni_from_java = jni_generator.JNIFromJavaSource(
        test_data, ('com/google/lookhowextremelylongiam/snarf/'
                    'icankeepthisupallday/ReallyLongClassNamesAreAllTheRage'))
    jni_lines = jni_from_java.GetContent().split('\n')
    line = filter(lambda line: line.lstrip().startswith('#ifndef'),
                  jni_lines)[0]
    self.assertTrue(len(line) > 80,
                    ('Expected #ifndef line to be > 80 chars: ', line))

if __name__ == '__main__':
  unittest.main()
