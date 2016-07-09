/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef COBALT_BASE_C_VAL_H_
#define COBALT_BASE_C_VAL_H_

#include <stdint.h>

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"

// The CVal system allows you to mark certain variables to be part of the
// CVal system and therefore analyzable and trackable by other systems.  All
// modifications to marked variables will trigger an event within a CValManager
// class, which will in turn trigger an event in all attached hooks (e.g. memory
// logging systems, tracing systems, etc...).

// Each marked variable is known as a CVal, and every CVal has an associated
// name so that it can be looked up at run time.  Thus, you can only ever
// have one CVal of the same name existing at a time.
//
// There are two variants of CVal, DebugCVal and PublicCVal. They are identical
// when DebugCVals are enabled; however, when DebugCVals are disabled,
// PublicCVal remains trackable through the CVal system, while DebugCVal stops
// being trackable and behaves as similarly as possible to its underlying type.

// You can mark a variable:
//
//   int memory_counter;
//
// as a CVal by wrapping the type with the CVal template type:
//
//   DebugCVal<int> memory_counter("Memory Counter", 0, "My memory counter");
//   PublicCVal<int> memory_counter("Memory Counter", 0, "My memory counter");
//
// The first constructor parameter is the name of the CVal, the second is the
// initial value for the variable, and the third is the description.
//
// For the system to function, the singleton CValManager class must be
// instantiated.  If a CVal is created before a CValManager is instantiated, it
// will attach itself to the CValManager the next time it is modified.  Thus,
// global CVal variables can be supported, but they will not be tracked until
// they are modified for the first time after the CValManager is created.  At
// this point, while CVals will be tracked, you will only be notified of changes
// to CVals.
//
// To hook in to the CValManager in order to receive CVal changed notifications,
// you should create a class that subclasses CValManager::OnChangedHook and
// implements the OnValueChanged virtual method.  When this class is
// instantiated, it will automatically hook in to the singleton CValManager
// object, which must exist at that point.
//
// NOTE: When the debug console is disabled, change events are never tracked.

namespace base {

namespace CValDetail {

class CValBase;

template <typename T>
class CValImpl;

}  // namespace CValDetail

// An enumeration to allow CVals to track the type that they hold in a run-time
// variable.
enum CValType {
  kU32,
  kU64,
  kS32,
  kS64,
  kFloat,
  kDouble,
  kString,
};

namespace CValDetail {

// Introduce a Traits class so that we can convert from C++ type to
// CValType.
template <typename T>
struct Traits {
  // If you get a compiler error here, you must add a Traits class specific to
  // the variable type you would like to support.
  int UnsupportedCValType[0];
};
template <>
struct Traits<uint32_t> {
  static const CValType kTypeVal = kU32;
  static const bool kIsNumerical = true;
};
template <>
struct Traits<uint64_t> {
  static const CValType kTypeVal = kU64;
  static const bool kIsNumerical = true;
};
template <>
struct Traits<int32_t> {
  static const CValType kTypeVal = kS32;
  static const bool kIsNumerical = true;
};
template <>
struct Traits<int64_t> {
  static const CValType kTypeVal = kS64;
  static const bool kIsNumerical = true;
};
template <>
struct Traits<float> {
  static const CValType kTypeVal = kFloat;
  static const bool kIsNumerical = true;
};
template <>
struct Traits<double> {
  static const CValType kTypeVal = kDouble;
  static const bool kIsNumerical = true;
};
template <>
struct Traits<std::string> {
  static const CValType kTypeVal = kString;
  static const bool kIsNumerical = false;
};

// Provide methods to convert from an arbitrary type to a string, useful for
// systems that want to read the value of a CVal without caring about its type.
template <typename T>
std::string ValToString(const T& value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

template <>
inline std::string ValToString<std::string>(const std::string& value) {
  return value;
}

// Helper function to implement the numerical branch of ValToPrettyString
template <typename T>
std::string NumericalValToPrettyString(const T& value) {
  struct {
    T threshold;
    T divide_by;
    const char* postfix;
  } thresholds[] = {
      {static_cast<T>(10 * 1024 * 1024), static_cast<T>(1024 * 1024), "MB"},
      {static_cast<T>(10 * 1024), static_cast<T>(1024), "KB"},
  };

  T divided_value = value;
  const char* postfix = "";

  for (size_t i = 0; i < sizeof(thresholds) / sizeof(thresholds[0]); ++i) {
    if (value >= thresholds[i].threshold) {
      divided_value = value / thresholds[i].divide_by;
      postfix = thresholds[i].postfix;
      break;
    }
  }

  std::ostringstream oss;
  oss << divided_value << postfix;
  return oss.str();
}

// Helper function for the subsequent ValToPrettyString function.
template <typename T, bool IsNumerical>
class ValToPrettyStringHelper {};
template <typename T>
class ValToPrettyStringHelper<T, true> {
 public:
  static std::string Call(const T& value) {
    return NumericalValToPrettyString(value);
  }
};
template <typename T>
class ValToPrettyStringHelper<T, false> {
 public:
  static std::string Call(const T& value) { return ValToString(value); }
};

// As opposed to ValToString, here we take the opportunity to clean up the
// number a bit to make it easier for humans to digest.  For example, if a
// number is much larger than one million, we can divide it by one million
// and postfix it with an 'M'.
template <typename T>
std::string ValToPrettyString(const T& value) {
  return ValToPrettyStringHelper<T, Traits<T>::kIsNumerical>::Call(value);
}

}  // namespace CValDetail

#if defined(ENABLE_DEBUG_CONSOLE)
// This class is passed back to the CVal modified event handlers so that they
// can examine the new CVal value.  This class knows its type, which can be
// checked through GetType, and then the actual value can be retrieved by
// calling CValGenericValue::AsNativeType<TYPE>() with the correct type.  If you
// don't care about the type, you can call CValGenericValue::AsString() and
// retrieve the string version of the value.
class CValGenericValue {
 public:
  CValType GetType() const { return type_; }

  // Return the value casted to the specified type.  The requested type must
  // match the contained value's actual native type.
  template <typename T>
  const T& AsNativeType() const {
    DCHECK(IsNativeType<T>());
    return *static_cast<T*>(generic_value_);
  }

  // Returns true if the type of this value is exactly T.
  template <typename T>
  bool IsNativeType() const {
    return type_ == CValDetail::Traits<T>::kTypeVal;
  }

  virtual std::string AsString() const = 0;
  virtual std::string AsPrettyString() const = 0;

 protected:
  CValGenericValue(CValType type, void* value_mem)
      : type_(type), generic_value_(value_mem) {}
  virtual ~CValGenericValue() {}

 private:
  CValType type_;
  void* generic_value_;
};

namespace CValDetail {

// Internally used to store the actual typed value that a CValGenericValue may
// contain.  This value actually stores the data as a copy and then passes a
// pointer (as a void*) to its parent class CValGenericValue so that the value
// can be accessed from the parent class.  This class is intended to snapshot a
// CVal value before passing it along to event handlers ensuring that they all
// receive the same value.
template <typename T>
class CValSpecificValue : public CValGenericValue {
 public:
  explicit CValSpecificValue(const T& value)
      : CValGenericValue(Traits<T>::kTypeVal, &value_), value_(value) {}
  CValSpecificValue(const CValSpecificValue<T>& other)
      : CValGenericValue(Traits<T>::kTypeVal, &value_), value_(other.value_) {}
  virtual ~CValSpecificValue() {}

  std::string AsString() const { return ValToString(value_); }

  std::string AsPrettyString() const { return ValToPrettyString(value_); }

 private:
  T value_;
};

}  // namespace CValDetail
#endif  // ENABLE_DEBUG_CONSOLE

// Manager class required for the CVal tracking system to function.
// This class is designed to be a singleton, instanced only through the methods
// of the base::Singleton class. For example,
// CValManager::GetInstance()->GetValueAsPrettyString(...)
class CValManager {
 public:
  // Method to get the singleton instance of this class.
  static CValManager* GetInstance();

#if defined(ENABLE_DEBUG_CONSOLE)
  // In order for a system to receive notifications when a tracked CVal changes,
  // it should create a subclass of OnChangedHook with OnValueChanged. When
  // instantiated, OnChangedHook will be called whenever a CVal changes.
  class OnChangedHook {
   public:
    OnChangedHook();
    virtual ~OnChangedHook();

    virtual void OnValueChanged(const std::string& name,
                                const CValGenericValue& value) = 0;
  };
#endif  // ENABLE_DEBUG_CONSOLE

  friend struct StaticMemorySingletonTraits<CValManager>;

  // Returns a set of the sorted names of all CVals currently registered.
  std::set<std::string> GetOrderedCValNames();

  // Returns the value of the given CVal as a string.  The return value is a
  // pair, the first element is true if the CVal exists, in which case the
  // second element is the value (as a string).  If the CVal does not exist,
  // the value of the second element is undefined.
  optional<std::string> GetValueAsString(const std::string& name);

  // Similar to the above, but formatting may be done on numerical values.
  // For example, large numbers may have metric postfixes appended to them.
  // i.e. 104857600 = 100M
  optional<std::string> GetValueAsPrettyString(const std::string& name);

 private:
  // Class can only be instanced via the singleton
  CValManager();
  ~CValManager();

  // Called in CVal constructors to register/deregister themselves with the
  // system.
  void RegisterCVal(const CValDetail::CValBase* cval);
  void UnregisterCVal(const CValDetail::CValBase* cval);

#if defined(ENABLE_DEBUG_CONSOLE)
  // Called whenever a CVal is modified, and does the work of notifying all
  // hooks.
  void PushValueChangedEvent(const CValDetail::CValBase* cval,
                             const CValGenericValue& value);
#endif  // ENABLE_DEBUG_CONSOLE

  // Helper function to remove code duplication between GetValueAsString
  // and GetValueAsPrettyString.
  optional<std::string> GetCValStringValue(const std::string& name,
                                           bool pretty);

#if defined(ENABLE_DEBUG_CONSOLE)
  // Lock that protects against changes to hooks.
  base::Lock hooks_lock_;
#endif  // ENABLE_DEBUG_CONSOLE

  // Lock that protects against CVals being registered/deregistered.
  base::Lock cvals_lock_;

  // Lock that synchronizes |value_| reads/writes.
  base::Lock values_lock_;

  // The actual value registry, mapping CVal name to actual CVal object.
  typedef base::hash_map<std::string, const CValDetail::CValBase*> NameVarMap;
  NameVarMap* registered_vars_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // The set of hooks that we should notify whenever a CVal is modified.
  typedef base::hash_set<OnChangedHook*> OnChangeHookSet;
  OnChangeHookSet* on_changed_hook_set_;
#endif  // ENABLE_DEBUG_CONSOLE

  template <typename T>
  friend class CValDetail::CValImpl;

  DISALLOW_COPY_AND_ASSIGN(CValManager);
};

namespace CValDetail {

class CValBase {
 public:
  CValBase(const std::string& name, CValType type,
           const std::string& description)
      : name_(name), description_(description), type_(type) {}

  const std::string& GetName() const { return name_; }
  const std::string& GetDescription() const { return description_; }
  CValType GetType() const { return type_; }

  virtual std::string GetValueAsString() const = 0;
  virtual std::string GetValueAsPrettyString() const = 0;

 private:
  std::string name_;
  std::string description_;
  CValType type_;
};

// This is a wrapper class that marks that we wish to track a value through
// the CVal system.
template <typename T>
class CValImpl : public CValBase {
 public:
  CValImpl(const std::string& name, const T& initial_value,
           const std::string& description)
      : CValBase(name, Traits<T>::kTypeVal, description),
        value_(initial_value) {
    CommonConstructor();
  }
  CValImpl(const std::string& name, const std::string& description)
      : CValBase(name, Traits<T>::kTypeVal, description) {
    CommonConstructor();
  }
  virtual ~CValImpl() {
    if (registered_) {
      CValManager::GetInstance()->UnregisterCVal(this);
    }
  }

  operator T() const {
    base::AutoLock auto_lock(CValManager::GetInstance()->values_lock_);
    return value_;
  }

  const CValImpl<T>& operator=(const T& rhs) {
    bool value_changed;
    {
      base::AutoLock auto_lock(CValManager::GetInstance()->values_lock_);
      value_changed = value_ != rhs;
      if (value_changed) {
        value_ = rhs;
      }
    }
#if defined(ENABLE_DEBUG_CONSOLE)
    if (value_changed) {
      OnValueChanged();
    }
#endif  // ENABLE_DEBUG_CONSOLE
    return *this;
  }

  const CValImpl<T>& operator+=(const T& rhs) {
    {
      base::AutoLock auto_lock(CValManager::GetInstance()->values_lock_);
      value_ += rhs;
    }
#if defined(ENABLE_DEBUG_CONSOLE)
    OnValueChanged();
#endif  // ENABLE_DEBUG_CONSOLE
    return *this;
  }

  const CValImpl<T>& operator-=(const T& rhs) {
    {
      base::AutoLock auto_lock(CValManager::GetInstance()->values_lock_);
      value_ -= rhs;
    }
#if defined(ENABLE_DEBUG_CONSOLE)
    OnValueChanged();
#endif  // ENABLE_DEBUG_CONSOLE
    return *this;
  }

  const CValImpl<T>& operator++() {
    {
      base::AutoLock auto_lock(CValManager::GetInstance()->values_lock_);
      ++value_;
    }
#if defined(ENABLE_DEBUG_CONSOLE)
    OnValueChanged();
#endif  // ENABLE_DEBUG_CONSOLE
    return *this;
  }

  const CValImpl<T>& operator--() {
    {
      base::AutoLock auto_lock(CValManager::GetInstance()->values_lock_);
      --value_;
    }
#if defined(ENABLE_DEBUG_CONSOLE)
    OnValueChanged();
#endif  // ENABLE_DEBUG_CONSOLE
    return *this;
  }

  std::string GetValueAsString() const {
    // Can be called to get the value of a CVal without knowing the type first.
    base::AutoLock auto_lock(CValManager::GetInstance()->values_lock_);
    return ValToString<T>(value_);
  }

  std::string GetValueAsPrettyString() const {
    // Similar to GetValueAsString(), but it will also format the string to
    // do things like make very large numbers more readable.
    base::AutoLock auto_lock(CValManager::GetInstance()->values_lock_);
    return ValToPrettyString<T>(value_);
  }

 private:
  void CommonConstructor() {
    registered_ = false;
    RegisterWithManager();
  }

  void RegisterWithManager() {
    if (!registered_) {
      CValManager::GetInstance()->RegisterCVal(this);
      registered_ = true;
    }
  }

#if defined(ENABLE_DEBUG_CONSOLE)
  void OnValueChanged() {
    // Push the value changed event to all listeners.
    CValManager::GetInstance()->PushValueChangedEvent(
        this, CValSpecificValue<T>(value_));
  }
#endif  // ENABLE_DEBUG_CONSOLE

  T value_;
  mutable bool registered_;
};

#if !defined(ENABLE_DEBUG_C_VAL)
// This is a wrapper class that mimics the behavior of the underlying type. It
// does not register with the |CValManager| and does not push value changed
// events.
template <typename T>
class CValStub {
 public:
  CValStub(const std::string& name, const T& initial_value,
           const std::string& description)
      : value_(initial_value) {
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(description);
  }
  CValStub(const std::string& name, const std::string& description) {
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(description);
  }

  operator T() const { return value_; }

  const CValStub<T>& operator=(const T& rhs) {
    value_ = rhs;
    return *this;
  }
  const CValStub<T>& operator+=(const T& rhs) {
    value_ += rhs;
    return *this;
  }
  const CValStub<T>& operator-=(const T& rhs) {
    value_ -= rhs;
    return *this;
  }

  const CValStub<T>& operator++() {
    ++value_;
    return *this;
  }

  const CValStub<T>& operator--() {
    --value_;
    return *this;
  }

 private:
  T value_;
};
#endif  // ENABLE_DEBUG_C_VAL

}  // namespace CValDetail

template <typename T>
#if defined(ENABLE_DEBUG_C_VAL)
// When DebugCVal is enabled, DebugCVals are tracked through the CVal system.
class DebugCVal : public CValDetail::CValImpl<T> {
  typedef CValDetail::CValImpl<T> CValParent;
#else   // ENABLE_DEBUG_C_VAL
// When DebugCVal is not enabled, DebugCVals are not tracked though the CVal
// system, and will behave as similarly as possible to their underlying types.
class DebugCVal : public CValDetail::CValStub<T> {
  typedef CValDetail::CValStub<T> CValParent;
#endif  // ENABLE_DEBUG_C_VAL

 public:
  DebugCVal(const std::string& name, const T& initial_value,
            const std::string& description)
      : CValParent(name, initial_value, description) {}

  DebugCVal(const std::string& name, const std::string& description)
      : CValParent(name, description) {}

  const DebugCVal& operator=(const T& rhs) {
    CValParent::operator=(rhs);
    return *this;
  }
};

// PublicCVals are always tracked though the CVal system, regardless of the
// DebugCVal state.
template <typename T>
class PublicCVal : public CValDetail::CValImpl<T> {
  typedef CValDetail::CValImpl<T> CValParent;

 public:
  PublicCVal(const std::string& name, const T& initial_value,
             const std::string& description)
      : CValParent(name, initial_value, description) {}

  PublicCVal(const std::string& name, const std::string& description)
      : CValParent(name, description) {}

  const PublicCVal& operator=(const T& rhs) {
    CValParent::operator=(rhs);
    return *this;
  }
};

}  // namespace base

#endif  // COBALT_BASE_C_VAL_H_
