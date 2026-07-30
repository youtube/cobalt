// META: global=window
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

h5vcc_native_stability_test(async (t, fake) => {
  assert_implements(window.h5vcc, 'window.h5vcc not supported');
  assert_implements(
    window.h5vcc.nativeStability,
    'window.h5vcc.nativeStability not supported');

  fake.stubReports([
    {
      crashReport: {
        base: {
          nativeStabilityEventUuid: 'crash-uuid-1',
          eventTimeSec: 1700000000n,
        }
      }
    },
    {
      crashReport: {
        base: {
          nativeStabilityEventUuid: 'crash-uuid-2',
          eventTimeSec: 1700001000n,
        }
      }
    }
  ]);

  let initialReports = await window.h5vcc.nativeStability.getPendingReports();
  assert_equals(initialReports.length, 2);

  // Acknowledge the first crash report.
  let ackResult = await window.h5vcc.nativeStability.acknowledgeReports(
    ['crash-uuid-1']);
  assert_equals(
    ackResult, undefined, 'acknowledgeReports should return undefined');

  let remainingReports =
    await window.h5vcc.nativeStability.getPendingReports();
  assert_equals(remainingReports.length, 1);
  assert_equals(remainingReports[0].nativeStabilityEventUuid, 'crash-uuid-2');
}, 'exercises H5vccNativeStability.acknowledgeReports() filtering');
