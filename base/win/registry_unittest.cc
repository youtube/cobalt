// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/registry.h"

#include <windows.h>

#include <shlobj.h>
#include <stdint.h>

#include <cstring>
#include <iterator>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/simple_thread.h"
#include "base/win/windows_version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

namespace {

constexpr wchar_t kRootKey[] = L"Base_Registry_Unittest";

// A test harness for registry tests that operate in HKCU. Each test is given
// a valid key distinct from that used by other tests.
template <typename Traits>
class RegistryTest : public testing::Test {
 protected:
  RegistryTest() : root_key_(std::wstring(L"Software\\") + kRootKey) {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));

    // Create the test's root key.
    typename Traits::RegType key(
        Traits::Create(HKEY_CURRENT_USER, L"", KEY_CREATE_SUB_KEY));
    ASSERT_NE(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER, root_key().c_str(), KEY_READ));
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(HKEY_CURRENT_USER, root_key().c_str(), KEY_READ));
  }

  // Returns the path to a key under HKCU that is made available for exclusive
  // use by a test.
  const std::wstring& root_key() const { return root_key_; }

 private:
  registry_util::RegistryOverrideManager registry_override_;
  const std::wstring root_key_;
};

}  // namespace

namespace internal {

template <typename T>
class RegTestTraits {
 public:
  using RegType = T;

  static T Create() { return T(); }

  static T Create(HKEY rootkey, const wchar_t* subkey, REGSAM access) {
    return T(rootkey, subkey, access);
  }
};

}  // namespace internal

namespace {

class RegistryTypeNames {
 public:
  template <typename T>
  static std::string GetName(int index) {
    if (std::is_same<typename T::RegType, RegKey>()) {
      return "RegKey";
    }
    if (std::is_same<typename T::RegType, ExportDerivedRegKey>()) {
      return "ExportDerivedRegKey";
    }
  }
};

}  // namespace

using RegistryTypes =
    ::testing::Types<internal::RegTestTraits<RegKey>,
                     internal::RegTestTraits<ExportDerivedRegKey>>;
TYPED_TEST_SUITE(RegistryTest, RegistryTypes, RegistryTypeNames);

TYPED_TEST(RegistryTest, ValueTest) {
  typename TypeParam::RegType key(TypeParam::Create());

  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, this->root_key().c_str(),
                                    KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key.Valid());

  const wchar_t kStringValueName[] = L"StringValue";
  const wchar_t kDWORDValueName[] = L"DWORDValue";
  const wchar_t kInt64ValueName[] = L"Int64Value";
  const wchar_t kStringData[] = L"string data";
  const DWORD kDWORDData = 0xdeadbabe;
  const int64_t kInt64Data = 0xdeadbabedeadbabeLL;

  // Test value creation
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kStringValueName, kStringData));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kDWORDValueName, kDWORDData));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(kInt64ValueName, &kInt64Data,
                                          sizeof(kInt64Data), REG_QWORD));
  EXPECT_EQ(3U, key.GetValueCount());
  EXPECT_TRUE(key.HasValue(kStringValueName));
  EXPECT_TRUE(key.HasValue(kDWORDValueName));
  EXPECT_TRUE(key.HasValue(kInt64ValueName));

  // Test Read
  std::wstring string_value;
  DWORD dword_value = 0;
  int64_t int64_value = 0;
  ASSERT_EQ(ERROR_SUCCESS, key.ReadValue(kStringValueName, &string_value));
  ASSERT_EQ(ERROR_SUCCESS, key.ReadValueDW(kDWORDValueName, &dword_value));
  ASSERT_EQ(ERROR_SUCCESS, key.ReadInt64(kInt64ValueName, &int64_value));
  EXPECT_EQ(kStringData, string_value);
  EXPECT_EQ(kDWORDData, dword_value);
  EXPECT_EQ(kInt64Data, int64_value);

  // Make sure out args are not touched if ReadValue fails
  const wchar_t* kNonExistent = L"NonExistent";
  ASSERT_NE(ERROR_SUCCESS, key.ReadValue(kNonExistent, &string_value));
  ASSERT_NE(ERROR_SUCCESS, key.ReadValueDW(kNonExistent, &dword_value));
  ASSERT_NE(ERROR_SUCCESS, key.ReadInt64(kNonExistent, &int64_value));
  EXPECT_EQ(kStringData, string_value);
  EXPECT_EQ(kDWORDData, dword_value);
  EXPECT_EQ(kInt64Data, int64_value);

  // Test delete
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kStringValueName));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kDWORDValueName));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteValue(kInt64ValueName));
  EXPECT_EQ(0U, key.GetValueCount());
  EXPECT_FALSE(key.HasValue(kStringValueName));
  EXPECT_FALSE(key.HasValue(kDWORDValueName));
  EXPECT_FALSE(key.HasValue(kInt64ValueName));
}

TYPED_TEST(RegistryTest, BigValueIteratorTest) {
  typename TypeParam::RegType key(TypeParam::Create());
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, this->root_key().c_str(),
                                    KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key.Valid());

  // Create a test value that is larger than MAX_PATH.
  std::wstring data(MAX_PATH * 2, 'a');

  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(data.c_str(), data.c_str()));

  RegistryValueIterator iterator(HKEY_CURRENT_USER, this->root_key().c_str());
  ASSERT_TRUE(iterator.Valid());
  EXPECT_EQ(data, iterator.Name());
  EXPECT_EQ(data, iterator.Value());
  // ValueSize() is in bytes, including NUL.
  EXPECT_EQ((MAX_PATH * 2 + 1) * sizeof(wchar_t), iterator.ValueSize());
  ++iterator;
  EXPECT_FALSE(iterator.Valid());
}

TYPED_TEST(RegistryTest, TruncatedCharTest) {
  typename TypeParam::RegType key(TypeParam::Create());
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, this->root_key().c_str(),
                                    KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key.Valid());

  const wchar_t kName[] = L"name";
  // kData size is not a multiple of sizeof(wchar_t).
  const uint8_t kData[] = {1, 2, 3, 4, 5};
  EXPECT_EQ(5u, std::size(kData));
  ASSERT_EQ(ERROR_SUCCESS,
            key.WriteValue(kName, kData, std::size(kData), REG_BINARY));

  RegistryValueIterator iterator(HKEY_CURRENT_USER, this->root_key().c_str());
  ASSERT_TRUE(iterator.Valid());
  // Avoid having to use EXPECT_STREQ here by leveraging StringPiece's
  // operator== to perform a deep comparison.
  EXPECT_EQ(WStringPiece(kName), WStringPiece(iterator.Name()));
  // ValueSize() is in bytes.
  ASSERT_EQ(std::size(kData), iterator.ValueSize());
  // Value() is NUL terminated.
  int end = (iterator.ValueSize() + sizeof(wchar_t) - 1) / sizeof(wchar_t);
  EXPECT_NE('\0', iterator.Value()[end - 1]);
  EXPECT_EQ('\0', iterator.Value()[end]);
  EXPECT_EQ(0, std::memcmp(kData, iterator.Value(), std::size(kData)));
  ++iterator;
  EXPECT_FALSE(iterator.Valid());
}

// Tests that the value iterator is okay with an empty key.
TYPED_TEST(RegistryTest, ValueIteratorEmptyKey) {
  RegistryValueIterator iterator(HKEY_CURRENT_USER, this->root_key().c_str());
  EXPECT_EQ(iterator.ValueCount(), 0U);
  EXPECT_FALSE(iterator.Valid());
}

// Tests that the default value is seen by a value iterator.
TYPED_TEST(RegistryTest, ValueIteratorDefaultValue) {
  const WStringPiece kTestString(L"i miss you");
  ASSERT_EQ(TypeParam::Create(HKEY_CURRENT_USER, this->root_key().c_str(),
                              KEY_SET_VALUE)
                .WriteValue(nullptr, kTestString.data()),
            ERROR_SUCCESS);
  RegistryValueIterator iterator(HKEY_CURRENT_USER, this->root_key().c_str());
  EXPECT_EQ(iterator.ValueCount(), 1U);
  ASSERT_TRUE(iterator.Valid());
  EXPECT_EQ(WStringPiece(iterator.Name()), WStringPiece());
  EXPECT_EQ(iterator.ValueSize(), (kTestString.size() + 1) * sizeof(wchar_t));
  EXPECT_EQ(iterator.Type(), REG_SZ);
  EXPECT_EQ(WStringPiece(iterator.Value()), kTestString);
  ++iterator;
  EXPECT_FALSE(iterator.Valid());
}

TYPED_TEST(RegistryTest, RecursiveDelete) {
  typename TypeParam::RegType key(TypeParam::Create());
  // Create root_key()
  //                  \->Bar (TestValue)
  //                     \->Foo (TestValue)
  //                        \->Bar
  //                           \->Foo
  //                  \->Moo
  //                  \->Foo
  // and delete root_key()
  std::wstring key_path = this->root_key();
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Bar", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"TestValue", L"TestData"));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Moo", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Foo", KEY_WRITE));

  key_path += L"\\Bar";
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  key_path += L"\\Foo";
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Foo", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.WriteValue(L"TestValue", L"TestData"));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, this->root_key().c_str(), KEY_WRITE));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteEmptyKey(L""));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteEmptyKey(L"Bar\\Foo"));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteEmptyKey(L"Bar"));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteEmptyKey(L"Foo"));

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_CREATE_SUB_KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Bar", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"Foo", KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(L""));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, this->root_key().c_str(), KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(L"Bar"));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(L"Bar"));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, key_path.c_str(), KEY_READ));
}

TYPED_TEST(RegistryTest, OpenSubKey) {
  typename TypeParam::RegType key(TypeParam::Create());
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_CURRENT_USER, this->root_key().c_str(),
                                    KEY_READ | KEY_CREATE_SUB_KEY));

  ASSERT_NE(ERROR_SUCCESS, key.OpenKey(L"foo", KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS, key.CreateKey(L"foo", KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, this->root_key().c_str(), KEY_READ));
  ASSERT_EQ(ERROR_SUCCESS, key.OpenKey(L"foo", KEY_READ));

  std::wstring foo_key = this->root_key() + L"\\Foo";
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, foo_key.c_str(), KEY_READ));

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, this->root_key().c_str(), KEY_WRITE));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(L"foo"));
}

namespace {

class TestChangeDelegate {
 public:
  TestChangeDelegate() = default;
  ~TestChangeDelegate() = default;

  void OnKeyChanged() {
    RunLoop::QuitCurrentWhenIdleDeprecated();
    called_ = true;
  }

  bool WasCalled() {
    bool was_called = called_;
    called_ = false;
    return was_called;
  }

 private:
  bool called_ = false;
};

}  // namespace

TYPED_TEST(RegistryTest, ChangeCallback) {
  typename TypeParam::RegType key(TypeParam::Create());
  TestChangeDelegate delegate;
  test::TaskEnvironment task_environment;

  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_CURRENT_USER, this->root_key().c_str(), KEY_READ));

  ASSERT_TRUE(key.StartWatching(
      BindOnce(&TestChangeDelegate::OnKeyChanged, Unretained(&delegate))));
  EXPECT_FALSE(delegate.WasCalled());

  // Make some change.
  typename TypeParam::RegType key2(TypeParam::Create());
  ASSERT_EQ(ERROR_SUCCESS,
            key2.Open(HKEY_CURRENT_USER, this->root_key().c_str(),
                      KEY_READ | KEY_SET_VALUE));
  ASSERT_TRUE(key2.Valid());
  EXPECT_EQ(ERROR_SUCCESS, key2.WriteValue(L"name", L"data"));

  // Allow delivery of the notification.
  EXPECT_FALSE(delegate.WasCalled());
  RunLoop().Run();

  ASSERT_TRUE(delegate.WasCalled());
  EXPECT_FALSE(delegate.WasCalled());

  ASSERT_TRUE(key.StartWatching(
      BindOnce(&TestChangeDelegate::OnKeyChanged, Unretained(&delegate))));

  // Change something else.
  EXPECT_EQ(ERROR_SUCCESS, key2.WriteValue(L"name2", L"data2"));
  RunLoop().Run();
  ASSERT_TRUE(delegate.WasCalled());

  ASSERT_TRUE(key.StartWatching(
      BindOnce(&TestChangeDelegate::OnKeyChanged, Unretained(&delegate))));
  RunLoop().RunUntilIdle();
  EXPECT_FALSE(delegate.WasCalled());
}

namespace {

// A thread that runs tasks from a TestMockTimeTaskRunner.
class RegistryWatcherThread : public SimpleThread {
 public:
  explicit RegistryWatcherThread(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner)
      : SimpleThread("RegistryWatcherThread"),
        task_runner_(std::move(task_runner)) {}

 private:
  void Run() override {
    task_runner_->DetachFromThread();
    task_runner_->RunUntilIdle();
  }
  const scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
};

}  // namespace

TYPED_TEST(RegistryTest, WatcherNotSignaledOnInitiatingThreadExit) {
  typename TypeParam::RegType key(TypeParam::Create());

  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, this->root_key().c_str(), KEY_READ),
            ERROR_SUCCESS);

  auto test_task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::TestMockTimeTaskRunner::Type::kBoundToThread);
  ::testing::StrictMock<
      base::MockCallback<typename TypeParam::RegType::ChangeCallback>>
      change_cb;

  test_task_runner->PostTask(
      FROM_HERE, BindOnce(IgnoreResult(&TypeParam::RegType::StartWatching),
                          Unretained(&key), change_cb.Get()));

  {
    // Start the watch on a thread that then goes away.
    RegistryWatcherThread watcher_thread(test_task_runner);
    watcher_thread.Start();
    watcher_thread.Join();
  }

  // Termination of the thread should not cause a notification to get sent.
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&change_cb));
  test_task_runner->DetachFromThread();
  ASSERT_FALSE(test_task_runner->HasPendingTask());

  // Expect that a notification is sent when a change is made. Exit the run loop
  // when this happens.
  base::RunLoop run_loop;
  EXPECT_CALL(change_cb, Run).WillOnce([&run_loop]() { run_loop.Quit(); });

  // Make some change.
  typename TypeParam::RegType key2(TypeParam::Create());
  ASSERT_EQ(key2.Open(HKEY_CURRENT_USER, this->root_key().c_str(),
                      KEY_READ | KEY_SET_VALUE),
            ERROR_SUCCESS);
  ASSERT_TRUE(key2.Valid());
  ASSERT_EQ(key2.WriteValue(L"name", L"data"), ERROR_SUCCESS);

  // Wait for the watcher to be signaled.
  run_loop.Run();
}

TYPED_TEST(RegistryTest, TestMoveConstruct) {
  typename TypeParam::RegType key(TypeParam::Create());

  ASSERT_EQ(
      key.Open(HKEY_CURRENT_USER, this->root_key().c_str(), KEY_SET_VALUE),
      ERROR_SUCCESS);
  typename TypeParam::RegType key2(std::move(key));

  // The old key should be meaningless now.
  EXPECT_EQ(key.Handle(), nullptr);

  // And the new one should work just fine.
  EXPECT_NE(key2.Handle(), nullptr);
  EXPECT_EQ(key2.WriteValue(L"foo", 1U), ERROR_SUCCESS);
}

TYPED_TEST(RegistryTest, TestMoveAssign) {
  typename TypeParam::RegType key(TypeParam::Create());
  typename TypeParam::RegType key2(TypeParam::Create());
  const wchar_t kFooValueName[] = L"foo";

  ASSERT_EQ(key.Open(HKEY_CURRENT_USER, this->root_key().c_str(),
                     KEY_SET_VALUE | KEY_QUERY_VALUE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(kFooValueName, 1U), ERROR_SUCCESS);
  ASSERT_EQ(key2.Create(HKEY_CURRENT_USER,
                        (this->root_key() + L"\\child").c_str(), KEY_SET_VALUE),
            ERROR_SUCCESS);
  key2 = std::move(key);

  // The old key should be meaningless now.
  EXPECT_EQ(key.Handle(), nullptr);

  // And the new one should hold what was the old one.
  EXPECT_NE(key2.Handle(), nullptr);
  DWORD foo = 0;
  ASSERT_EQ(key2.ReadValueDW(kFooValueName, &foo), ERROR_SUCCESS);
  EXPECT_EQ(foo, 1U);
}

// Verify that either the platform, or the API-integration, causes deletion
// attempts via an invalid handle to fail with the expected error code.
TYPED_TEST(RegistryTest, DeleteWithInvalidRegKey) {
  typename TypeParam::RegType key(TypeParam::Create());

  static const wchar_t kFooName[] = L"foo";

  EXPECT_EQ(key.DeleteKey(kFooName), ERROR_INVALID_HANDLE);
  EXPECT_EQ(key.DeleteEmptyKey(kFooName), ERROR_INVALID_HANDLE);
  EXPECT_EQ(key.DeleteValue(kFooName), ERROR_INVALID_HANDLE);
}

namespace {

// A test harness for tests that use HKLM to test WoW redirection and such.
// TODO(https://crbug.com/377917): The tests here that write to the registry are
// disabled because they need work to handle parallel runs of different tests.
template <typename Traits>
class RegistryTestHKLM : public ::testing::Test {
 protected:
  enum : REGSAM {
#if defined(_WIN64)
    kNativeViewMask = KEY_WOW64_64KEY,
    kRedirectedViewMask = KEY_WOW64_32KEY,
#else
    kNativeViewMask = KEY_WOW64_32KEY,
    kRedirectedViewMask = KEY_WOW64_64KEY,
#endif  //  _WIN64
  };

  RegistryTestHKLM()
      : foo_software_key_(std::wstring(L"Software\\") + kRootKey + L"\\Foo") {}

  static bool IsRedirectorPresent() {
#if defined(_WIN64)
    return true;
#else
    return OSInfo::GetInstance()->IsWowX86OnAMD64();
#endif
  }

  const std::wstring foo_software_key_;
};

}  // namespace

TYPED_TEST_SUITE(RegistryTestHKLM, RegistryTypes, RegistryTypeNames);

namespace {

template <typename Traits>
class RegistryTestHKLMAdmin : public RegistryTestHKLM<Traits> {
 protected:
  void SetUp() override {
    if (!this->IsRedirectorPresent()) {
      GTEST_SKIP();
    }
    if (!::IsUserAnAdmin()) {
      GTEST_SKIP();
    }
    // Clean up any stale registry keys.
    for (const REGSAM mask :
         {this->kNativeViewMask, this->kRedirectedViewMask}) {
      typename Traits::RegType key(Traits::Create());
      key.Open(HKEY_LOCAL_MACHINE, L"Software", KEY_SET_VALUE | mask);
      key.DeleteKey(kRootKey);
    }
  }
};

}  // namespace

TYPED_TEST_SUITE(RegistryTestHKLMAdmin, RegistryTypes, RegistryTypeNames);

// This test requires running as an Administrator as it tests redirected
// registry writes to HKLM\Software
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa384253.aspx
TYPED_TEST(RegistryTestHKLMAdmin, Wow64RedirectedFromNative) {
  typename TypeParam::RegType key(TypeParam::Create());

  // Test redirected key access from non-redirected.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, this->foo_software_key_.c_str(),
                       KEY_WRITE | this->kRedirectedViewMask));
  ASSERT_NE(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE,
                                    this->foo_software_key_.c_str(), KEY_READ));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, this->foo_software_key_.c_str(),
                     KEY_READ | this->kNativeViewMask));

  // Open the non-redirected view of the parent and try to delete the test key.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, L"Software", KEY_SET_VALUE));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(kRootKey));
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_SET_VALUE | this->kNativeViewMask));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(kRootKey));

  // Open the redirected view and delete the key created above.
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_SET_VALUE | this->kRedirectedViewMask));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(kRootKey));
}

// Test for the issue found in http://crbug.com/384587 where OpenKey would call
// Close() and reset wow64_access_ flag to 0 and cause a NOTREACHED to hit on a
// subsequent OpenKey call.
TYPED_TEST(RegistryTestHKLM, SameWowFlags) {
  typename TypeParam::RegType key(TypeParam::Create());

  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_READ | KEY_WOW64_64KEY));
  ASSERT_EQ(ERROR_SUCCESS,
            key.OpenKey(L"Microsoft", KEY_READ | KEY_WOW64_64KEY));
  ASSERT_EQ(ERROR_SUCCESS, key.OpenKey(L"Windows", KEY_READ | KEY_WOW64_64KEY));
}

TYPED_TEST(RegistryTestHKLMAdmin, Wow64NativeFromRedirected) {
  typename TypeParam::RegType key(TypeParam::Create());

  // Test non-redirected key access from redirected.
  ASSERT_EQ(ERROR_SUCCESS,
            key.Create(HKEY_LOCAL_MACHINE, this->foo_software_key_.c_str(),
                       KEY_WRITE | this->kNativeViewMask));
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE,
                                    this->foo_software_key_.c_str(), KEY_READ));
  ASSERT_NE(ERROR_SUCCESS,
            key.Open(HKEY_LOCAL_MACHINE, this->foo_software_key_.c_str(),
                     KEY_READ | this->kRedirectedViewMask));

  // Open the redirected view of the parent and try to delete the test key
  // from the non-redirected view.
  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_SET_VALUE | this->kRedirectedViewMask));
  ASSERT_NE(ERROR_SUCCESS, key.DeleteKey(kRootKey));

  ASSERT_EQ(ERROR_SUCCESS, key.Open(HKEY_LOCAL_MACHINE, L"Software",
                                    KEY_SET_VALUE | this->kNativeViewMask));
  ASSERT_EQ(ERROR_SUCCESS, key.DeleteKey(kRootKey));
}

}  // namespace base::win
