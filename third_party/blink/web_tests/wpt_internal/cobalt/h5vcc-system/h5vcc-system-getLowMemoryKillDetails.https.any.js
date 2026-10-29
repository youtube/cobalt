// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  assert_implements(window.h5vcc, 'window.h5vcc not supported');
  assert_implements(window.h5vcc.system, 'window.h5vcc.system not supported');
  assert_equals(
      typeof window.h5vcc.system.getLowMemoryKillDetails, 'function');

  // Default (unstubbed) resolves to null when not low-memory killed.
  let actual = await window.h5vcc.system.getLowMemoryKillDetails();
  assert_equals(actual, null);

  // Explicit null stub also resolves to null.
  mockH5vccSystem.stubLowMemoryKillDetails(null);
  actual = await window.h5vcc.system.getLowMemoryKillDetails();
  assert_equals(actual, null);
}, 'getLowMemoryKillDetails() resolves to null when not low-memory killed');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  mockH5vccSystem.stubLowMemoryKillDetails({
    timestampPosixUsec: 1700000000123456n,
    foregroundStatus: mockH5vccSystem.ForegroundStatus.kForeground,
    pressureSource: mockH5vccSystem.PressureSource.kCobalt,
    systemMemoryAvailable: 1048576n,
    systemMemoryTotal: 8589934592n,
    cobaltMemoryUsage: 4294967297n,
    cobaltMemoryLimit: 8589934592n,
    reason: 'oom-killer: cgroup limit exceeded',
  });

  const actual = await window.h5vcc.system.getLowMemoryKillDetails();
  assert_not_equals(actual, null);
  assert_equals(actual.timestampPosixUsec, 1700000000123456);
  assert_equals(actual.foregroundStatus, 'foreground');
  assert_equals(actual.pressureSource, 'cobalt');
  assert_equals(actual.systemMemoryAvailable, 1048576);
  assert_equals(actual.systemMemoryTotal, 8589934592);
  assert_equals(actual.cobaltMemoryUsage, 4294967297);
  assert_equals(actual.cobaltMemoryLimit, 8589934592);
  assert_equals(actual.reason, 'oom-killer: cgroup limit exceeded');
}, 'getLowMemoryKillDetails() populates all fields with expected JS types');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  // 0n bytes must serialize to JS Number 0, whereas null/undefined memory or
  // empty/null/undefined reason must serialize to JS null.
  for (const emptyReason of [null, undefined, '']) {
    mockH5vccSystem.stubLowMemoryKillDetails({
      timestampPosixUsec: 0n,
      foregroundStatus: mockH5vccSystem.ForegroundStatus.kUnknown,
      pressureSource: mockH5vccSystem.PressureSource.kSystem,
      systemMemoryAvailable: 0n,
      systemMemoryTotal: null,
      cobaltMemoryUsage: 0n,
      cobaltMemoryLimit: undefined,
      reason: emptyReason,
    });

    const actual = await window.h5vcc.system.getLowMemoryKillDetails();
    assert_not_equals(actual, null);
    assert_equals(actual.timestampPosixUsec, 0);
    assert_equals(actual.systemMemoryAvailable, 0);
    assert_true('systemMemoryTotal' in actual);
    assert_equals(actual.systemMemoryTotal, null);
    assert_equals(actual.cobaltMemoryUsage, 0);
    assert_true('cobaltMemoryLimit' in actual);
    assert_equals(actual.cobaltMemoryLimit, null);
    assert_true('reason' in actual);
    assert_equals(actual.reason, null);
  }
}, 'getLowMemoryKillDetails() distinguishes 0 bytes from null and normalizes empty reason');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  const cases = [
    [
      mockH5vccSystem.ForegroundStatus.kUnknown,
      'unknown',
      mockH5vccSystem.PressureSource.kUnknown,
      'unknown',
    ],
    [
      mockH5vccSystem.ForegroundStatus.kForeground,
      'foreground',
      mockH5vccSystem.PressureSource.kSystem,
      'system',
    ],
    [
      mockH5vccSystem.ForegroundStatus.kBackground,
      'background',
      mockH5vccSystem.PressureSource.kCobalt,
      'cobalt',
    ],
  ];
  for (const [mojoFg, expectedFg, mojoPs, expectedPs] of cases) {
    mockH5vccSystem.stubLowMemoryKillDetails({
      timestampPosixUsec: 1700000000000000n,
      foregroundStatus: mojoFg,
      pressureSource: mojoPs,
      systemMemoryAvailable: null,
      systemMemoryTotal: null,
      cobaltMemoryUsage: null,
      cobaltMemoryLimit: null,
      reason: null,
    });
    const actual = await window.h5vcc.system.getLowMemoryKillDetails();
    assert_equals(actual.foregroundStatus, expectedFg);
    assert_equals(actual.pressureSource, expectedPs);
  }
}, 'getLowMemoryKillDetails() maps all foregroundStatus and pressureSource enum values');

h5vcc_system_tests(async (t, mockH5vccSystem) => {
  mockH5vccSystem.stubLowMemoryKillDetails({
    timestampPosixUsec: 1700000000999999n,
    foregroundStatus: mockH5vccSystem.ForegroundStatus.kBackground,
    pressureSource: mockH5vccSystem.PressureSource.kSystem,
    systemMemoryAvailable: 4096n,
    systemMemoryTotal: 1073741824n,
    cobaltMemoryUsage: null,
    cobaltMemoryLimit: null,
    reason: 'system-lowmem',
  });

  const [r1, r2] = await Promise.all([
    window.h5vcc.system.getLowMemoryKillDetails(),
    window.h5vcc.system.getLowMemoryKillDetails(),
  ]);
  r1.reason = 'mutated-by-caller';
  const r3 = await window.h5vcc.system.getLowMemoryKillDetails();

  assert_not_equals(r1, r2);
  assert_equals(r2.timestampPosixUsec, 1700000000999999);
  assert_equals(r2.foregroundStatus, 'background');
  assert_equals(r2.pressureSource, 'system');
  assert_equals(r2.systemMemoryAvailable, 4096);
  assert_equals(r2.systemMemoryTotal, 1073741824);
  assert_equals(r2.cobaltMemoryUsage, null);
  assert_equals(r2.cobaltMemoryLimit, null);
  assert_equals(r2.reason, 'system-lowmem');
  assert_equals(r3.reason, 'system-lowmem');
}, 'getLowMemoryKillDetails() returns fresh consistent objects across repeated calls');
