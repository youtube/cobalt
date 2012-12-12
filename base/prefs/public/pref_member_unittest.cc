// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/public/pref_member.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

const char kBoolPref[] = "bool";
const char kIntPref[] = "int";
const char kDoublePref[] = "double";
const char kStringPref[] = "string";
const char kStringListPref[] = "string_list";

void RegisterTestPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(kBoolPref, false, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterIntegerPref(kIntPref, 0, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(kDoublePref, 0.0, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(kStringPref,
                            "default",
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterListPref(kStringListPref,
                          new ListValue(),
                          PrefService::UNSYNCABLE_PREF);
}

class GetPrefValueCallback
    : public base::RefCountedThreadSafe<GetPrefValueCallback> {
 public:
  GetPrefValueCallback() : value_(false) {}

  void Init(const char* pref_name, PrefService* prefs) {
    pref_.Init(pref_name, prefs);
    pref_.MoveToThread(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }

  void Destroy() {
    pref_.Destroy();
  }

  bool FetchValue() {
    if (!BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&GetPrefValueCallback::GetPrefValueOnIOThread, this))) {
      return false;
    }
    MessageLoop::current()->Run();
    return true;
  }

  bool value() { return value_; }

 private:
  friend class base::RefCountedThreadSafe<GetPrefValueCallback>;
  ~GetPrefValueCallback() {}

  void GetPrefValueOnIOThread() {
    value_ = pref_.GetValue();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            MessageLoop::QuitClosure());
  }

  BooleanPrefMember pref_;
  bool value_;
};

class PrefMemberTestClass {
 public:
  explicit PrefMemberTestClass(PrefService* prefs)
      : observe_cnt_(0), prefs_(prefs) {
    str_.Init(kStringPref, prefs,
              base::Bind(&PrefMemberTestClass::OnPreferenceChanged,
                         base::Unretained(this)));
  }

  void OnPreferenceChanged(const std::string& pref_name) {
    EXPECT_EQ(pref_name, kStringPref);
    EXPECT_EQ(str_.GetValue(), prefs_->GetString(kStringPref));
    ++observe_cnt_;
  }

  StringPrefMember str_;
  int observe_cnt_;

 private:
  PrefService* prefs_;
};

}  // anonymous namespace

TEST(PrefMemberTest, BasicGetAndSet) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);

  // Test bool
  BooleanPrefMember boolean;
  boolean.Init(kBoolPref, &prefs);

  // Check the defaults
  EXPECT_FALSE(prefs.GetBoolean(kBoolPref));
  EXPECT_FALSE(boolean.GetValue());
  EXPECT_FALSE(*boolean);

  // Try changing through the member variable.
  boolean.SetValue(true);
  EXPECT_TRUE(boolean.GetValue());
  EXPECT_TRUE(prefs.GetBoolean(kBoolPref));
  EXPECT_TRUE(*boolean);

  // Try changing back through the pref.
  prefs.SetBoolean(kBoolPref, false);
  EXPECT_FALSE(prefs.GetBoolean(kBoolPref));
  EXPECT_FALSE(boolean.GetValue());
  EXPECT_FALSE(*boolean);

  // Test int
  IntegerPrefMember integer;
  integer.Init(kIntPref, &prefs);

  // Check the defaults
  EXPECT_EQ(0, prefs.GetInteger(kIntPref));
  EXPECT_EQ(0, integer.GetValue());
  EXPECT_EQ(0, *integer);

  // Try changing through the member variable.
  integer.SetValue(5);
  EXPECT_EQ(5, integer.GetValue());
  EXPECT_EQ(5, prefs.GetInteger(kIntPref));
  EXPECT_EQ(5, *integer);

  // Try changing back through the pref.
  prefs.SetInteger(kIntPref, 2);
  EXPECT_EQ(2, prefs.GetInteger(kIntPref));
  EXPECT_EQ(2, integer.GetValue());
  EXPECT_EQ(2, *integer);

  // Test double
  DoublePrefMember double_member;
  double_member.Init(kDoublePref, &prefs);

  // Check the defaults
  EXPECT_EQ(0.0, prefs.GetDouble(kDoublePref));
  EXPECT_EQ(0.0, double_member.GetValue());
  EXPECT_EQ(0.0, *double_member);

  // Try changing through the member variable.
  double_member.SetValue(1.0);
  EXPECT_EQ(1.0, double_member.GetValue());
  EXPECT_EQ(1.0, prefs.GetDouble(kDoublePref));
  EXPECT_EQ(1.0, *double_member);

  // Try changing back through the pref.
  prefs.SetDouble(kDoublePref, 3.0);
  EXPECT_EQ(3.0, prefs.GetDouble(kDoublePref));
  EXPECT_EQ(3.0, double_member.GetValue());
  EXPECT_EQ(3.0, *double_member);

  // Test string
  StringPrefMember string;
  string.Init(kStringPref, &prefs);

  // Check the defaults
  EXPECT_EQ("default", prefs.GetString(kStringPref));
  EXPECT_EQ("default", string.GetValue());
  EXPECT_EQ("default", *string);

  // Try changing through the member variable.
  string.SetValue("foo");
  EXPECT_EQ("foo", string.GetValue());
  EXPECT_EQ("foo", prefs.GetString(kStringPref));
  EXPECT_EQ("foo", *string);

  // Try changing back through the pref.
  prefs.SetString(kStringPref, "bar");
  EXPECT_EQ("bar", prefs.GetString(kStringPref));
  EXPECT_EQ("bar", string.GetValue());
  EXPECT_EQ("bar", *string);

  // Test string list
  ListValue expected_list;
  std::vector<std::string> expected_vector;
  StringListPrefMember string_list;
  string_list.Init(kStringListPref, &prefs);

  // Check the defaults
  EXPECT_TRUE(expected_list.Equals(prefs.GetList(kStringListPref)));
  EXPECT_EQ(expected_vector, string_list.GetValue());
  EXPECT_EQ(expected_vector, *string_list);

  // Try changing through the pref member.
  expected_list.AppendString("foo");
  expected_vector.push_back("foo");
  string_list.SetValue(expected_vector);

  EXPECT_TRUE(expected_list.Equals(prefs.GetList(kStringListPref)));
  EXPECT_EQ(expected_vector, string_list.GetValue());
  EXPECT_EQ(expected_vector, *string_list);

  // Try adding through the pref.
  expected_list.AppendString("bar");
  expected_vector.push_back("bar");
  prefs.Set(kStringListPref, expected_list);

  EXPECT_TRUE(expected_list.Equals(prefs.GetList(kStringListPref)));
  EXPECT_EQ(expected_vector, string_list.GetValue());
  EXPECT_EQ(expected_vector, *string_list);

  // Try removing through the pref.
  expected_list.Remove(0, NULL);
  expected_vector.erase(expected_vector.begin());
  prefs.Set(kStringListPref, expected_list);

  EXPECT_TRUE(expected_list.Equals(prefs.GetList(kStringListPref)));
  EXPECT_EQ(expected_vector, string_list.GetValue());
  EXPECT_EQ(expected_vector, *string_list);
}

TEST(PrefMemberTest, InvalidList) {
  // Set the vector to an initial good value.
  std::vector<std::string> expected_vector;
  expected_vector.push_back("foo");

  // Try to add a valid list first.
  ListValue list;
  list.AppendString("foo");
  std::vector<std::string> vector;
  EXPECT_TRUE(subtle::PrefMemberVectorStringUpdate(list, &vector));
  EXPECT_EQ(expected_vector, vector);

  // Now try to add an invalid list.  |vector| should not be changed.
  list.AppendInteger(0);
  EXPECT_FALSE(subtle::PrefMemberVectorStringUpdate(list, &vector));
  EXPECT_EQ(expected_vector, vector);
}

TEST(PrefMemberTest, TwoPrefs) {
  // Make sure two DoublePrefMembers stay in sync.
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);

  DoublePrefMember pref1;
  pref1.Init(kDoublePref, &prefs);
  DoublePrefMember pref2;
  pref2.Init(kDoublePref, &prefs);

  pref1.SetValue(2.3);
  EXPECT_EQ(2.3, *pref2);

  pref2.SetValue(3.5);
  EXPECT_EQ(3.5, *pref1);

  prefs.SetDouble(kDoublePref, 4.2);
  EXPECT_EQ(4.2, *pref1);
  EXPECT_EQ(4.2, *pref2);
}

TEST(PrefMemberTest, Observer) {
  TestingPrefService prefs;
  RegisterTestPrefs(&prefs);

  PrefMemberTestClass test_obj(&prefs);
  EXPECT_EQ("default", *test_obj.str_);

  // Calling SetValue should not fire the observer.
  test_obj.str_.SetValue("hello");
  EXPECT_EQ(0, test_obj.observe_cnt_);
  EXPECT_EQ("hello", prefs.GetString(kStringPref));

  // Changing the pref does fire the observer.
  prefs.SetString(kStringPref, "world");
  EXPECT_EQ(1, test_obj.observe_cnt_);
  EXPECT_EQ("world", *(test_obj.str_));

  // Not changing the value should not fire the observer.
  prefs.SetString(kStringPref, "world");
  EXPECT_EQ(1, test_obj.observe_cnt_);
  EXPECT_EQ("world", *(test_obj.str_));

  prefs.SetString(kStringPref, "hello");
  EXPECT_EQ(2, test_obj.observe_cnt_);
  EXPECT_EQ("hello", prefs.GetString(kStringPref));
}

TEST(PrefMemberTest, NoInit) {
  // Make sure not calling Init on a PrefMember doesn't cause problems.
  IntegerPrefMember pref;
}

TEST(PrefMemberTest, MoveToThread) {
  TestingPrefService prefs;
  scoped_refptr<GetPrefValueCallback> callback =
      make_scoped_refptr(new GetPrefValueCallback());
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);
  content::TestBrowserThread io_thread(BrowserThread::IO);
  ASSERT_TRUE(io_thread.Start());
  RegisterTestPrefs(&prefs);
  callback->Init(kBoolPref, &prefs);

  ASSERT_TRUE(callback->FetchValue());
  EXPECT_FALSE(callback->value());

  prefs.SetBoolean(kBoolPref, true);

  ASSERT_TRUE(callback->FetchValue());
  EXPECT_TRUE(callback->value());

  callback->Destroy();

  ASSERT_TRUE(callback->FetchValue());
  EXPECT_TRUE(callback->value());
}
