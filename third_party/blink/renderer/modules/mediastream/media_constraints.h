/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_CONSTRAINTS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_CONSTRAINTS_H_

#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class MediaConstraintsPrivate;

class MODULES_EXPORT BaseConstraint {
 public:
  explicit BaseConstraint(const char* name);
  virtual ~BaseConstraint();

  bool IsPresent() const { return is_present_ || !IsUnconstrained(); }
  void SetIsPresent(bool is_present) { is_present_ = is_present; }

  // true if the Cconstraint has neither Mandatory (min/max/exact) not Ideal
  // values, false otherwise.
  virtual bool IsUnconstrained() const = 0;
  bool HasMandatory() const;
  virtual bool HasMin() const { return false; }
  virtual bool HasMax() const { return false; }
  virtual bool HasExact() const = 0;
  const char* GetName() const { return name_; }
  virtual String ToString() const = 0;

 private:
  const char* name_;
  bool is_present_ = false;
};

// Note this class refers to the "long" WebIDL definition which is
// equivalent to int32_t.
class MODULES_EXPORT LongConstraint : public BaseConstraint {
 public:
  explicit LongConstraint(const char* name);

  void SetMin(int32_t value) {
    min_ = value;
    has_min_ = true;
  }

  void SetMax(int32_t value) {
    max_ = value;
    has_max_ = true;
  }

  void SetExact(int32_t value) {
    exact_ = value;
    has_exact_ = true;
  }

  void SetIdeal(int32_t value) {
    ideal_ = value;
    has_ideal_ = true;
  }

  bool Matches(int32_t value) const;
  bool IsUnconstrained() const override;
  bool HasMin() const override { return has_min_; }
  bool HasMax() const override { return has_max_; }
  bool HasExact() const override { return has_exact_; }
  String ToString() const override;
  int32_t Min() const { return min_; }
  int32_t Max() const { return max_; }
  int32_t Exact() const { return exact_; }
  bool HasIdeal() const { return has_ideal_; }
  int32_t Ideal() const { return ideal_; }

 private:
  int32_t min_;
  int32_t max_;
  int32_t exact_;
  int32_t ideal_;
  unsigned has_min_ : 1;
  unsigned has_max_ : 1;
  unsigned has_exact_ : 1;
  unsigned has_ideal_ : 1;
};

class MODULES_EXPORT DoubleConstraint : public BaseConstraint {
 public:
  // Permit a certain leeway when comparing floats. The offset of 0.00001
  // is chosen based on observed behavior of doubles formatted with
  // rtc::ToString.
  static const double kConstraintEpsilon;

  explicit DoubleConstraint(const char* name);

  void SetMin(double value) {
    min_ = value;
    has_min_ = true;
  }

  void SetMax(double value) {
    max_ = value;
    has_max_ = true;
  }

  void SetExact(double value) {
    exact_ = value;
    has_exact_ = true;
  }

  void SetIdeal(double value) {
    ideal_ = value;
    has_ideal_ = true;
  }

  bool Matches(double value) const;
  bool IsUnconstrained() const override;
  bool HasMin() const override { return has_min_; }
  bool HasMax() const override { return has_max_; }
  bool HasExact() const override { return has_exact_; }
  String ToString() const override;
  double Min() const { return min_; }
  double Max() const { return max_; }
  double Exact() const { return exact_; }
  bool HasIdeal() const { return has_ideal_; }
  double Ideal() const { return ideal_; }

 private:
  double min_;
  double max_;
  double exact_;
  double ideal_;
  unsigned has_min_ : 1;
  unsigned has_max_ : 1;
  unsigned has_exact_ : 1;
  unsigned has_ideal_ : 1;
};

class MODULES_EXPORT StringConstraint : public BaseConstraint {
 public:
  // String-valued options don't have min or max, but can have multiple
  // values for ideal and exact.
  explicit StringConstraint(const char* name);

  void SetExact(const String& exact) { exact_ = {exact}; }

  void SetExact(const Vector<String>& exact) { exact_ = exact; }

  void SetIdeal(const String& ideal) { ideal_ = {ideal}; }

  void SetIdeal(const Vector<String>& ideal) { ideal_ = ideal; }

  bool Matches(String value) const;
  bool IsUnconstrained() const override;
  bool HasExact() const override { return !exact_.empty(); }
  String ToString() const override;
  bool HasIdeal() const { return !ideal_.empty(); }
  const Vector<String>& Exact() const;
  const Vector<String>& Ideal() const;

 private:
  Vector<String> exact_;
  Vector<String> ideal_;
};

class MODULES_EXPORT BooleanConstraint : public BaseConstraint {
 public:
  explicit BooleanConstraint(const char* name);

  bool Exact() const { return exact_; }
  bool Ideal() const { return ideal_; }
  void SetIdeal(bool value) {
    ideal_ = value;
    has_ideal_ = true;
  }

  void SetExact(bool value) {
    exact_ = value;
    has_exact_ = true;
  }

  bool Matches(bool value) const;
  bool IsUnconstrained() const override;
  bool HasExact() const override { return has_exact_; }
  String ToString() const override;
  bool HasIdeal() const { return has_ideal_; }

 private:
  unsigned ideal_ : 1;
  unsigned exact_ : 1;
  unsigned has_ideal_ : 1;
  unsigned has_exact_ : 1;
};

struct MediaTrackConstraintSetPlatform {
 public:
  MODULES_EXPORT MediaTrackConstraintSetPlatform();

  LongConstraint width;
  LongConstraint height;
  DoubleConstraint aspect_ratio;
  DoubleConstraint frame_rate;
  StringConstraint facing_mode;
  StringConstraint resize_mode;
  DoubleConstraint volume;
  LongConstraint sample_rate;
  LongConstraint sample_size;
  BooleanConstraint echo_cancellation;
  StringConstraint echo_cancellation_type;
  DoubleConstraint latency;
  LongConstraint channel_count;
  StringConstraint device_id;
  BooleanConstraint disable_local_echo;
  BooleanConstraint suppress_local_audio_playback;
  DoubleConstraint pan;
  DoubleConstraint tilt;
  DoubleConstraint zoom;
  StringConstraint group_id;
  StringConstraint display_surface;
  // Constraints not exposed in Blink at the moment, only through
  // the legacy name interface.
  StringConstraint media_stream_source;  // tab, screen, desktop, system
  BooleanConstraint render_to_associated_sink;
  BooleanConstraint goog_echo_cancellation;
  BooleanConstraint goog_experimental_echo_cancellation;
  BooleanConstraint goog_auto_gain_control;
  BooleanConstraint goog_noise_suppression;
  BooleanConstraint goog_highpass_filter;
  BooleanConstraint goog_experimental_noise_suppression;
  BooleanConstraint goog_audio_mirroring;
  BooleanConstraint goog_da_echo_cancellation;
  BooleanConstraint goog_noise_reduction;

  MODULES_EXPORT bool IsUnconstrained() const;
  MODULES_EXPORT bool HasMandatory() const;
  MODULES_EXPORT bool HasMandatoryOutsideSet(const Vector<String>&,
                                             String&) const;
  MODULES_EXPORT bool HasMin() const;
  MODULES_EXPORT bool HasExact() const;
  MODULES_EXPORT String ToString() const;

 private:
  Vector<const BaseConstraint*> AllConstraints() const;
};

class MediaConstraints {
 public:
  MediaConstraints() = default;
  MediaConstraints(const MediaConstraints& other) { Assign(other); }
  ~MediaConstraints() { Reset(); }

  MediaConstraints& operator=(const MediaConstraints& other) {
    Assign(other);
    return *this;
  }

  MODULES_EXPORT void Assign(const MediaConstraints&);

  MODULES_EXPORT void Reset();
  bool IsNull() const { return private_.IsNull(); }
  MODULES_EXPORT bool IsUnconstrained() const;

  MODULES_EXPORT void Initialize();
  MODULES_EXPORT void Initialize(
      const MediaTrackConstraintSetPlatform& basic,
      const Vector<MediaTrackConstraintSetPlatform>& advanced);

  MODULES_EXPORT const MediaTrackConstraintSetPlatform& Basic() const;
  MODULES_EXPORT const Vector<MediaTrackConstraintSetPlatform>& Advanced()
      const;

  MODULES_EXPORT const String ToString() const;

 private:
  WebPrivatePtr<MediaConstraintsPrivate> private_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_MEDIA_CONSTRAINTS_H_
