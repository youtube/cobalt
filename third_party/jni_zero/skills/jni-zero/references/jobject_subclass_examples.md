# JNI Zero Refactoring Examples

## 1. Instance Methods

**Before:**

```cpp
void TokenAndroid::FromJavaToken(JNIEnv* env, const JavaRef<jobject>& j_token) {
  uint64_t high = Java_TokenBase_getHighForSerialization(env, j_token);
}
```

**After:**

```cpp
// In .h
static base::Token FromJavaToken(JNIEnv* env, const JavaRef<JTokenBase>& j_token);

// In .cc
...
base::Token TokenAndroid::FromJavaToken(JNIEnv* env, const JavaRef<JTokenBase>& j_token) {
  uint64_t high = static_cast<uint64_t>(j_token->getHighForSerialization(env));
}
```

## 2. Static Methods

**Before:**

```cpp
void ApplicationStatusListener::NotifyApplicationStateChange(ApplicationState state) {
  Java_ApplicationStatus_registerThreadSafeNativeApplicationStateListener(AttachCurrentThread());
}
```

**After:**

```cpp
// In .cc
...
void ApplicationStatusListener::NotifyApplicationStateChange(ApplicationState state) {
  JApplicationStatusClass::registerThreadSafeNativeApplicationStateListener(AttachCurrentThread());
}
```

## 3. System Classes (e.g., ParcelFileDescriptor)

**Before:**

```cpp
int ContentUriGetFd(const JavaRef<jobject>& java_parcel_file_descriptor) {
  int fd = Java_ContentUriUtils_getFd(env, java_parcel_file_descriptor);
}
```

**After:**

```cpp
// In .cc
...
int ContentUriGetFd(const JavaRef<jobject>& java_parcel_file_descriptor) {
  int fd = JContentUriUtilsClass::getFd(env, java_parcel_file_descriptor);
}
```

## 4. Converting ScopedJavaLocalRef<jobject>()

**Before:**

```cpp
return ScopedJavaLocalRef<jobject>();
```

**After:**

```cpp
return nullptr;
```

## 5. Replacing Unsafe Manual JNI Get method call with Safe Helpers

**Before:**

```cpp
const void* MediaDrmBridge::GetMetrics(int* size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_metrics = Java_MediaDrmBridge_getMetrics(env, j_media_drm_bridge_);
  
  // Unsafe: Manual pointer retrieval
  jbyte* metrics_elements = env->GetByteArrayElements(j_metrics.obj(), nullptr);
  jsize metrics_size = base::android::SafeGetArrayLength(env, j_metrics);
  SB_DCHECK(metrics_elements);

  metrics_.assign(metrics_elements, metrics_elements + metrics_size);

  // Unsafe: Must remember to release, otherwise it leaks!
  env->ReleaseByteArrayElements(j_metrics.obj(), metrics_elements, JNI_ABORT);
  
  *size = static_cast<int>(metrics_.size());
  return metrics_.data();
}
```

**After:**

```cpp
const void* MediaDrmBridge::GetMetrics(int* size) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> j_metrics = Java_MediaDrmBridge_getMetrics(env, j_media_drm_bridge_);
  
  // Safe: Automatically handles lifetime, copying, and release
  base::android::JavaByteArrayToByteVector(env, j_metrics, &metrics_);
  
  *size = static_cast<int>(metrics_.size());
  return metrics_.data();
}
```

## 6. Replacing Unsafe Manual JNI Creation (New*) with Safe Helpers

**Before:**

```cpp
ScopedJavaLocalRef<jbyteArray> ToScopedJavaByteArray(JNIEnv* env, std::string_view data) {
  // Unsafe: Raw NewByteArray call returns a raw local ref that can leak
  jbyteArray j_array = env->NewByteArray(data.size());
  if (!j_array) {
    return nullptr;
  }
  
  // Unsafe: Raw SetByteArrayRegion call
  env->SetByteArrayRegion(j_array, 0, data.size(), reinterpret_cast<const jbyte*>(data.data()));
  
  // Must remember to wrap it before returning, but any early return above would have leaked j_array!
  return ScopedJavaLocalRef<jbyteArray>(env, j_array);
}
```

**After:**

```cpp
ScopedJavaLocalRef<jbyteArray> ToScopedJavaByteArray(JNIEnv* env, std::string_view data) {
  // Safe: ToJavaByteArray handles NewByteArray, SetByteArrayRegion, and ScopedJavaLocalRef wrapping in one shot
  return base::android::ToJavaByteArray(env, data);
}
```