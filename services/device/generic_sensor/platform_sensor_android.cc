// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_android.h"

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "services/device/generic_sensor/jni_headers/PlatformSensor_jni.h"
#include "services/device/public/cpp/device_features.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;

namespace device {
namespace {
void StartSensorBlocking(base::android::ScopedJavaGlobalRef<jobject> j_object,
                         double frequency) {
  device::Java_PlatformSensor_startSensor2(AttachCurrentThread(), j_object,
                                           frequency);
}

void StopSensorBlocking(base::android::ScopedJavaGlobalRef<jobject> j_object) {
  device::Java_PlatformSensor_stopSensor(AttachCurrentThread(), j_object);
}
}  // namespace

// static
scoped_refptr<PlatformSensorAndroid> PlatformSensorAndroid::Create(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider,
    const JavaRef<jobject>& java_provider) {
  auto sensor = base::MakeRefCounted<PlatformSensorAndroid>(
      type, reading_buffer, provider);
  JNIEnv* env = AttachCurrentThread();
  sensor->j_object_.Reset(
      Java_PlatformSensor_create(env, java_provider, static_cast<jint>(type),
                                 reinterpret_cast<jlong>(sensor.get())));
  if (!sensor->j_object_) {
    return nullptr;
  }

  return sensor;
}

PlatformSensorAndroid::PlatformSensorAndroid(
    mojom::SensorType type,
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider)
    : PlatformSensor(type, reading_buffer, provider) {}

PlatformSensorAndroid::~PlatformSensorAndroid() {
  if (j_object_) {
    if (base::FeatureList::IsEnabled(features::kAsyncSensorCalls)) {
      StopSensor();
    }
    Java_PlatformSensor_sensorDestroyed(AttachCurrentThread(), j_object_);
  }
}

mojom::ReportingMode PlatformSensorAndroid::GetReportingMode() {
  JNIEnv* env = AttachCurrentThread();
  return static_cast<mojom::ReportingMode>(
      Java_PlatformSensor_getReportingMode(env, j_object_));
}

PlatformSensorConfiguration PlatformSensorAndroid::GetDefaultConfiguration() {
  JNIEnv* env = AttachCurrentThread();
  jdouble frequency =
      Java_PlatformSensor_getDefaultConfiguration(env, j_object_);
  return PlatformSensorConfiguration(frequency);
}

double PlatformSensorAndroid::GetMaximumSupportedFrequency() {
  JNIEnv* env = AttachCurrentThread();
  return Java_PlatformSensor_getMaximumSupportedFrequency(env, j_object_);
}

bool PlatformSensorAndroid::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  if (base::FeatureList::IsEnabled(features::kAsyncSensorCalls)) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StartSensorBlocking, j_object_,
                                  configuration.frequency()));
    return true;
  } else {
    return Java_PlatformSensor_startSensor(AttachCurrentThread(), j_object_,
                                           configuration.frequency());
  }
}

void PlatformSensorAndroid::StopSensor() {
  if (base::FeatureList::IsEnabled(features::kAsyncSensorCalls)) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&StopSensorBlocking, j_object_));
  } else {
    Java_PlatformSensor_stopSensor(AttachCurrentThread(), j_object_);
  }
}

bool PlatformSensorAndroid::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  JNIEnv* env = AttachCurrentThread();
  return Java_PlatformSensor_checkSensorConfiguration(
      env, j_object_, configuration.frequency());
}

void PlatformSensorAndroid::NotifyPlatformSensorError(
    JNIEnv*,
    const JavaRef<jobject>& caller) {
  PostTaskToMainSequence(
      FROM_HERE,
      base::BindOnce(&PlatformSensorAndroid::NotifySensorError, this));
}

void PlatformSensorAndroid::UpdatePlatformSensorReading(
    JNIEnv*,
    const base::android::JavaRef<jobject>& caller,
    jdouble timestamp,
    jdouble value1,
    jdouble value2,
    jdouble value3,
    jdouble value4) {
  SensorReading reading;
  reading.raw.timestamp = timestamp;
  reading.raw.values[0] = value1;
  reading.raw.values[1] = value2;
  reading.raw.values[2] = value3;
  reading.raw.values[3] = value4;

  UpdateSharedBufferAndNotifyClients(reading);
}

}  // namespace device
