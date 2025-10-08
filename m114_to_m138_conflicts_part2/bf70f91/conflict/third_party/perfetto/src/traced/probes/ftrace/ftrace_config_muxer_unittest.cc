/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/traced/probes/ftrace/ftrace_config_muxer.h"

#include <memory>

<<<<<<< HEAD
#include "perfetto/ext/base/utils.h"
#include "protos/perfetto/trace/ftrace/ftrace_event.pbzero.h"
=======
#include "ftrace_config_muxer.h"
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "src/traced/probes/ftrace/atrace_wrapper.h"
#include "src/traced/probes/ftrace/compact_sched.h"
#include "src/traced/probes/ftrace/ftrace_procfs.h"
#include "src/traced/probes/ftrace/ftrace_stats.h"
<<<<<<< HEAD
#include "src/traced/probes/ftrace/predefined_tracepoints.h"
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
#include "src/traced/probes/ftrace/proto_translation_table.h"
#include "test/gtest_and_gmock.h"

using testing::_;
using testing::AnyNumber;
using testing::Contains;
<<<<<<< HEAD
using testing::ElementsAre;
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
using testing::ElementsAreArray;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
<<<<<<< HEAD
using testing::IsSupersetOf;
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
using testing::MatchesRegex;
using testing::NiceMock;
using testing::Not;
using testing::Return;
using testing::UnorderedElementsAre;

namespace perfetto {
namespace {

constexpr int kFakeSchedSwitchEventId = 1;
constexpr int kCgroupMkdirEventId = 12;
constexpr int kFakePrintEventId = 20;
constexpr int kSysEnterId = 329;

<<<<<<< HEAD
struct FakeSyscallTable {
  static constexpr char names[] =
      "sys_open\0"
      "sys_read\0";
  static constexpr SyscallTable::OffT offsets[]{0, 9};
};

std::string PageSizeKb() {
  return std::to_string(base::GetSysPageSize() / 1024);
}

FtraceConfig CreateFtraceConfig(const std::set<std::string>& names) {
  FtraceConfig config;
  for (const std::string& name : names)
    *config.add_ftrace_events() = name;
  return config;
}

=======
constexpr size_t kFakeSyscallCount = 2;
constexpr const char* kFakeSyscalls[] = {
    "sys_open",
    "sys_read",
};

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
class MockFtraceProcfs : public FtraceProcfs {
 public:
  MockFtraceProcfs() : FtraceProcfs("/root/") {
    ON_CALL(*this, NumberOfCpus()).WillByDefault(Return(1));
    ON_CALL(*this, WriteToFile(_, _)).WillByDefault(Return(true));
<<<<<<< HEAD
    ON_CALL(*this, AppendToFile(_, _)).WillByDefault(Return(true));
    ON_CALL(*this, ClearFile(_)).WillByDefault(Return(true));
    ON_CALL(*this, IsFileWriteable(_)).WillByDefault(Return(true));
=======
    ON_CALL(*this, ClearFile(_)).WillByDefault(Return(true));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    EXPECT_CALL(*this, NumberOfCpus()).Times(AnyNumber());
  }

  MOCK_METHOD(bool,
              WriteToFile,
              (const std::string& path, const std::string& str),
              (override));
  MOCK_METHOD(bool,
              AppendToFile,
              (const std::string& path, const std::string& str),
              (override));
  MOCK_METHOD(char, ReadOneCharFromFile, (const std::string& path), (override));
  MOCK_METHOD(bool, ClearFile, (const std::string& path), (override));
  MOCK_METHOD(std::string,
              ReadFileIntoString,
              (const std::string& path),
              (const, override));
  MOCK_METHOD(size_t, NumberOfCpus, (), (const, override));
  MOCK_METHOD(const std::set<std::string>,
              GetEventNamesForGroup,
              (const std::string& path),
              (const, override));
  MOCK_METHOD(std::string,
              ReadEventFormat,
              (const std::string& group, const std::string& name),
              (const, override));
<<<<<<< HEAD
  MOCK_METHOD(bool, IsFileWriteable, (const std::string& path), (override));
};

class MockAtraceWrapper : public AtraceWrapper {
 public:
  MOCK_METHOD(bool, RunAtrace, (const std::vector<std::string>&, std::string*));
  MOCK_METHOD(bool, SupportsUserspaceOnly, ());
  MOCK_METHOD(bool, SupportsPreferSdk, ());
=======
};

struct MockRunAtrace {
  MockRunAtrace() {
    static MockRunAtrace* instance;
    instance = this;
    SetRunAtraceForTesting(
        [](const std::vector<std::string>& args, std::string* atrace_errors) {
          return instance->RunAtrace(args, atrace_errors);
        });
  }

  ~MockRunAtrace() { SetRunAtraceForTesting(nullptr); }

  MOCK_METHOD(bool, RunAtrace, (const std::vector<std::string>&, std::string*));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
};

class MockProtoTranslationTable : public ProtoTranslationTable {
 public:
  MockProtoTranslationTable(NiceMock<MockFtraceProcfs>* ftrace_procfs,
                            const std::vector<Event>& events,
                            std::vector<Field> common_fields,
                            FtracePageHeaderSpec ftrace_page_header_spec,
                            CompactSchedEventFormat compact_sched_format)
      : ProtoTranslationTable(ftrace_procfs,
                              events,
                              common_fields,
                              ftrace_page_header_spec,
                              compact_sched_format,
                              PrintkMap()) {}
  MOCK_METHOD(Event*,
<<<<<<< HEAD
              CreateGenericEvent,
              (const GroupAndName& group_and_name),
              (override));
  MOCK_METHOD(Event*,
              CreateKprobeEvent,
=======
              GetOrCreateEvent,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
              (const GroupAndName& group_and_name),
              (override));
  MOCK_METHOD(const Event*,
              GetEvent,
              (const GroupAndName& group_and_name),
              (const, override));
};

<<<<<<< HEAD
TEST(ComputeCpuBufferSizeInPagesTest, DifferentCases) {
  constexpr auto test = ComputeCpuBufferSizeInPages;
  auto KbToPages = [](uint64_t kb) {
    return kb * 1024 / base::GetSysPageSize();
  };
  int64_t kNoRamInfo = 0;
  bool kExactSize = false;
  bool kLowerBoundSize = true;
  int64_t kLowRamPages =
      static_cast<int64_t>(KbToPages(3 * (1ULL << 20) + 512 * (1ULL << 10)));
  int64_t kHighRamPages =
      static_cast<int64_t>(KbToPages(7 * (1ULL << 20) + 512 * (1ULL << 10)));

  // No buffer size given: good default.
  EXPECT_EQ(test(0, kExactSize, kNoRamInfo), KbToPages(2048));
  // Default depends on device ram size.
  EXPECT_EQ(test(0, kExactSize, kLowRamPages), KbToPages(2048));
  EXPECT_EQ(test(0, kExactSize, kHighRamPages), KbToPages(8192));

  // buffer_size_lower_bound lets us choose a higher default than given.
  // default > requested:
  EXPECT_EQ(test(4096, kExactSize, kHighRamPages), KbToPages(4096));
  EXPECT_EQ(test(4096, kLowerBoundSize, kHighRamPages), KbToPages(8192));
  // requested > default:
  EXPECT_EQ(test(4096, kExactSize, kLowRamPages), KbToPages(4096));
  EXPECT_EQ(test(4096, kLowerBoundSize, kLowRamPages), KbToPages(4096));
  // requested > default:
  EXPECT_EQ(test(16384, kExactSize, kHighRamPages), KbToPages(16384));
  EXPECT_EQ(test(16384, kLowerBoundSize, kHighRamPages), KbToPages(16384));

  // Your size ends up with less than 1 page per cpu -> 1 page.
  EXPECT_EQ(test(3, kExactSize, kNoRamInfo), 1u);
  // You picked a good size -> your size rounded to nearest page.
  EXPECT_EQ(test(42, kExactSize, kNoRamInfo), KbToPages(42));

  // Sysconf returning an error is ok.
  EXPECT_EQ(test(0, kExactSize, -1), KbToPages(2048));
  EXPECT_EQ(test(4096, kExactSize, -1), KbToPages(4096));
}

// Base fixture that provides some dependencies but doesn't construct a
// FtraceConfigMuxer.
class FtraceConfigMuxerTest : public ::testing::Test {
 protected:
  FtraceConfigMuxerTest() {
    ON_CALL(atrace_wrapper_, RunAtrace).WillByDefault(Return(true));
    ON_CALL(atrace_wrapper_, SupportsUserspaceOnly).WillByDefault(Return(true));
    ON_CALL(atrace_wrapper_, SupportsPreferSdk).WillByDefault(Return(true));
  }

=======
class FtraceConfigMuxerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Don't probe for older SDK levels, that would relax the atrace-related
    // checks on older versions of Android (But some tests here test those).
    // We want the unittests to behave consistently (as if we were on a post P
    // device) regardless of the Android versions they run on.
    SetIsOldAtraceForTesting(false);
  }
  void TearDown() override { ClearIsOldAtraceForTesting(); }
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  std::unique_ptr<MockProtoTranslationTable> GetMockTable() {
    std::vector<Field> common_fields;
    std::vector<Event> events;
    return std::unique_ptr<MockProtoTranslationTable>(
        new MockProtoTranslationTable(
<<<<<<< HEAD
            &ftrace_, events, std::move(common_fields),
=======
            &table_procfs_, events, std::move(common_fields),
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
            ProtoTranslationTable::DefaultPageHeaderSpecForTesting(),
            InvalidCompactSchedEventFormatForTesting()));
  }

  SyscallTable GetSyscallTable() {
<<<<<<< HEAD
    return SyscallTable::Load<FakeSyscallTable>();
  }

  std::map<std::string, base::FlatSet<GroupAndName>>
  GetAccessiblePredefinedTracePoints(const ProtoTranslationTable* table) {
    return predefined_tracepoints::GetAccessiblePredefinedTracePoints(table,
                                                                      &ftrace_);
=======
    return SyscallTable(kFakeSyscalls, kFakeSyscallCount);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  std::unique_ptr<ProtoTranslationTable> CreateFakeTable(
      CompactSchedEventFormat compact_format =
          InvalidCompactSchedEventFormatForTesting()) {
    std::vector<Field> common_fields;
    std::vector<Event> events;
    {
      Event event = {};
      event.name = "sched_switch";
      event.group = "sched";
      event.ftrace_event_id = kFakeSchedSwitchEventId;
      events.push_back(event);
    }

    {
      Event event = {};
      event.name = "sched_wakeup";
      event.group = "sched";
      event.ftrace_event_id = 10;
      events.push_back(event);
    }

    {
      Event event = {};
      event.name = "sched_new";
      event.group = "sched";
      event.ftrace_event_id = 11;
      events.push_back(event);
    }

    {
      Event event = {};
      event.name = "cgroup_mkdir";
      event.group = "cgroup";
      event.ftrace_event_id = kCgroupMkdirEventId;
      events.push_back(event);
    }

    {
      Event event = {};
      event.name = "mm_vmscan_direct_reclaim_begin";
      event.group = "vmscan";
      event.ftrace_event_id = 13;
      events.push_back(event);
    }

    {
      Event event = {};
      event.name = "lowmemory_kill";
      event.group = "lowmemorykiller";
      event.ftrace_event_id = 14;
      events.push_back(event);
    }

    {
      Event event = {};
      event.name = "print";
      event.group = "ftrace";
      event.ftrace_event_id = kFakePrintEventId;
      events.push_back(event);
    }

    {
      Event event = {};
      event.name = "sys_enter";
      event.group = "raw_syscalls";
      event.ftrace_event_id = kSysEnterId;
      events.push_back(event);
    }

    return std::unique_ptr<ProtoTranslationTable>(new ProtoTranslationTable(
<<<<<<< HEAD
        &ftrace_, events, std::move(common_fields),
=======
        &table_procfs_, events, std::move(common_fields),
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
        ProtoTranslationTable::DefaultPageHeaderSpecForTesting(),
        compact_format, PrintkMap()));
  }

<<<<<<< HEAD
  NiceMock<MockFtraceProcfs> ftrace_;
  NiceMock<MockAtraceWrapper> atrace_wrapper_;
};

TEST_F(FtraceConfigMuxerTest, SecondaryInstanceDoNotSupportAtrace) {
  auto fake_table = CreateFakeTable();
  FtraceConfigMuxer model(
      &ftrace_, &atrace_wrapper_, fake_table.get(), GetSyscallTable(),
      GetAccessiblePredefinedTracePoints(fake_table.get()), {},
      /* secondary_instance= */ true);

  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  *config.add_atrace_categories() = "sched";

  ASSERT_FALSE(model.SetupConfig(/* id= */ 73, config));
}

TEST_F(FtraceConfigMuxerTest, CompactSchedConfig) {
  // Set scheduling event format as validated. The pre-parsed format itself
  // doesn't need to be sensible, as the tests won't use it.
  auto format_with_id = CompactSchedSwitchFormat{};
  format_with_id.event_id = kFakeSchedSwitchEventId;
  auto valid_compact_format = CompactSchedEventFormat{
      /*format_valid=*/true, format_with_id, CompactSchedWakingFormat{}};

  std::unique_ptr<ProtoTranslationTable> table =
      CreateFakeTable(valid_compact_format);
  FtraceConfigMuxer muxer(&ftrace_, &atrace_wrapper_, table.get(),
                          GetSyscallTable(),
                          GetAccessiblePredefinedTracePoints(table.get()), {});

  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  {
    // Explicitly enabled.
    FtraceConfig cfg = CreateFtraceConfig({"sched/sched_switch"});
    cfg.mutable_compact_sched()->set_enabled(true);

    FtraceConfigId id = 42;
    ASSERT_TRUE(muxer.SetupConfig(id, cfg));
    const FtraceDataSourceConfig* ds_config = muxer.GetDataSourceConfig(id);
    ASSERT_TRUE(ds_config);
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                Contains(kFakeSchedSwitchEventId));
    EXPECT_TRUE(ds_config->compact_sched.enabled);
  }
  {
    // Implicitly enabled (default).
    FtraceConfig cfg = CreateFtraceConfig({"sched/sched_switch"});

    FtraceConfigId id = 43;
    ASSERT_TRUE(muxer.SetupConfig(id, cfg));
    const FtraceDataSourceConfig* ds_config = muxer.GetDataSourceConfig(id);
    ASSERT_TRUE(ds_config);
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                Contains(kFakeSchedSwitchEventId));
    EXPECT_TRUE(ds_config->compact_sched.enabled);
  }
  {
    // Explicitly disabled.
    FtraceConfig cfg = CreateFtraceConfig({"sched/sched_switch"});
    cfg.mutable_compact_sched()->set_enabled(false);

    FtraceConfigId id = 44;
    ASSERT_TRUE(muxer.SetupConfig(id, cfg));
    const FtraceDataSourceConfig* ds_config = muxer.GetDataSourceConfig(id);
    ASSERT_TRUE(ds_config);
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                Contains(kFakeSchedSwitchEventId));
    EXPECT_FALSE(ds_config->compact_sched.enabled);
  }
  {
    // Disabled if not recording sched_switch.
    FtraceConfig cfg = CreateFtraceConfig({});

    FtraceConfigId id = 45;
    ASSERT_TRUE(muxer.SetupConfig(id, cfg));
    const FtraceDataSourceConfig* ds_config = muxer.GetDataSourceConfig(id);
    ASSERT_TRUE(ds_config);
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                Not(Contains(kFakeSchedSwitchEventId)));
    EXPECT_FALSE(ds_config->compact_sched.enabled);
  }
}

// Fixture that constructs a FtraceConfigMuxer with a fake
// ProtoTranslationTable.
class FtraceConfigMuxerFakeTableTest : public FtraceConfigMuxerTest {
 protected:
  std::unique_ptr<ProtoTranslationTable> table_ = CreateFakeTable();
  FtraceConfigMuxer model_ =
      FtraceConfigMuxer(&ftrace_,
                        &atrace_wrapper_,
                        table_.get(),
                        GetSyscallTable(),
                        GetAccessiblePredefinedTracePoints(table_.get()),
                        {});
};

TEST_F(FtraceConfigMuxerFakeTableTest, GenericSyscallFiltering) {
=======
  NiceMock<MockFtraceProcfs> table_procfs_;
  std::unique_ptr<ProtoTranslationTable> table_ = CreateFakeTable();
};

TEST_F(FtraceConfigMuxerTest, ComputeCpuBufferSizeInPages) {
  static constexpr size_t kMaxBufSizeInPages = 16 * 1024u;
  // No buffer size given: good default (128 pages = 2mb).
  EXPECT_EQ(ComputeCpuBufferSizeInPages(0), 512u);
  // Buffer size given way too big: good default.
  EXPECT_EQ(ComputeCpuBufferSizeInPages(10 * 1024 * 1024), kMaxBufSizeInPages);
  // The limit is 64mb per CPU, 512mb is too much.
  EXPECT_EQ(ComputeCpuBufferSizeInPages(512 * 1024), kMaxBufSizeInPages);
  // Your size ends up with less than 1 page per cpu -> 1 page.
  EXPECT_EQ(ComputeCpuBufferSizeInPages(3), 1u);
  // You picked a good size -> your size rounded to nearest page.
  EXPECT_EQ(ComputeCpuBufferSizeInPages(42), 10u);
}

TEST_F(FtraceConfigMuxerTest, GenericSyscallFiltering) {
  auto fake_table = CreateFakeTable();
  NiceMock<MockFtraceProcfs> ftrace;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config = CreateFtraceConfig({"raw_syscalls/sys_enter"});
  *config.add_syscall_events() = "sys_open";
  *config.add_syscall_events() = "sys_read";

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace_, WriteToFile(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/raw_syscalls/sys_enter/filter",
                                   "id == 0 || id == 1"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/raw_syscalls/sys_exit/filter",
                                   "id == 0 || id == 1"));

  FtraceConfigId id = 37;
  ASSERT_TRUE(model_.SetupConfig(id, config));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const std::set<size_t>& filter = model_.GetSyscallFilterForTesting();
  ASSERT_THAT(filter, UnorderedElementsAre(0, 1));
}

TEST_F(FtraceConfigMuxerFakeTableTest, UnknownSyscallFilter) {
=======
  FtraceConfigMuxer model(&ftrace, fake_table.get(), GetSyscallTable(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace, WriteToFile(_, _)).WillRepeatedly(Return(true));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/raw_syscalls/sys_enter/filter",
                                  "id == 0 || id == 1"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/raw_syscalls/sys_exit/filter",
                                  "id == 0 || id == 1"));

  FtraceConfigId id = 37;
  ASSERT_TRUE(model.SetupConfig(id, config));
  ASSERT_TRUE(model.ActivateConfig(id));

  const std::set<size_t>& filter = model.GetSyscallFilterForTesting();
  ASSERT_THAT(filter, UnorderedElementsAre(0, 1));
}

TEST_F(FtraceConfigMuxerTest, UnknownSyscallFilter) {
  auto fake_table = CreateFakeTable();
  NiceMock<MockFtraceProcfs> ftrace;
  FtraceConfigMuxer model(&ftrace, fake_table.get(), GetSyscallTable(), {});

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config = CreateFtraceConfig({"raw_syscalls/sys_enter"});
  config.add_syscall_events("sys_open");
  config.add_syscall_events("sys_not_a_call");

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));

  // Unknown syscall is ignored.
  ASSERT_TRUE(model_.SetupConfig(/*id = */ 73, config));
  ASSERT_THAT(model_.GetSyscallFilterForTesting(), UnorderedElementsAre(0));
}

TEST_F(FtraceConfigMuxerFakeTableTest, SyscallFilterMuxing) {
=======
  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));

  // Unknown syscall is ignored.
  ASSERT_TRUE(model.SetupConfig(/*id = */ 73, config));
  ASSERT_THAT(model.GetSyscallFilterForTesting(), UnorderedElementsAre(0));
}

TEST_F(FtraceConfigMuxerTest, SyscallFilterMuxing) {
  auto fake_table = CreateFakeTable();
  NiceMock<MockFtraceProcfs> ftrace;
  FtraceConfigMuxer model(&ftrace, fake_table.get(), GetSyscallTable(), {});

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig empty_config = CreateFtraceConfig({});

  FtraceConfig syscall_config = empty_config;
  syscall_config.add_ftrace_events("raw_syscalls/sys_enter");

  FtraceConfig syscall_open_config = syscall_config;
  syscall_open_config.add_syscall_events("sys_open");

  FtraceConfig syscall_read_config = syscall_config;
  syscall_read_config.add_syscall_events("sys_read");

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));

  // Expect no filter for non-syscall config.
  ASSERT_TRUE(model_.SetupConfig(/* id= */ 179239, empty_config));
  ASSERT_THAT(model_.GetSyscallFilterForTesting(), UnorderedElementsAre());

  // Expect no filter for syscall config with no specified events.
  FtraceConfigId syscall_id = 73;
  ASSERT_TRUE(model_.SetupConfig(syscall_id, syscall_config));
  ASSERT_THAT(model_.GetSyscallFilterForTesting(), UnorderedElementsAre());

  // Still expect no filter to satisfy this and the above.
  FtraceConfigId syscall_open_id = 101;
  ASSERT_TRUE(model_.SetupConfig(syscall_open_id, syscall_open_config));
  ASSERT_THAT(model_.GetSyscallFilterForTesting(), UnorderedElementsAre());

  // After removing the generic syscall trace, only the one with filter is left.
  ASSERT_TRUE(model_.RemoveConfig(syscall_id));
  ASSERT_THAT(model_.GetSyscallFilterForTesting(), UnorderedElementsAre(0));

  // With sys_read and sys_open traced separately, filter includes both.
  FtraceConfigId syscall_read_id = 57;
  ASSERT_TRUE(model_.SetupConfig(syscall_read_id, syscall_read_config));
  ASSERT_THAT(model_.GetSyscallFilterForTesting(), UnorderedElementsAre(0, 1));

  // After removing configs with filters, filter is reset to empty.
  ASSERT_TRUE(model_.RemoveConfig(syscall_open_id));
  ASSERT_TRUE(model_.RemoveConfig(syscall_read_id));
  ASSERT_THAT(model_.GetSyscallFilterForTesting(), UnorderedElementsAre());
}

TEST_F(FtraceConfigMuxerFakeTableTest, TurnFtraceOnOff) {
  FtraceConfig config = CreateFtraceConfig({"sched_switch", "foo"});

  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace_, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));

  FtraceConfigId id = 97;
  ASSERT_TRUE(model_.SetupConfig(id, config));

  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
=======
  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));

  // Expect no filter for non-syscall config.
  ASSERT_TRUE(model.SetupConfig(/* id= */ 179239, empty_config));
  ASSERT_THAT(model.GetSyscallFilterForTesting(), UnorderedElementsAre());

  // Expect no filter for syscall config with no specified events.
  FtraceConfigId syscall_id = 73;
  ASSERT_TRUE(model.SetupConfig(syscall_id, syscall_config));
  ASSERT_THAT(model.GetSyscallFilterForTesting(), UnorderedElementsAre());

  // Still expect no filter to satisfy this and the above.
  FtraceConfigId syscall_open_id = 101;
  ASSERT_TRUE(model.SetupConfig(syscall_open_id, syscall_open_config));
  ASSERT_THAT(model.GetSyscallFilterForTesting(), UnorderedElementsAre());

  // After removing the generic syscall trace, only the one with filter is left.
  ASSERT_TRUE(model.RemoveConfig(syscall_id));
  ASSERT_THAT(model.GetSyscallFilterForTesting(), UnorderedElementsAre(0));

  // With sys_read and sys_open traced separately, filter includes both.
  FtraceConfigId syscall_read_id = 57;
  ASSERT_TRUE(model.SetupConfig(syscall_read_id, syscall_read_config));
  ASSERT_THAT(model.GetSyscallFilterForTesting(), UnorderedElementsAre(0, 1));

  // After removing configs with filters, filter is reset to empty.
  ASSERT_TRUE(model.RemoveConfig(syscall_open_id));
  ASSERT_TRUE(model.RemoveConfig(syscall_read_id));
  ASSERT_THAT(model.GetSyscallFilterForTesting(), UnorderedElementsAre());
}

TEST_F(FtraceConfigMuxerTest, AddGenericEvent) {
  auto mock_table = GetMockTable();
  MockFtraceProcfs ftrace;

  FtraceConfig config = CreateFtraceConfig({"power/cpu_frequency"});

  FtraceConfigMuxer model(&ftrace, mock_table.get(), GetSyscallTable(), {});

  EXPECT_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/power/cpu_frequency/enable", "1"));
  EXPECT_CALL(*mock_table, GetEvent(GroupAndName("power", "cpu_frequency")))
      .Times(AnyNumber());

  static constexpr int kExpectedEventId = 77;
  Event event_to_return;
  event_to_return.name = "cpu_frequency";
  event_to_return.group = "power";
  event_to_return.ftrace_event_id = kExpectedEventId;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("power", "cpu_frequency")))
      .WillByDefault(Return(&event_to_return));
  EXPECT_CALL(*mock_table,
              GetOrCreateEvent(GroupAndName("power", "cpu_frequency")));

  FtraceConfigId id = 7;
  ASSERT_TRUE(model.SetupConfig(id, config));

  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kExpectedEventId}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kExpectedEventId}));
}

TEST_F(FtraceConfigMuxerTest, AddSameNameEvents) {
  auto mock_table = GetMockTable();
  NiceMock<MockFtraceProcfs> ftrace;

  FtraceConfig config = CreateFtraceConfig({"group_one/foo", "group_two/foo"});

  FtraceConfigMuxer model(&ftrace, mock_table.get(), GetSyscallTable(), {});

  static constexpr int kEventId1 = 1;
  Event event1;
  event1.name = "foo";
  event1.group = "group_one";
  event1.ftrace_event_id = kEventId1;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_one", "foo")))
      .WillByDefault(Return(&event1));
  EXPECT_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_one", "foo")));

  static constexpr int kEventId2 = 2;
  Event event2;
  event2.name = "foo";
  event2.group = "group_two";
  event2.ftrace_event_id = kEventId2;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_two", "foo")))
      .WillByDefault(Return(&event2));
  EXPECT_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_two", "foo")));

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  FtraceConfigId id = 5;
  ASSERT_TRUE(model.SetupConfig(id, config));
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));
}

TEST_F(FtraceConfigMuxerTest, AddAllEvents) {
  auto mock_table = GetMockTable();
  MockFtraceProcfs ftrace;

  FtraceConfig config = CreateFtraceConfig({"sched/*"});

  EXPECT_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_new_event/enable", "1"));

  FtraceConfigMuxer model(&ftrace, mock_table.get(), GetSyscallTable(), {});
  std::set<std::string> n = {"sched_switch", "sched_new_event"};
  ON_CALL(ftrace, GetEventNamesForGroup("events/sched"))
      .WillByDefault(Return(n));
  EXPECT_CALL(ftrace, GetEventNamesForGroup("events/sched")).Times(1);

  // Non-generic event.
  static constexpr int kSchedSwitchEventId = 1;
  Event sched_switch = {"sched_switch", "sched", {}, 0, 0, 0};
  sched_switch.ftrace_event_id = kSchedSwitchEventId;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("sched", "sched_switch")))
      .WillByDefault(Return(&sched_switch));
  EXPECT_CALL(*mock_table,
              GetOrCreateEvent(GroupAndName("sched", "sched_switch")))
      .Times(AnyNumber());

  // Generic event.
  static constexpr int kGenericEventId = 2;
  Event event_to_return;
  event_to_return.name = "sched_new_event";
  event_to_return.group = "sched";
  event_to_return.ftrace_event_id = kGenericEventId;
  ON_CALL(*mock_table,
          GetOrCreateEvent(GroupAndName("sched", "sched_new_event")))
      .WillByDefault(Return(&event_to_return));
  EXPECT_CALL(*mock_table,
              GetOrCreateEvent(GroupAndName("sched", "sched_new_event")));

  FtraceConfigId id = 13;
  ASSERT_TRUE(model.SetupConfig(id, config));
  ASSERT_TRUE(id);

  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kSchedSwitchEventId, kGenericEventId}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kSchedSwitchEventId, kGenericEventId}));
}

TEST_F(FtraceConfigMuxerTest, TwoWildcardGroups) {
  auto mock_table = GetMockTable();
  NiceMock<MockFtraceProcfs> ftrace;

  FtraceConfig config = CreateFtraceConfig({"group_one/*", "group_two/*"});

  FtraceConfigMuxer model(&ftrace, mock_table.get(), GetSyscallTable(), {});

  std::set<std::string> event_names = {"foo"};
  ON_CALL(ftrace, GetEventNamesForGroup("events/group_one"))
      .WillByDefault(Return(event_names));
  EXPECT_CALL(ftrace, GetEventNamesForGroup("events/group_one"))
      .Times(AnyNumber());

  ON_CALL(ftrace, GetEventNamesForGroup("events/group_two"))
      .WillByDefault(Return(event_names));
  EXPECT_CALL(ftrace, GetEventNamesForGroup("events/group_two"))
      .Times(AnyNumber());

  static constexpr int kEventId1 = 1;
  Event event1;
  event1.name = "foo";
  event1.group = "group_one";
  event1.ftrace_event_id = kEventId1;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_one", "foo")))
      .WillByDefault(Return(&event1));
  EXPECT_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_one", "foo")));

  static constexpr int kEventId2 = 2;
  Event event2;
  event2.name = "foo";
  event2.group = "group_two";
  event2.ftrace_event_id = kEventId2;
  ON_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_two", "foo")))
      .WillByDefault(Return(&event2));
  EXPECT_CALL(*mock_table, GetOrCreateEvent(GroupAndName("group_two", "foo")));

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  FtraceConfigId id = 23;
  ASSERT_TRUE(model.SetupConfig(id, config));
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));

  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));
}

TEST_F(FtraceConfigMuxerTest, TurnFtraceOnOff) {
  MockFtraceProcfs ftrace;

  FtraceConfig config = CreateFtraceConfig({"sched_switch", "foo"});

  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  EXPECT_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));

  FtraceConfigId id = 97;
  ASSERT_TRUE(model.SetupConfig(id, config));

  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kFakeSchedSwitchEventId}));

<<<<<<< HEAD
  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kFakeSchedSwitchEventId}));

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace_));
  EXPECT_CALL(ftrace_, NumberOfCpus()).Times(AnyNumber());
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_percent", _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/sched/sched_switch/enable", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", PageSizeKb()));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "1"));

  ASSERT_TRUE(model_.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerFakeTableTest, FtraceIsAlreadyOn) {
  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});

  // If someone is using ftrace already don't stomp on what they are doing.
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("function"));
  ASSERT_FALSE(model_.SetupConfig(/* id= */ 123, config));
}

TEST_F(FtraceConfigMuxerFakeTableTest, Atrace) {
  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  *config.add_atrace_categories() = "sched";

  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(atrace_wrapper_,
              RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                          "--only_userspace", "sched"}),
                        _))
      .WillOnce(Return(true));

  FtraceConfigId id = 57;
  ASSERT_TRUE(model_.SetupConfig(id, config));
=======
  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kFakeSchedSwitchEventId}));

  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace));
  EXPECT_CALL(ftrace, NumberOfCpus()).Times(AnyNumber());

  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", "4"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));

  ASSERT_TRUE(model.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerTest, FtraceIsAlreadyOn) {
  MockFtraceProcfs ftrace;

  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});

  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  // If someone is using ftrace already don't stomp on what they are doing.
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("function"));
  ASSERT_FALSE(model.SetupConfig(/* id= */ 123, config));
}

TEST_F(FtraceConfigMuxerTest, Atrace) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  *config.add_atrace_categories() = "sched";

  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "sched"}),
                                _))
      .WillOnce(Return(true));

  FtraceConfigId id = 57;
  ASSERT_TRUE(model.SetupConfig(id, config));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // "ftrace" group events are always enabled, and therefore the "print" event
  // will show up in the per data source event filter (as we want to record it),
  // but not the central filter (as we're not enabling/disabling it).
<<<<<<< HEAD
  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
=======
  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  ASSERT_TRUE(ds_config);
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(kFakeSchedSwitchEventId));
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(kFakePrintEventId));

<<<<<<< HEAD
  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
=======
  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  EXPECT_THAT(central_filter->GetEnabledEvents(),
              Contains(kFakeSchedSwitchEventId));

  EXPECT_CALL(
<<<<<<< HEAD
      atrace_wrapper_,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerFakeTableTest, AtraceTwoApps) {
=======
      atrace,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerTest, AtraceTwoApps) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config = CreateFtraceConfig({});
  *config.add_atrace_apps() = "com.google.android.gms.persistent";
  *config.add_atrace_apps() = "com.google.android.gms";

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(
      atrace_wrapper_,
=======
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(
      atrace,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      RunAtrace(
          ElementsAreArray(
              {"atrace", "--async_start", "--only_userspace", "-a",
               "com.google.android.gms,com.google.android.gms.persistent"}),
          _))
      .WillOnce(Return(true));

  FtraceConfigId id = 97;
<<<<<<< HEAD
  ASSERT_TRUE(model_.SetupConfig(id, config));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
=======
  ASSERT_TRUE(model.SetupConfig(id, config));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(kFakePrintEventId));

  EXPECT_CALL(
<<<<<<< HEAD
      atrace_wrapper_,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerFakeTableTest, AtraceMultipleConfigs) {
=======
      atrace,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerTest, AtraceMultipleConfigs) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config_a = CreateFtraceConfig({});
  *config_a.add_atrace_apps() = "app_a";
  *config_a.add_atrace_categories() = "cat_a";

  FtraceConfig config_b = CreateFtraceConfig({});
  *config_b.add_atrace_apps() = "app_b";
  *config_b.add_atrace_categories() = "cat_b";

  FtraceConfig config_c = CreateFtraceConfig({});
  *config_c.add_atrace_apps() = "app_c";
  *config_c.add_atrace_categories() = "cat_c";

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_a", "-a", "app_a"}),
                _))
      .WillOnce(Return(true));
  FtraceConfigId id_a = 3;
  ASSERT_TRUE(model_.SetupConfig(id_a, config_a));

  EXPECT_CALL(
      atrace_wrapper_,
=======
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "cat_a",
                                                  "-a", "app_a"}),
                                _))
      .WillOnce(Return(true));
  FtraceConfigId id_a = 3;
  ASSERT_TRUE(model.SetupConfig(id_a, config_a));

  EXPECT_CALL(
      atrace,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_a", "cat_b", "-a", "app_a,app_b"}),
                _))
      .WillOnce(Return(true));
  FtraceConfigId id_b = 13;
<<<<<<< HEAD
  ASSERT_TRUE(model_.SetupConfig(id_b, config_b));

  EXPECT_CALL(atrace_wrapper_,
=======
  ASSERT_TRUE(model.SetupConfig(id_b, config_b));

  EXPECT_CALL(atrace,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
              RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                          "--only_userspace", "cat_a", "cat_b",
                                          "cat_c", "-a", "app_a,app_b,app_c"}),
                        _))
      .WillOnce(Return(true));
  FtraceConfigId id_c = 23;
<<<<<<< HEAD
  ASSERT_TRUE(model_.SetupConfig(id_c, config_c));

  EXPECT_CALL(
      atrace_wrapper_,
=======
  ASSERT_TRUE(model.SetupConfig(id_c, config_c));

  EXPECT_CALL(
      atrace,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_a", "cat_c", "-a", "app_a,app_c"}),
                _))
      .WillOnce(Return(true));
<<<<<<< HEAD
  ASSERT_TRUE(model_.RemoveConfig(id_b));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_c", "-a", "app_c"}),
                _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_a));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_c));
}

TEST_F(FtraceConfigMuxerFakeTableTest, AtraceFailedConfig) {
=======
  ASSERT_TRUE(model.RemoveConfig(id_b));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "cat_c",
                                                  "-a", "app_c"}),
                                _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_a));

  EXPECT_CALL(
      atrace,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_c));
}

TEST_F(FtraceConfigMuxerTest, AtraceFailedConfig) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config_a = CreateFtraceConfig({});
  *config_a.add_atrace_apps() = "app_1";
  *config_a.add_atrace_apps() = "app_2";
  *config_a.add_atrace_categories() = "cat_1";
  *config_a.add_atrace_categories() = "cat_2";

  FtraceConfig config_b = CreateFtraceConfig({});
  *config_b.add_atrace_apps() = "app_fail";
  *config_b.add_atrace_categories() = "cat_fail";

  FtraceConfig config_c = CreateFtraceConfig({});
  *config_c.add_atrace_apps() = "app_1";
  *config_c.add_atrace_apps() = "app_3";
  *config_c.add_atrace_categories() = "cat_1";
  *config_c.add_atrace_categories() = "cat_3";

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(
      atrace_wrapper_,
=======
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(
      atrace,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2", "-a", "app_1,app_2"}),
                _))
      .WillOnce(Return(true));
  FtraceConfigId id_a = 7;
<<<<<<< HEAD
  ASSERT_TRUE(model_.SetupConfig(id_a, config_a));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2", "cat_fail", "-a",
                                  "app_1,app_2,app_fail"}),
                _))
      .WillOnce(Return(false));
  FtraceConfigId id_b = 17;
  ASSERT_TRUE(model_.SetupConfig(id_b, config_b));

  EXPECT_CALL(atrace_wrapper_,
=======
  ASSERT_TRUE(model.SetupConfig(id_a, config_a));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "cat_1",
                                                  "cat_2", "cat_fail", "-a",
                                                  "app_1,app_2,app_fail"}),
                                _))
      .WillOnce(Return(false));
  FtraceConfigId id_b = 17;
  ASSERT_TRUE(model.SetupConfig(id_b, config_b));

  EXPECT_CALL(atrace,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
              RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                          "--only_userspace", "cat_1", "cat_2",
                                          "cat_3", "-a", "app_1,app_2,app_3"}),
                        _))
      .WillOnce(Return(true));
  FtraceConfigId id_c = 47;
<<<<<<< HEAD
  ASSERT_TRUE(model_.SetupConfig(id_c, config_c));

  EXPECT_CALL(
      atrace_wrapper_,
=======
  ASSERT_TRUE(model.SetupConfig(id_c, config_c));

  EXPECT_CALL(
      atrace,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2", "-a", "app_1,app_2"}),
                _))
      .WillOnce(Return(true));
<<<<<<< HEAD
  ASSERT_TRUE(model_.RemoveConfig(id_c));

  // Removing the config we failed to enable doesn't change the atrace state
  // so we don't expect a call here.
  ASSERT_TRUE(model_.RemoveConfig(id_b));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_a));
}

TEST_F(FtraceConfigMuxerFakeTableTest, AtraceDuplicateConfigs) {
=======
  ASSERT_TRUE(model.RemoveConfig(id_c));

  // Removing the config we failed to enable doesn't change the atrace state
  // so we don't expect a call here.
  ASSERT_TRUE(model.RemoveConfig(id_b));

  EXPECT_CALL(
      atrace,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_a));
}

TEST_F(FtraceConfigMuxerTest, AtraceDuplicateConfigs) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config_a = CreateFtraceConfig({});
  *config_a.add_atrace_apps() = "app_1";
  *config_a.add_atrace_categories() = "cat_1";

  FtraceConfig config_b = CreateFtraceConfig({});
  *config_b.add_atrace_apps() = "app_1";
  *config_b.add_atrace_categories() = "cat_1";

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "-a", "app_1"}),
                _))
      .WillOnce(Return(true));
  FtraceConfigId id_a = 19;
  ASSERT_TRUE(model_.SetupConfig(id_a, config_a));

  FtraceConfigId id_b = 29;
  ASSERT_TRUE(model_.SetupConfig(id_b, config_b));

  ASSERT_TRUE(model_.RemoveConfig(id_a));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_b));
}

TEST_F(FtraceConfigMuxerFakeTableTest, AtraceAndFtraceConfigs) {
=======
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "cat_1",
                                                  "-a", "app_1"}),
                                _))
      .WillOnce(Return(true));
  FtraceConfigId id_a = 19;
  ASSERT_TRUE(model.SetupConfig(id_a, config_a));

  FtraceConfigId id_b = 29;
  ASSERT_TRUE(model.SetupConfig(id_b, config_b));

  ASSERT_TRUE(model.RemoveConfig(id_a));

  EXPECT_CALL(
      atrace,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_b));
}

TEST_F(FtraceConfigMuxerTest, AtraceAndFtraceConfigs) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config_a = CreateFtraceConfig({"sched/sched_cpu_hotplug"});

  FtraceConfig config_b = CreateFtraceConfig({"sched/sched_switch"});
  *config_b.add_atrace_categories() = "b";

  FtraceConfig config_c = CreateFtraceConfig({"sched/sched_switch"});

  FtraceConfig config_d = CreateFtraceConfig({"sched/sched_cpu_hotplug"});
  *config_d.add_atrace_categories() = "d";

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  FtraceConfigId id_a = 179;
  ASSERT_TRUE(model_.SetupConfig(id_a, config_a));

  EXPECT_CALL(atrace_wrapper_,
              RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                          "--only_userspace", "b"}),
                        _))
      .WillOnce(Return(true));
  FtraceConfigId id_b = 239;
  ASSERT_TRUE(model_.SetupConfig(id_b, config_b));

  FtraceConfigId id_c = 101;
  ASSERT_TRUE(model_.SetupConfig(id_c, config_c));

  EXPECT_CALL(atrace_wrapper_,
=======
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  FtraceConfigId id_a = 179;
  ASSERT_TRUE(model.SetupConfig(id_a, config_a));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "b"}),
                                _))
      .WillOnce(Return(true));
  FtraceConfigId id_b = 239;
  ASSERT_TRUE(model.SetupConfig(id_b, config_b));

  FtraceConfigId id_c = 101;
  ASSERT_TRUE(model.SetupConfig(id_c, config_c));

  EXPECT_CALL(atrace,
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
              RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                          "--only_userspace", "b", "d"}),
                        _))
      .WillOnce(Return(true));
  FtraceConfigId id_d = 47;
<<<<<<< HEAD
  ASSERT_TRUE(model_.SetupConfig(id_d, config_d));

  EXPECT_CALL(atrace_wrapper_,
              RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                          "--only_userspace", "b"}),
                        _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_d));

  ASSERT_TRUE(model_.RemoveConfig(id_c));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_b));

  ASSERT_TRUE(model_.RemoveConfig(id_a));
}

TEST_F(FtraceConfigMuxerFakeTableTest, AtraceErrorsPropagated) {
=======
  ASSERT_TRUE(model.SetupConfig(id_d, config_d));

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "b"}),
                                _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_d));

  ASSERT_TRUE(model.RemoveConfig(id_c));

  EXPECT_CALL(
      atrace,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.RemoveConfig(id_b));

  ASSERT_TRUE(model.RemoveConfig(id_a));
}

TEST_F(FtraceConfigMuxerTest, AtraceErrorsPropagated) {
  NiceMock<MockFtraceProcfs> ftrace;
  MockRunAtrace atrace;

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config = CreateFtraceConfig({});
  *config.add_atrace_categories() = "cat_1";
  *config.add_atrace_categories() = "cat_2";

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2"}),
                _))
=======
  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  EXPECT_CALL(atrace, RunAtrace(ElementsAreArray({"atrace", "--async_start",
                                                  "--only_userspace", "cat_1",
                                                  "cat_2"}),
                                _))
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      .WillOnce(Invoke([](const std::vector<std::string>&, std::string* err) {
        EXPECT_NE(err, nullptr);
        if (err)
          err->append("foo\nbar\n");
        return true;
      }));

  FtraceSetupErrors errors{};
  FtraceConfigId id_a = 23;
<<<<<<< HEAD
  ASSERT_TRUE(model_.SetupConfig(id_a, config, &errors));
  EXPECT_EQ(errors.atrace_errors, "foo\nbar\n");
}

TEST_F(FtraceConfigMuxerFakeTableTest, AtracePreferTrackEvent) {
  const FtraceConfigId id_a = 3;
  FtraceConfig config_a = CreateFtraceConfig({});
  *config_a.add_atrace_categories() = "cat_1";
  *config_a.add_atrace_categories() = "cat_2";
  *config_a.add_atrace_categories() = "cat_3";
  *config_a.add_atrace_categories_prefer_sdk() = "cat_1";
  *config_a.add_atrace_categories_prefer_sdk() = "cat_2";

  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));
  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2", "cat_3"}),
                _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--prefer_sdk", "cat_1", "cat_2"}),
                _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.SetupConfig(id_a, config_a));

  const FtraceConfigId id_b = 13;
  FtraceConfig config_b = CreateFtraceConfig({});
  *config_b.add_atrace_categories() = "cat_1";
  *config_b.add_atrace_categories() = "cat_2";
  *config_b.add_atrace_categories() = "cat_3";
  *config_b.add_atrace_categories_prefer_sdk() = "cat_2";
  *config_b.add_atrace_categories_prefer_sdk() = "cat_3";

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--prefer_sdk", "cat_2"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.SetupConfig(id_b, config_b));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--prefer_sdk", "cat_1", "cat_2"}),
                _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_b));

  const FtraceConfigId id_c = 13;
  FtraceConfig config_c = CreateFtraceConfig({});
  *config_c.add_atrace_categories() = "cat_1";
  *config_c.add_atrace_categories() = "cat_2";
  *config_c.add_atrace_categories() = "cat_3";
  *config_c.add_atrace_categories() = "cat_4";
  *config_c.add_atrace_categories_prefer_sdk() = "cat_1";
  *config_c.add_atrace_categories_prefer_sdk() = "cat_3";
  *config_c.add_atrace_categories_prefer_sdk() = "cat_4";

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2", "cat_3", "cat_4"}),
                _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--prefer_sdk", "cat_1", "cat_4"}),
                _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.SetupConfig(id_c, config_c));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--async_start", "--only_userspace",
                                  "cat_1", "cat_2", "cat_3"}),
                _))
      .WillOnce(Return(true));
  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(ElementsAreArray({"atrace", "--prefer_sdk", "cat_1", "cat_2"}),
                _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_c));

  EXPECT_CALL(
      atrace_wrapper_,
      RunAtrace(
          ElementsAreArray({"atrace", "--async_stop", "--only_userspace"}), _))
      .WillOnce(Return(true));
  EXPECT_CALL(atrace_wrapper_,
              RunAtrace(ElementsAreArray({"atrace", "--prefer_sdk"}), _))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.RemoveConfig(id_a));
}

TEST_F(FtraceConfigMuxerFakeTableTest, SetupClockForTesting) {
  FtraceConfig config;

  namespace pb0 = protos::pbzero;

  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());

  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/trace_clock", "boot"));
  model_.SetupClockForTesting(config);
  // unspecified = boot.
  EXPECT_EQ(model_.ftrace_clock(),
            static_cast<int>(pb0::FTRACE_CLOCK_UNSPECIFIED));

  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/trace_clock", "global"));
  model_.SetupClockForTesting(config);
  EXPECT_EQ(model_.ftrace_clock(), static_cast<int>(pb0::FTRACE_CLOCK_GLOBAL));

  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return(""));
  model_.SetupClockForTesting(config);
  EXPECT_EQ(model_.ftrace_clock(), static_cast<int>(pb0::FTRACE_CLOCK_UNKNOWN));

  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("local [global]"));
  model_.SetupClockForTesting(config);
  EXPECT_EQ(model_.ftrace_clock(), static_cast<int>(pb0::FTRACE_CLOCK_GLOBAL));
}

TEST_F(FtraceConfigMuxerFakeTableTest, GetFtraceEvents) {
  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  std::set<GroupAndName> events =
      model_.GetFtraceEventsForTesting(config, table_.get());
=======
  ASSERT_TRUE(model.SetupConfig(id_a, config, &errors));
  EXPECT_EQ(errors.atrace_errors, "foo\nbar\n");
}

TEST_F(FtraceConfigMuxerTest, SetupClockForTesting) {
  MockFtraceProcfs ftrace;
  FtraceConfig config;

  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});
  namespace pb0 = protos::pbzero;

  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  model.SetupClockForTesting(config);
  // unspecified = boot.
  EXPECT_EQ(model.ftrace_clock(),
            static_cast<int>(pb0::FTRACE_CLOCK_UNSPECIFIED));

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global"));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "global"));
  model.SetupClockForTesting(config);
  EXPECT_EQ(model.ftrace_clock(), static_cast<int>(pb0::FTRACE_CLOCK_GLOBAL));

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return(""));
  model.SetupClockForTesting(config);
  EXPECT_EQ(model.ftrace_clock(), static_cast<int>(pb0::FTRACE_CLOCK_UNKNOWN));

  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("local [global]"));
  model.SetupClockForTesting(config);
  EXPECT_EQ(model.ftrace_clock(), static_cast<int>(pb0::FTRACE_CLOCK_GLOBAL));
}

TEST_F(FtraceConfigMuxerTest, GetFtraceEvents) {
  MockFtraceProcfs ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  std::set<GroupAndName> events =
      model.GetFtraceEventsForTesting(config, table_.get());
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_switch")));
  EXPECT_THAT(events, Not(Contains(GroupAndName("ftrace", "print"))));
}

<<<<<<< HEAD
TEST_F(FtraceConfigMuxerFakeTableTest, GetFtraceEventsAtrace) {
  FtraceConfig config = CreateFtraceConfig({});
  *config.add_atrace_categories() = "sched";
  std::set<GroupAndName> events =
      model_.GetFtraceEventsForTesting(config, table_.get());
=======
TEST_F(FtraceConfigMuxerTest, GetFtraceEventsAtrace) {
  MockFtraceProcfs ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  FtraceConfig config = CreateFtraceConfig({});
  *config.add_atrace_categories() = "sched";
  std::set<GroupAndName> events =
      model.GetFtraceEventsForTesting(config, table_.get());
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_switch")));
  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_cpu_hotplug")));
  EXPECT_THAT(events, Contains(GroupAndName("ftrace", "print")));
}

<<<<<<< HEAD
TEST_F(FtraceConfigMuxerFakeTableTest, GetFtraceEventsAtraceCategories) {
=======
TEST_F(FtraceConfigMuxerTest, GetFtraceEventsAtraceCategories) {
  MockFtraceProcfs ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config = CreateFtraceConfig({});
  *config.add_atrace_categories() = "sched";
  *config.add_atrace_categories() = "memreclaim";
  std::set<GroupAndName> events =
<<<<<<< HEAD
      model_.GetFtraceEventsForTesting(config, table_.get());
=======
      model.GetFtraceEventsForTesting(config, table_.get());
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_switch")));
  EXPECT_THAT(events, Contains(GroupAndName("sched", "sched_cpu_hotplug")));
  EXPECT_THAT(events, Contains(GroupAndName("cgroup", "cgroup_mkdir")));
  EXPECT_THAT(events, Contains(GroupAndName("vmscan",
                                            "mm_vmscan_direct_reclaim_begin")));
  EXPECT_THAT(events,
              Contains(GroupAndName("lowmemorykiller", "lowmemory_kill")));
  EXPECT_THAT(events, Contains(GroupAndName("ftrace", "print")));
}

// Tests the enabling fallback logic that tries to use the "set_event" interface
// if writing the individual xxx/enable file fails.
<<<<<<< HEAD
TEST_F(FtraceConfigMuxerFakeTableTest, FallbackOnSetEvent) {
  FtraceConfig config =
      CreateFtraceConfig({"sched/sched_switch", "cgroup/cgroup_mkdir"});

  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_percent", _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace_, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/cgroup/cgroup_mkdir/enable", "1"))
      .WillOnce(Return(false));
  EXPECT_CALL(ftrace_, AppendToFile("/root/set_event", "cgroup:cgroup_mkdir"))
      .WillOnce(Return(true));
  FtraceConfigId id = 97;
  ASSERT_TRUE(model_.SetupConfig(id, config));

  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
=======
TEST_F(FtraceConfigMuxerTest, FallbackOnSetEvent) {
  MockFtraceProcfs ftrace;
  FtraceConfig config =
      CreateFtraceConfig({"sched/sched_switch", "cgroup/cgroup_mkdir"});
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  EXPECT_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/cgroup/cgroup_mkdir/enable", "1"))
      .WillOnce(Return(false));
  EXPECT_CALL(ftrace, AppendToFile("/root/set_event", "cgroup:cgroup_mkdir"))
      .WillOnce(Return(true));
  FtraceConfigId id = 97;
  ASSERT_TRUE(model.SetupConfig(id, config));

  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  ASSERT_TRUE(ds_config);
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(kFakeSchedSwitchEventId));
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(kCgroupMkdirEventId));

<<<<<<< HEAD
  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
=======
  const EventFilter* central_filter = model.GetCentralEventFilterForTesting();
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  EXPECT_THAT(central_filter->GetEnabledEvents(),
              Contains(kFakeSchedSwitchEventId));
  EXPECT_THAT(central_filter->GetEnabledEvents(),
              Contains(kCgroupMkdirEventId));

<<<<<<< HEAD
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/sched/sched_switch/enable", "0"));
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/cgroup/cgroup_mkdir/enable", "0"))
      .WillOnce(Return(false));
  EXPECT_CALL(ftrace_, AppendToFile("/root/set_event", "!cgroup:cgroup_mkdir"))
      .WillOnce(Return(true));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", PageSizeKb()));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model_.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerFakeTableTest, CompactSchedConfigWithInvalidFormat) {
=======
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/sched/sched_switch/enable", "0"));
  EXPECT_CALL(ftrace,
              WriteToFile("/root/events/cgroup/cgroup_mkdir/enable", "0"))
      .WillOnce(Return(false));
  EXPECT_CALL(ftrace, AppendToFile("/root/set_event", "!cgroup:cgroup_mkdir"))
      .WillOnce(Return(true));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace, WriteToFile("/root/buffer_size_kb", "4"));
  EXPECT_CALL(ftrace, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  EXPECT_CALL(ftrace, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model.RemoveConfig(id));
}

TEST_F(FtraceConfigMuxerTest, CompactSchedConfig) {
  // Set scheduling event format as validated. The pre-parsed format itself
  // doesn't need to be sensible, as the tests won't use it.
  auto valid_compact_format =
      CompactSchedEventFormat{/*format_valid=*/true, CompactSchedSwitchFormat{},
                              CompactSchedWakingFormat{}};

  NiceMock<MockFtraceProcfs> ftrace;
  table_ = CreateFakeTable(valid_compact_format);
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  // First data source - request compact encoding.
  FtraceConfig config_enabled = CreateFtraceConfig({"sched/sched_switch"});
  config_enabled.mutable_compact_sched()->set_enabled(true);

  // Second data source - no compact encoding (default).
  FtraceConfig config_disabled = CreateFtraceConfig({"sched/sched_switch"});

  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  {
    FtraceConfigId id = 73;
    ASSERT_TRUE(model.SetupConfig(id, config_enabled));
    const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
    ASSERT_TRUE(ds_config);
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                Contains(kFakeSchedSwitchEventId));
    EXPECT_TRUE(ds_config->compact_sched.enabled);
  }
  {
    FtraceConfigId id = 87;
    ASSERT_TRUE(model.SetupConfig(id, config_disabled));
    const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
    ASSERT_TRUE(ds_config);
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                Contains(kFakeSchedSwitchEventId));
    EXPECT_FALSE(ds_config->compact_sched.enabled);
  }
}

TEST_F(FtraceConfigMuxerTest, CompactSchedConfigWithInvalidFormat) {
  NiceMock<MockFtraceProcfs> ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  // Request compact encoding.
  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  config.mutable_compact_sched()->set_enabled(true);

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  FtraceConfigId id = 67;
  ASSERT_TRUE(model_.SetupConfig(id, config));
=======
  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  FtraceConfigId id = 67;
  ASSERT_TRUE(model.SetupConfig(id, config));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // The translation table says that the scheduling events' format didn't match
  // compile-time assumptions, so we won't enable compact events even if
  // requested.
<<<<<<< HEAD
  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
=======
  const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  ASSERT_TRUE(ds_config);
  EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
              Contains(kFakeSchedSwitchEventId));
  EXPECT_FALSE(ds_config->compact_sched.enabled);
}

<<<<<<< HEAD
TEST_F(FtraceConfigMuxerFakeTableTest, SkipGenericEventsOption) {
  static constexpr int kFtraceGenericEventId = 42;
  ON_CALL(ftrace_, ReadEventFormat("sched", "generic"))
=======
TEST_F(FtraceConfigMuxerTest, SkipGenericEventsOption) {
  NiceMock<MockFtraceProcfs> ftrace;
  FtraceConfigMuxer model(&ftrace, table_.get(), GetSyscallTable(), {});

  static constexpr int kFtraceGenericEventId = 42;
  ON_CALL(table_procfs_, ReadEventFormat("sched", "generic"))
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      .WillByDefault(Return(R"(name: generic
ID: 42
format:
	field:int common_pid;	offset:0;	size:4;	signed:1;

	field:u32 field_a;	offset:4;	size:4;	signed:0;
	field:int field_b;	offset:8;	size:4;	signed:1;

print fmt: "unused")"));

  // Data source asking for one known and one generic event.
  FtraceConfig config_default =
      CreateFtraceConfig({"sched/sched_switch", "sched/generic"});

  // As above, but with an option to suppress generic events.
  FtraceConfig config_with_disable =
      CreateFtraceConfig({"sched/sched_switch", "sched/generic"});
  config_with_disable.set_disable_generic_events(true);

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
=======
  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace, ReadFileIntoString("/root/events/enable"))
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      .WillByDefault(Return("0"));

  {
    FtraceConfigId id = 123;
<<<<<<< HEAD
    ASSERT_TRUE(model_.SetupConfig(id, config_default));
    const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
=======
    ASSERT_TRUE(model.SetupConfig(id, config_default));
    const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    ASSERT_TRUE(ds_config);
    // Both events enabled for the data source by default.
    EXPECT_THAT(
        ds_config->event_filter.GetEnabledEvents(),
        UnorderedElementsAre(kFakeSchedSwitchEventId, kFtraceGenericEventId));
  }
  {
    FtraceConfigId id = 321;
<<<<<<< HEAD
    ASSERT_TRUE(model_.SetupConfig(id, config_with_disable));
    const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
=======
    ASSERT_TRUE(model.SetupConfig(id, config_with_disable));
    const FtraceDataSourceConfig* ds_config = model.GetDataSourceConfig(id);
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    ASSERT_TRUE(ds_config);
    // Only the statically known event is enabled.
    EXPECT_THAT(ds_config->event_filter.GetEnabledEvents(),
                UnorderedElementsAre(kFakeSchedSwitchEventId));
  }
}

<<<<<<< HEAD
TEST_F(FtraceConfigMuxerFakeTableTest, Funcgraph) {
=======
TEST_F(FtraceConfigMuxerTest, Funcgraph) {
  auto fake_table = CreateFakeTable();
  NiceMock<MockFtraceProcfs> ftrace;
  FtraceConfigMuxer model(&ftrace, fake_table.get(), GetSyscallTable(), {});

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  FtraceConfig config;
  config.set_enable_function_graph(true);
  *config.add_function_filters() = "sched*";
  *config.add_function_filters() = "handle_mm_fault";

  *config.add_function_graph_roots() = "sched*";
  *config.add_function_graph_roots() = "*mm_fault";

<<<<<<< HEAD
  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));

  EXPECT_CALL(ftrace_, WriteToFile(_, _)).WillRepeatedly(Return(true));

  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));

  // Set up config, assert that the tracefs writes happened:
  EXPECT_CALL(ftrace_, ClearFile("/root/set_ftrace_filter"));
  EXPECT_CALL(ftrace_, ClearFile("/root/set_graph_function"));
  EXPECT_CALL(ftrace_, AppendToFile("/root/set_ftrace_filter",
                                    "sched*\nhandle_mm_fault"))
      .WillOnce(Return(true));
  EXPECT_CALL(ftrace_,
              AppendToFile("/root/set_graph_function", "sched*\n*mm_fault"))
      .WillOnce(Return(true));
  EXPECT_CALL(ftrace_, WriteToFile("/root/current_tracer", "function_graph"))
      .WillOnce(Return(true));
  FtraceConfigId id = 43;
  ASSERT_TRUE(model_.SetupConfig(id, config));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace_));
  // Toggle config on and off, tracer won't be reset yet:
  ASSERT_TRUE(model_.ActivateConfig(id));
  ASSERT_TRUE(model_.RemoveConfig(id));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace_));

  // Emulate ftrace_controller's call to ResetCurrentTracer (see impl comments
  // for why RemoveConfig is insufficient).
  EXPECT_CALL(ftrace_, ClearFile("/root/set_ftrace_filter"));
  EXPECT_CALL(ftrace_, ClearFile("/root/set_graph_function"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/current_tracer", "nop"))
      .WillOnce(Return(true));
  ASSERT_TRUE(model_.ResetCurrentTracer());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace_));
}

TEST_F(FtraceConfigMuxerFakeTableTest, PreserveFtraceBufferNotSetBufferSizeKb) {
  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});

  config.set_preserve_ftrace_buffer(true);
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", _)).Times(0);
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));

  FtraceConfigId id = 44;
  ASSERT_TRUE(model_.SetupConfig(id, config));
}

// Fixture that constructs a FtraceConfigMuxer with a mock
// ProtoTranslationTable.
class FtraceConfigMuxerMockTableTest : public FtraceConfigMuxerTest {
 protected:
  std::unique_ptr<MockProtoTranslationTable> mock_table_ = GetMockTable();
  FtraceConfigMuxer model_ =
      FtraceConfigMuxer(&ftrace_,
                        &atrace_wrapper_,
                        mock_table_.get(),
                        GetSyscallTable(),
                        GetAccessiblePredefinedTracePoints(mock_table_.get()),
                        {});
};

TEST_F(FtraceConfigMuxerMockTableTest, AddGenericEvent) {
  FtraceConfig config = CreateFtraceConfig({"power/cpu_frequency"});

  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace_, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/power/cpu_frequency/enable", "1"));
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("power", "cpu_frequency")))
      .Times(AnyNumber());

  static constexpr int kExpectedEventId = 77;
  Event event_to_return;
  event_to_return.name = "cpu_frequency";
  event_to_return.group = "power";
  event_to_return.ftrace_event_id = kExpectedEventId;
  ON_CALL(*mock_table_, GetEvent(GroupAndName("power", "cpu_frequency")))
      .WillByDefault(Return(&event_to_return));
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("power", "cpu_frequency")));

  FtraceConfigId id = 7;
  ASSERT_TRUE(model_.SetupConfig(id, config));

  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kExpectedEventId}));

  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kExpectedEventId}));
}

class FtraceConfigMuxerMockTableParamTest
    : public FtraceConfigMuxerMockTableTest,
      public testing::WithParamInterface<
          std::pair<perfetto::protos::gen::FtraceConfig_KprobeEvent_KprobeType,
                    std::string>> {};

TEST_P(FtraceConfigMuxerMockTableParamTest, AddKprobeEvent) {
  auto kprobe_type = std::get<0>(GetParam());
  std::string group_name(std::get<1>(GetParam()));

  FtraceConfig config;
  FtraceConfig::KprobeEvent kprobe_event;

  kprobe_event.set_probe("fuse_file_write_iter");
  kprobe_event.set_type(kprobe_type);
  *config.add_kprobe_events() = kprobe_event;

  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace_, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/" + group_name +
                                       "/fuse_file_write_iter/enable",
                                   "1"));

  static constexpr int kExpectedEventId = 77;
  Event event_to_return_kprobe;
  event_to_return_kprobe.name = "fuse_file_write_iter";
  event_to_return_kprobe.group = group_name.c_str();
  event_to_return_kprobe.ftrace_event_id = kExpectedEventId;
  event_to_return_kprobe.proto_field_id =
      protos::pbzero::FtraceEvent::kKprobeEventFieldNumber;
  EXPECT_CALL(*mock_table_,
              GetEvent(GroupAndName(group_name, "fuse_file_write_iter")))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_table_, CreateKprobeEvent(GroupAndName(
                                group_name, "fuse_file_write_iter")))
      .WillOnce(Return(&event_to_return_kprobe));

  FtraceConfigId id = 7;
  ASSERT_TRUE(model_.SetupConfig(id, config));

  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAre(kExpectedEventId));

  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAre(kExpectedEventId));
}

INSTANTIATE_TEST_SUITE_P(
    KprobeTypes,
    FtraceConfigMuxerMockTableParamTest,
    testing::Values(
        std::make_pair(
            protos::gen::FtraceConfig::KprobeEvent::KPROBE_TYPE_KPROBE,
            kKprobeGroup),
        std::make_pair(
            protos::gen::FtraceConfig::KprobeEvent::KPROBE_TYPE_KRETPROBE,
            kKretprobeGroup)));

TEST_F(FtraceConfigMuxerMockTableTest, AddKprobeBothEvent) {
  FtraceConfig config;
  FtraceConfig::KprobeEvent kprobe_event;

  kprobe_event.set_probe("fuse_file_write_iter");
  kprobe_event.set_type(
      protos::gen::FtraceConfig::KprobeEvent::KPROBE_TYPE_BOTH);
  *config.add_kprobe_events() = kprobe_event;

  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace_, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(
      ftrace_,
      WriteToFile("/root/events/perfetto_kprobes/fuse_file_write_iter/enable",
                  "1"));
  EXPECT_CALL(
      ftrace_,
      WriteToFile(
          "/root/events/perfetto_kretprobes/fuse_file_write_iter/enable", "1"));
  EXPECT_CALL(
      ftrace_,
      AppendToFile(
          "/root/kprobe_events",
          "p:perfetto_kprobes/fuse_file_write_iter fuse_file_write_iter"));
  EXPECT_CALL(
      ftrace_,
      AppendToFile("/root/kprobe_events",
                   std::string("r") + std::string(kKretprobeDefaultMaxactives) +
                       ":perfetto_kretprobes/fuse_file_write_iter "
                       "fuse_file_write_iter"));

  std::string g1(kKprobeGroup);
  static constexpr int kExpectedEventId = 77;
  Event event_to_return_kprobe;
  event_to_return_kprobe.name = "fuse_file_write_iter";
  event_to_return_kprobe.group = g1.c_str();
  event_to_return_kprobe.ftrace_event_id = kExpectedEventId;
  event_to_return_kprobe.proto_field_id =
      protos::pbzero::FtraceEvent::kKprobeEventFieldNumber;
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("perfetto_kprobes",
                                                  "fuse_file_write_iter")))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_table_, CreateKprobeEvent(GroupAndName(
                                "perfetto_kprobes", "fuse_file_write_iter")))
      .WillOnce(Return(&event_to_return_kprobe));

  std::string g2(kKretprobeGroup);
  static constexpr int kExpectedEventId2 = 78;
  Event event_to_return_kretprobe;
  event_to_return_kretprobe.name = "fuse_file_write_iter";
  event_to_return_kretprobe.group = g2.c_str();
  event_to_return_kretprobe.ftrace_event_id = kExpectedEventId2;
  event_to_return_kretprobe.proto_field_id =
      protos::pbzero::FtraceEvent::kKprobeEventFieldNumber;
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("perfetto_kretprobes",
                                                  "fuse_file_write_iter")))
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_table_, CreateKprobeEvent(GroupAndName(
                                "perfetto_kretprobes", "fuse_file_write_iter")))
      .WillOnce(Return(&event_to_return_kretprobe));

  FtraceConfigId id = 7;
  ASSERT_TRUE(model_.SetupConfig(id, config));

  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              UnorderedElementsAre(kExpectedEventId, kExpectedEventId2));

  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              UnorderedElementsAre(kExpectedEventId, kExpectedEventId2));
}

TEST_F(FtraceConfigMuxerMockTableTest, AddAllEvents) {
  FtraceConfig config = CreateFtraceConfig({"sched/*"});

  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillOnce(Return("nop"));
  EXPECT_CALL(ftrace_, ReadOneCharFromFile("/root/tracing_on"))
      .WillOnce(Return('1'));
  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "0"));
  EXPECT_CALL(ftrace_, WriteToFile("/root/events/enable", "0"));
  EXPECT_CALL(ftrace_, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace_, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));
  ON_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .WillByDefault(Return("[local] global boot"));
  EXPECT_CALL(ftrace_, ReadFileIntoString("/root/trace_clock"))
      .Times(AnyNumber());
  EXPECT_CALL(ftrace_, WriteToFile("/root/buffer_size_kb", _));
  EXPECT_CALL(ftrace_, WriteToFile("/root/trace_clock", "boot"));
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/sched/sched_switch/enable", "1"));
  EXPECT_CALL(ftrace_,
              WriteToFile("/root/events/sched/sched_new_event/enable", "1"));

  std::set<std::string> n = {"sched_switch", "sched_new_event"};
  ON_CALL(ftrace_, GetEventNamesForGroup("events/sched"))
      .WillByDefault(Return(n));
  EXPECT_CALL(ftrace_, GetEventNamesForGroup("events/sched")).Times(1);

  // Non-generic event.
  static constexpr int kSchedSwitchEventId = 1;
  Event sched_switch = {"sched_switch", "sched", {}, 0, 0, 0};
  sched_switch.ftrace_event_id = kSchedSwitchEventId;
  ON_CALL(*mock_table_, GetEvent(GroupAndName("sched", "sched_switch")))
      .WillByDefault(Return(&sched_switch));
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("sched", "sched_switch")))
      .Times(AnyNumber());

  // Generic event.
  static constexpr int kGenericEventId = 2;
  Event event_to_return;
  event_to_return.name = "sched_new_event";
  event_to_return.group = "sched";
  event_to_return.ftrace_event_id = kGenericEventId;
  ON_CALL(*mock_table_, GetEvent(GroupAndName("sched", "sched_new_event")))
      .WillByDefault(Return(&event_to_return));
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("sched", "sched_new_event")));

  FtraceConfigId id = 13;
  ASSERT_TRUE(model_.SetupConfig(id, config));
  ASSERT_TRUE(id);

  EXPECT_CALL(ftrace_, WriteToFile("/root/tracing_on", "1"));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kSchedSwitchEventId, kGenericEventId}));

  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kSchedSwitchEventId, kGenericEventId}));
}

TEST_F(FtraceConfigMuxerMockTableTest, TwoWildcardGroups) {
  FtraceConfig config = CreateFtraceConfig({"group_one/*", "group_two/*"});

  std::set<std::string> event_names = {"foo"};
  ON_CALL(ftrace_, GetEventNamesForGroup("events/group_one"))
      .WillByDefault(Return(event_names));
  EXPECT_CALL(ftrace_, GetEventNamesForGroup("events/group_one"))
      .Times(AnyNumber());

  ON_CALL(ftrace_, GetEventNamesForGroup("events/group_two"))
      .WillByDefault(Return(event_names));
  EXPECT_CALL(ftrace_, GetEventNamesForGroup("events/group_two"))
      .Times(AnyNumber());

  static constexpr int kEventId1 = 1;
  Event event1;
  event1.name = "foo";
  event1.group = "group_one";
  event1.ftrace_event_id = kEventId1;
  ON_CALL(*mock_table_, GetEvent(GroupAndName("group_one", "foo")))
      .WillByDefault(Return(&event1));
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("group_one", "foo")));

  static constexpr int kEventId2 = 2;
  Event event2;
  event2.name = "foo";
  event2.group = "group_two";
  event2.ftrace_event_id = kEventId2;
  ON_CALL(*mock_table_, GetEvent(GroupAndName("group_two", "foo")))
      .WillByDefault(Return(&event2));
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("group_two", "foo")));

  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  FtraceConfigId id = 23;
  ASSERT_TRUE(model_.SetupConfig(id, config));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
  ASSERT_TRUE(ds_config);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));

  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));
}

TEST_F(FtraceConfigMuxerMockTableTest, AddSameNameEvents) {
  FtraceConfig config = CreateFtraceConfig({"group_one/foo", "group_two/foo"});

  static constexpr int kEventId1 = 1;
  Event event1;
  event1.name = "foo";
  event1.group = "group_one";
  event1.ftrace_event_id = kEventId1;
  ON_CALL(*mock_table_, GetEvent(GroupAndName("group_one", "foo")))
      .WillByDefault(Return(&event1));
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("group_one", "foo")));

  static constexpr int kEventId2 = 2;
  Event event2;
  event2.name = "foo";
  event2.group = "group_two";
  event2.ftrace_event_id = kEventId2;
  ON_CALL(*mock_table_, GetEvent(GroupAndName("group_two", "foo")))
      .WillByDefault(Return(&event2));
  EXPECT_CALL(*mock_table_, GetEvent(GroupAndName("group_two", "foo")));

  ON_CALL(ftrace_, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));
  ON_CALL(ftrace_, ReadFileIntoString("/root/events/enable"))
      .WillByDefault(Return("0"));

  FtraceConfigId id = 5;
  ASSERT_TRUE(model_.SetupConfig(id, config));
  ASSERT_TRUE(model_.ActivateConfig(id));

  const FtraceDataSourceConfig* ds_config = model_.GetDataSourceConfig(id);
  ASSERT_THAT(ds_config->event_filter.GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));

  const EventFilter* central_filter = model_.GetCentralEventFilterForTesting();
  ASSERT_THAT(central_filter->GetEnabledEvents(),
              ElementsAreArray({kEventId1, kEventId2}));
=======
  ON_CALL(ftrace, ReadFileIntoString("/root/current_tracer"))
      .WillByDefault(Return("nop"));

  EXPECT_CALL(ftrace, WriteToFile(_, _)).WillRepeatedly(Return(true));

  EXPECT_CALL(ftrace, ClearFile("/root/trace"));
  EXPECT_CALL(ftrace, ClearFile(MatchesRegex("/root/per_cpu/cpu[0-9]/trace")));

  // Set up config, assert that the tracefs writes happened:
  EXPECT_CALL(ftrace, ClearFile("/root/set_ftrace_filter"));
  EXPECT_CALL(ftrace, ClearFile("/root/set_graph_function"));
  EXPECT_CALL(ftrace, AppendToFile("/root/set_ftrace_filter",
                                   "sched*\nhandle_mm_fault"))
      .WillOnce(Return(true));
  EXPECT_CALL(ftrace,
              AppendToFile("/root/set_graph_function", "sched*\n*mm_fault"))
      .WillOnce(Return(true));
  EXPECT_CALL(ftrace, WriteToFile("/root/current_tracer", "function_graph"))
      .WillOnce(Return(true));
  FtraceConfigId id = 43;
  ASSERT_TRUE(model.SetupConfig(id, config));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace));
  // Toggle config on and off, tracer won't be reset yet:
  ASSERT_TRUE(model.ActivateConfig(id));
  ASSERT_TRUE(model.RemoveConfig(id));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace));

  // Emulate ftrace_controller's call to ResetCurrentTracer (see impl comments
  // for why RemoveConfig is insufficient).
  EXPECT_CALL(ftrace, ClearFile("/root/set_ftrace_filter"));
  EXPECT_CALL(ftrace, ClearFile("/root/set_graph_function"));
  EXPECT_CALL(ftrace, WriteToFile("/root/current_tracer", "nop"))
      .WillOnce(Return(true));
  ASSERT_TRUE(model.ResetCurrentTracer());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&ftrace));
}

TEST_F(FtraceConfigMuxerTest, SecondaryInstanceDoNotSupportAtrace) {
  auto fake_table = CreateFakeTable();
  NiceMock<MockFtraceProcfs> ftrace;
  FtraceConfigMuxer model(&ftrace, fake_table.get(), GetSyscallTable(), {},
                          /* secondary_instance= */ true);

  FtraceConfig config = CreateFtraceConfig({"sched/sched_switch"});
  *config.add_atrace_categories() = "sched";

  ASSERT_FALSE(model.SetupConfig(/* id= */ 73, config));
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

}  // namespace
}  // namespace perfetto
