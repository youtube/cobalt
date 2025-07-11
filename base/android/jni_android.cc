// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include <cstring>
#include <stddef.h>
#include <sys/prctl.h>

#if BUILDFLAG(IS_COBALT)
#include <string>
#include <iostream>
#include <regex>
#endif

#include "base/android/java_exception_reporter.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/android_runtime_jni_headers/Throwable_jni.h"
#include "base/debug/debugging_buildflags.h"
#include "base/feature_list.h"
#include "base/logging.h"
<<<<<<< HEAD
#include "base/strings/string_util.h"
=======
#include "base/base_switches.h"
#include "base/command_line.h"
>>>>>>> 6c8079971a5 (CoAT changes to support Kimono build (#4506))
#include "build/build_config.h"
#include "build/robolectric_buildflags.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_minimal_jni/JniAndroid_jni.h"

namespace base {
namespace android {
namespace {

// If disabled, we LOG(FATAL) immediately in native code when faced with an
// uncaught Java exception (historical behavior). If enabled, we give the Java
// uncaught exception handler a chance to handle the exception first, so that
// the crash is (hopefully) seen as a Java crash, not a native crash.
// TODO(crbug.com/40261529): remove this switch once we are confident the
// new behavior is fine.
BASE_FEATURE(kHandleExceptionsInJava,
             "HandleJniExceptionsInJava",
             base::FEATURE_ENABLED_BY_DEFAULT);

jclass g_out_of_memory_error_class = nullptr;

<<<<<<< HEAD
#if !BUILDFLAG(IS_ROBOLECTRIC)
jmethodID g_class_loader_load_class_method_id = nullptr;
// ClassLoader.loadClass() accepts either slashes or dots on Android, but JVM
// requires dots. We could translate, but there is no need to go through
// ClassLoaders in Robolectric anyways.
// https://cs.android.com/search?q=symbol:DexFile_defineClassNative
jclass GetClassFromSplit(JNIEnv* env,
                         const char* class_name,
                         const char* split_name) {
  DCHECK(IsStringASCII(class_name));
  ScopedJavaLocalRef<jstring> j_class_name(env, env->NewStringUTF(class_name));
  return static_cast<jclass>(env->CallObjectMethod(
      GetSplitClassLoader(env, split_name), g_class_loader_load_class_method_id,
      j_class_name.obj()));
=======
bool g_fatal_exception_occurred = false;

/* Cobalt specific hack to move Java classes to a custom namespace.
   For every class org.chromium.foo moves them to cobalt.org.chromium.foo
   This works around link-time conflicts when building the final
   package against other Chromium release artifacts. */
#if BUILDFLAG(IS_COBALT)
const char* COBALT_ORG_CHROMIUM = "cobalt/org/chromium";
const char* ORG_CHROMIUM = "org/chromium";

bool g_add_cobalt_prefix = false;
std::atomic<bool> g_checked_command_line(false);

std::string getRepackagedName(const char* signature) {
  std::string holder(signature);
  size_t pos = 0;
  while ((pos = holder.find(ORG_CHROMIUM, pos)) != std::string::npos) {
    holder.replace(pos, strlen(ORG_CHROMIUM), COBALT_ORG_CHROMIUM);
    pos += strlen(COBALT_ORG_CHROMIUM);
  }
  return holder;
}

bool shouldAddCobaltPrefix() {
  if (!g_checked_command_line && base::CommandLine::InitializedForCurrentProcess()) {
    g_add_cobalt_prefix = base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kCobaltJniPrefix);
    g_checked_command_line = true;
  }
  return g_add_cobalt_prefix;
}
#endif

ScopedJavaLocalRef<jclass> GetClassInternal(JNIEnv* env,
#if BUILDFLAG(IS_COBALT)
                                            const char* original_class_name,
                                            jobject class_loader) {
  const char* class_name;
  std::string holder;
  if (shouldAddCobaltPrefix()) {
    holder = getRepackagedName(original_class_name);
    class_name = holder.c_str();
  } else {
    class_name = original_class_name;
  }
#else
                                            const char* class_name,
                                            jobject class_loader) {
#endif
  jclass clazz;
  if (class_loader != nullptr) {
    // ClassLoader.loadClass expects a classname with components separated by
    // dots instead of the slashes that JNIEnv::FindClass expects. The JNI
    // generator generates names with slashes, so we have to replace them here.
    // TODO(torne): move to an approach where we always use ClassLoader except
    // for the special case of base::android::GetClassLoader(), and change the
    // JNI generator to generate dot-separated names. http://crbug.com/461773
    size_t bufsize = strlen(class_name) + 1;
    char dotted_name[bufsize];
    memmove(dotted_name, class_name, bufsize);
    for (size_t i = 0; i < bufsize; ++i) {
      if (dotted_name[i] == '/') {
        dotted_name[i] = '.';
      }
    }

    clazz = static_cast<jclass>(
        env->CallObjectMethod(class_loader, g_class_loader_load_class_method_id,
                              ConvertUTF8ToJavaString(env, dotted_name).obj()));
  } else {
    clazz = env->FindClass(class_name);
  }
  if (ClearException(env) || !clazz) {
    LOG(FATAL) << "Failed to find class " << class_name;
  }
  return ScopedJavaLocalRef<jclass>(env, clazz);
>>>>>>> 6c8079971a5 (CoAT changes to support Kimono build (#4506))
}

// Must be called before using GetClassFromSplit - we need to set the global,
// and we need to call GetClassLoader at least once to allow the default
// resolver (env->FindClass()) to get our main ClassLoader class instance, which
// we then cache use for all future calls to GetSplitClassLoader.
void PrepareClassLoaders(JNIEnv* env) {
  if (g_class_loader_load_class_method_id == nullptr) {
    GetClassLoader(env);
    ScopedJavaLocalRef<jclass> class_loader_clazz = ScopedJavaLocalRef<jclass>(
        env, env->FindClass("java/lang/ClassLoader"));
    CHECK(!ClearException(env));
    g_class_loader_load_class_method_id =
        env->GetMethodID(class_loader_clazz.obj(), "loadClass",
                         "(Ljava/lang/String;)Ljava/lang/Class;");
    CHECK(!ClearException(env));
  }
}
#endif  // !BUILDFLAG(IS_ROBOLECTRIC)
}  // namespace

LogFatalCallback g_log_fatal_callback_for_testing = nullptr;
const char kUnableToGetStackTraceMessage[] =
    "Unable to retrieve Java caller stack trace as the exception handler is "
    "being re-entered";
const char kReetrantOutOfMemoryMessage[] =
    "While handling an uncaught Java exception, an OutOfMemoryError "
    "occurred.";
const char kReetrantExceptionMessage[] =
    "While handling an uncaught Java exception, another exception "
    "occurred.";
const char kUncaughtExceptionMessage[] =
    "Uncaught Java exception in native code. Please include the Java exception "
    "stack from the Android log in your crash report.";
const char kUncaughtExceptionHandlerFailedMessage[] =
    "Uncaught Java exception in native code and the Java uncaught exception "
    "handler did not terminate the process. Please include the Java exception "
    "stack from the Android log in your crash report.";
const char kOomInGetJavaExceptionInfoMessage[] =
    "Unable to obtain Java stack trace due to OutOfMemoryError";

void InitVM(JavaVM* vm) {
<<<<<<< HEAD
  jni_zero::InitVM(vm);
  jni_zero::SetExceptionHandler(CheckException);
  JNIEnv* env = jni_zero::AttachCurrentThread();
#if !BUILDFLAG(IS_ROBOLECTRIC)
  // Warm-up needed for GetClassFromSplit, must be called before we set the
  // resolver, since GetClassFromSplit won't work until after
  // PrepareClassLoaders has happened.
  PrepareClassLoaders(env);
  jni_zero::SetClassResolver(GetClassFromSplit);
#endif
  g_out_of_memory_error_class = static_cast<jclass>(
      env->NewGlobalRef(env->FindClass("java/lang/OutOfMemoryError")));
  DCHECK(g_out_of_memory_error_class);
=======
  DCHECK(!g_jvm || g_jvm == vm);
  g_jvm = vm;
}

bool IsVMInitialized() {
  return g_jvm != nullptr;
}

JavaVM* GetVM() {
  return g_jvm;
}

void InitGlobalClassLoader(JNIEnv* env) {
  DCHECK(g_class_loader == nullptr);

  ScopedJavaLocalRef<jclass> class_loader_clazz =
      GetClass(env, "java/lang/ClassLoader");
  CHECK(!ClearException(env));
  g_class_loader_load_class_method_id =
      env->GetMethodID(class_loader_clazz.obj(),
                       "loadClass",
                       "(Ljava/lang/String;)Ljava/lang/Class;");
  CHECK(!ClearException(env));

  // GetClassLoader() caches the reference, so we do not need to wrap it in a
  // smart pointer as well.
  g_class_loader = GetClassLoader(env);
}

ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env,
                                    const char* class_name,
                                    const char* split_name) {
  return GetClassInternal(env, class_name,
                          GetSplitClassLoader(env, split_name));
}

ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env, const char* class_name) {
  return GetClassInternal(env, class_name, g_class_loader);
}

// This is duplicated with LazyGetClass below because these are performance
// sensitive.
jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    const char* split_name,
                    std::atomic<jclass>* atomic_class_id) {
  const jclass value = atomic_class_id->load(std::memory_order_acquire);
  if (value)
    return value;
  ScopedJavaGlobalRef<jclass> clazz;
  clazz.Reset(GetClass(env, class_name, split_name));
  jclass cas_result = nullptr;
  if (atomic_class_id->compare_exchange_strong(cas_result, clazz.obj(),
                                               std::memory_order_acq_rel)) {
    // We intentionally leak the global ref since we now storing it as a raw
    // pointer in |atomic_class_id|.
    return clazz.Release();
  } else {
    return cas_result;
  }
}

// This is duplicated with LazyGetClass above because these are performance
// sensitive.
jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    std::atomic<jclass>* atomic_class_id) {
  const jclass value = atomic_class_id->load(std::memory_order_acquire);
  if (value)
    return value;
  ScopedJavaGlobalRef<jclass> clazz;
  clazz.Reset(GetClass(env, class_name));
  jclass cas_result = nullptr;
  if (atomic_class_id->compare_exchange_strong(cas_result, clazz.obj(),
                                               std::memory_order_acq_rel)) {
    // We intentionally leak the global ref since we now storing it as a raw
    // pointer in |atomic_class_id|.
    return clazz.Release();
  } else {
    return cas_result;
  }
}

template<MethodID::Type type>
jmethodID MethodID::Get(JNIEnv* env,
                        jclass clazz,
                        const char* method_name,
                        const char* jni_signature) {
  auto get_method_ptr = type == MethodID::TYPE_STATIC ?
      &JNIEnv::GetStaticMethodID :
      &JNIEnv::GetMethodID;
  jmethodID id = (env->*get_method_ptr)(clazz, method_name, jni_signature);
  if (base::android::ClearException(env) || !id) {
    LOG(FATAL) << "Failed to find " <<
        (type == TYPE_STATIC ? "static " : "") <<
        "method " << method_name << " " << jni_signature;
  }
  return id;
}

// If |atomic_method_id| set, it'll return immediately. Otherwise, it'll call
// into ::Get() above. If there's a race, it's ok since the values are the same
// (and the duplicated effort will happen only once).
template <MethodID::Type type>
jmethodID MethodID::LazyGet(JNIEnv* env,
                            jclass clazz,
                            const char* method_name,
                            const char* jni_signature,
                            std::atomic<jmethodID>* atomic_method_id) {
  const jmethodID value = atomic_method_id->load(std::memory_order_acquire);
  if (value)
    return value;
#if BUILDFLAG(IS_COBALT)
  jmethodID id;
  if (shouldAddCobaltPrefix()) {
    std::string holder = getRepackagedName(jni_signature);
    id = MethodID::Get<type>(env, clazz, method_name, holder.c_str());
  } else {
    id = MethodID::Get<type>(env, clazz, method_name, jni_signature);
  }
#else
  jmethodID id = MethodID::Get<type>(env, clazz, method_name, jni_signature);
#endif
  atomic_method_id->store(id, std::memory_order_release);
  return id;
}

// Various template instantiations.
template jmethodID MethodID::Get<MethodID::TYPE_STATIC>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature);

template jmethodID MethodID::Get<MethodID::TYPE_INSTANCE>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature);

template jmethodID MethodID::LazyGet<MethodID::TYPE_STATIC>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature, std::atomic<jmethodID>* atomic_method_id);

template jmethodID MethodID::LazyGet<MethodID::TYPE_INSTANCE>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature, std::atomic<jmethodID>* atomic_method_id);

bool HasException(JNIEnv* env) {
  return env->ExceptionCheck() != JNI_FALSE;
}

bool ClearException(JNIEnv* env) {
  if (!HasException(env))
    return false;
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
>>>>>>> 6c8079971a5 (CoAT changes to support Kimono build (#4506))
}

void CheckException(JNIEnv* env) {
  if (!jni_zero::HasException(env)) {
    return;
<<<<<<< HEAD
  }

  static thread_local bool g_reentering = false;
  if (g_reentering) {
    // We were handling an uncaught Java exception already, but one of the Java
    // methods we called below threw another exception. (This is unlikely to
    // happen, as we are careful to never throw from these methods, but we
    // can't rule it out entirely. E.g. an OutOfMemoryError when constructing
    // the jstring for the return value of
    // sanitizedStacktraceForUnhandledException().
    env->ExceptionDescribe();
    jthrowable raw_throwable = env->ExceptionOccurred();
    env->ExceptionClear();
    jclass clazz = env->GetObjectClass(raw_throwable);
    bool is_oom_error = env->IsSameObject(clazz, g_out_of_memory_error_class);
    env->Throw(raw_throwable);  // Ensure we don't re-enter Java.

    if (is_oom_error) {
      base::android::SetJavaException(kReetrantOutOfMemoryMessage);
      // Use different LOG(FATAL) statements to ensure unique stack traces.
      if (g_log_fatal_callback_for_testing) {
        g_log_fatal_callback_for_testing(kReetrantOutOfMemoryMessage);
      } else {
        LOG(FATAL) << kReetrantOutOfMemoryMessage;
      }
    } else {
      base::android::SetJavaException(kReetrantExceptionMessage);
      if (g_log_fatal_callback_for_testing) {
        g_log_fatal_callback_for_testing(kReetrantExceptionMessage);
      } else {
        LOG(FATAL) << kReetrantExceptionMessage;
      }
    }
    // Needed for tests, which do not terminate from LOG(FATAL).
    return;
  }
  g_reentering = true;

  // Log a message to ensure there is something in the log even if the rest of
  // this function goes horribly wrong, and also to provide a convenient marker
  // in the log for where Java exception crash information starts.
  LOG(ERROR) << "Crashing due to uncaught Java exception";

  const bool handle_exception_in_java =
      base::FeatureList::IsEnabled(kHandleExceptionsInJava);

  if (!handle_exception_in_java) {
    env->ExceptionDescribe();
  }

  // We cannot use `ScopedJavaLocalRef` directly because that ends up calling
  // env->GetObjectRefType() when DCHECK is on, and that call is not allowed
  // with a pending exception according to the JNI spec.
  jthrowable raw_throwable = env->ExceptionOccurred();
  // Now that we saved the reference to the throwable, clear the exception.
  //
  // We need to do this as early as possible to remove the risk that code below
  // might accidentally call back into Java, which is not allowed when `env`
  // has an exception set, per the JNI spec. (For example, LOG(FATAL) doesn't
  // work with a JNI exception set, because it calls
  // GetJavaStackTraceIfPresent()).
  env->ExceptionClear();
  // The reference returned by `ExceptionOccurred()` is a local reference.
  // `ExceptionClear()` merely removes the exception information from `env`;
  // it doesn't delete the reference, which is why this call is valid.
  auto throwable = ScopedJavaLocalRef<jthrowable>::Adopt(env, raw_throwable);

  if (!handle_exception_in_java) {
    base::android::SetJavaException(
        GetJavaExceptionInfo(env, throwable).c_str());
    if (g_log_fatal_callback_for_testing) {
      g_log_fatal_callback_for_testing(kUncaughtExceptionMessage);
    } else {
      LOG(FATAL) << kUncaughtExceptionMessage;
    }
    // Needed for tests, which do not terminate from LOG(FATAL).
    g_reentering = false;
    return;
  }

  // We don't need to call SetJavaException() in this branch because we
  // expect handleException() to eventually call JavaExceptionReporter through
  // the global uncaught exception handler.

  const std::string native_stack_trace = base::debug::StackTrace().ToString();
  LOG(ERROR) << "Native stack trace:" << std::endl << native_stack_trace;

  ScopedJavaLocalRef<jthrowable> secondary_exception =
      Java_JniAndroid_handleException(env, throwable, native_stack_trace);

  // Ideally handleException() should have terminated the process and we should
  // not get here. This can happen in the case of OutOfMemoryError or if the
  // app that embedded WebView installed an exception handler that does not
  // terminate, or itself threw an exception. We cannot be confident that
  // JavaExceptionReporter ran, so set the java exception explicitly.
  base::android::SetJavaException(
      GetJavaExceptionInfo(
          env, secondary_exception ? secondary_exception : throwable)
          .c_str());
  if (g_log_fatal_callback_for_testing) {
    g_log_fatal_callback_for_testing(kUncaughtExceptionHandlerFailedMessage);
  } else {
    LOG(FATAL) << kUncaughtExceptionHandlerFailedMessage;
  }
  // Needed for tests, which do not terminate from LOG(FATAL).
  g_reentering = false;
=======

#if BUILDFLAG(IS_COBALT)
  std::string exception_token;
#endif
  jthrowable java_throwable = env->ExceptionOccurred();
  if (java_throwable) {
    // Clear the pending exception, since a local reference is now held.
    env->ExceptionDescribe();
    env->ExceptionClear();

    if (g_fatal_exception_occurred) {
      // Another exception (probably OOM) occurred during GetJavaExceptionInfo.
      base::android::SetJavaException(
          "Java OOM'ed in exception handling, check logcat");
#if BUILDFLAG(IS_COBALT)
      exception_token = "Java OOM'ed";
#endif
    } else {
      g_fatal_exception_occurred = true;
#if BUILDFLAG(IS_COBALT)
      std::string exception_info = GetJavaExceptionInfo(env, java_throwable);
      base::android::SetJavaException(exception_info.c_str());
      exception_token = FindFirstJavaFileAndLine(exception_info);
#else
      // RVO should avoid any extra copies of the exception string.
      base::android::SetJavaException(
          GetJavaExceptionInfo(env, java_throwable).c_str());
#endif
    }
  }

  // Now, feel good about it and die.
#if BUILDFLAG(IS_COBALT)
  LOG(FATAL) << "JNI exception: " << exception_token;
#else
  LOG(FATAL) << "Please include Java exception stack in crash report";
#endif
>>>>>>> 502a632502b (Informative JNI crash message (#6337))
}

std::string GetJavaExceptionInfo(JNIEnv* env,
                                 const JavaRef<jthrowable>& throwable) {
  std::string sanitized_exception_string =
      Java_JniAndroid_sanitizedStacktraceForUnhandledException(env, throwable);
  // Returns null when PiiElider results in an OutOfMemoryError.
  return !sanitized_exception_string.empty()
             ? sanitized_exception_string
             : kOomInGetJavaExceptionInfoMessage;
}

<<<<<<< HEAD
<<<<<<< HEAD
std::string GetJavaStackTraceIfPresent() {
  JNIEnv* env = nullptr;
  JavaVM* jvm = jni_zero::GetVM();
  if (jvm) {
    jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_2);
  }
  if (!env) {
    // JNI has not been initialized on this thread.
    return {};
  }
=======
=======
#if BUILDFLAG(IS_COBALT)
>>>>>>> 649a0a7bbb9 (Add IS_COBALT macro for jni_android files (#6363))
std::string FindFirstJavaFileAndLine(const std::string& stack_trace) {
    // This regular expression looks for a pattern inside parentheses.
    // Breakdown of the pattern: \(([^)]+\.java:\d+)\)
    // \\(      - Matches the literal opening parenthesis '('. We need two backslashes in a C++ string literal.
    // (        - Starts a capturing group. This is the part of the match we want to extract.
    // [^)]+    - Matches one or more characters that are NOT a closing parenthesis ')'. This captures the file name.
    // \\.java: - Matches the literal text ".java:".
    // \\d+     - Matches one or more digits (the line number).
    // )        - Ends the capturing group.
    // \\)      - Matches the literal closing parenthesis ')'.
    std::regex pattern("\\(([^)]+\\.java:\\d+)\\)");

    // smatch object will store the results of the search.
    std::smatch match;

    // Search the input string for the first occurrence of the pattern.
    if (std::regex_search(stack_trace, match, pattern)) {
        // The full match is match[0] (e.g., "(CobaltActivity.java:219)").
        // The first captured group is match[1] (e.g., "CobaltActivity.java:219").
        // We return the content of the first captured group.
        return match[1].str();
    }

    // Return an empty string if no match was found.
    return "";
}
#endif

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
>>>>>>> 502a632502b (Informative JNI crash message (#6337))

  if (HasException(env)) {
    // This can happen if CheckException() is being re-entered, decided to
    // LOG(FATAL) immediately, and LOG(FATAL) itself is calling us. In that case
    // it is imperative that we don't try to call Java again.
    return kUnableToGetStackTraceMessage;
  }

  ScopedJavaLocalRef<jthrowable> throwable =
      JNI_Throwable::Java_Throwable_Constructor(env);
  std::string ret = GetJavaExceptionInfo(env, throwable);
  // Strip the exception message and leave only the "at" lines. Example:
  // java.lang.Throwable:
  // {tab}at Clazz.method(Clazz.java:111)
  // {tab}at ...
  size_t newline_idx = ret.find('\n');
  if (newline_idx == std::string::npos) {
    // There are no java frames.
    return {};
  }
  return ret.substr(newline_idx + 1);
}

}  // namespace android
}  // namespace base
