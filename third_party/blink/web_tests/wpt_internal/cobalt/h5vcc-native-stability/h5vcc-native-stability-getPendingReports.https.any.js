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
          nativeStabilityEventUuid: 'crash-uuid-12345',
          eventTimeSec: 1700000000n,
        }
      }
    },
    {
      hangReport: {
        base: {
          nativeStabilityEventUuid: 'hang-uuid-67890',
          eventTimeSec: 1700005000n,
        },
        isRecovered: false,
      }
    }
  ]);

  let actual = await window.h5vcc.nativeStability.getPendingReports();
  assert_equals(actual.length, 2);

  // Note: Web IDL dictionaries are flattened into the JS object, so members
  // from the inherited dictionary are present alongside members from the
  // inheriting dictionary.

  // Order-independent lookup for Crash Report
  const crashReport = actual.find(r => r.reportType === 'native_crash');
  assert_true(!!crashReport, 'crash report should be present');
  assert_equals(crashReport.nativeStabilityEventUuid, 'crash-uuid-12345');
  assert_equals(crashReport.eventTimeSec, 1700000000);

  // Order-independent lookup for Hang Report
  const hangReport = actual.find(r => r.reportType === 'hang');
  assert_true(!!hangReport, 'hang report should be present');
  assert_equals(hangReport.nativeStabilityEventUuid, 'hang-uuid-67890');
  assert_equals(hangReport.eventTimeSec, 1700005000);
  assert_equals(hangReport.isRecovered, false);
}, 'exercises H5vccNativeStability.getPendingReports() with crash & hang');
