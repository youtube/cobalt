// META: timeout=long
// META: script=/common/get-host-info.sub.js
// META: script=../aggregation-service/support/aggregation-service.js
// META: script=resources/utils.js
// META: script=/shared-storage/resources/util.js

'use strict';

const reportPoller = new ReportPoller(
    '/.well-known/private-aggregation/report-shared-storage',
    '/.well-known/private-aggregation/debug/report-shared-storage',
    /*fullTimeoutMs=*/ 2000,
);

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    contributions: [{bucket: 1n, value: 2, filteringId: 3n}],
    enableDebugMode: true
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_WITH_FILTERING_ID_EXAMPLE,
          NUM_CONTRIBUTIONS_SHARED_STORAGE),
      /*expected_context_id=*/ undefined);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation with a non-default filtering ID');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    contributions: [{bucket: 1n, value: 2, filteringId: 3n}],
    enableDebugMode: false
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);
  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ undefined);
}, 'run() that calls Private Aggregation with a non-default filtering ID and no debug mode');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {contributions: [{bucket: 1n, value: 2}], enableDebugMode: true};
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE),
      /*expected_context_id=*/ undefined);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation with no filtering ID specified');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    contributions: [{bucket: 1n, value: 2, filteringId: 0n}],
    enableDebugMode: true
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_EXAMPLE, NUM_CONTRIBUTIONS_SHARED_STORAGE),
      /*expected_context_id=*/ undefined);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation with an explicitly default filtering ID');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {contributions: [{bucket: 1n, value: 2, filteringId: 255n}]};
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ undefined);
}, 'run() that calls Private Aggregation with max filtering ID for max bytes');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {contributions: [{bucket: 1n, value: 2, filteringId: 256n}]};
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'run() that calls Private Aggregation with filtering ID too big for max bytes');

private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {contributions: [{bucket: 1n, value: 2, filteringId: -1n}]};
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 0, /*expectedNumDebugReports=*/ 0);
}, 'run() that calls Private Aggregation with negative filtering ID');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    contributions: [{bucket: 1n, value: 2, filteringId: 259n}],
    enableDebugMode: true
  };
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {filteringIdMaxBytes: 3}
  });

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_WITH_FILTERING_ID_AND_CUSTOM_MAX_BYTES_EXAMPLE,
          NUM_CONTRIBUTIONS_SHARED_STORAGE,
          NULL_CONTRIBUTION_WITH_CUSTOM_FILTERING_ID_MAX_BYTES),
      /*expected_context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ undefined);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation with a filtering ID and custom max bytes');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {contributions: [{bucket: 1n, value: 2}], enableDebugMode: true};
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {filteringIdMaxBytes: 3}
  });

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          ONE_CONTRIBUTION_WITH_CUSTOM_FILTERING_ID_MAX_BYTES_EXAMPLE,
          NUM_CONTRIBUTIONS_SHARED_STORAGE,
          NULL_CONTRIBUTION_WITH_CUSTOM_FILTERING_ID_MAX_BYTES),
      /*expected_context_id=*/ undefined,
      /*aggregation_coordinator_origin=*/ undefined);

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation with no filtering ID specified, but still a custom max bytes');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    contributions: [{bucket: 1n, value: 2, filteringId: (1n << 64n) - 1n}]
  };
  await sharedStorage.run('contribute-to-histogram', {
    data,
    keepAlive: true,
    privateAggregationConfig: {filteringIdMaxBytes: 8}
  });

  const {reports: [report]} = await reportPoller.pollReportsAndAssert(
      /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 0);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ false,
      /*debug_key=*/ undefined,
      /*expected_payload=*/ undefined,
      /*expected_context_id=*/ undefined);
}, 'run() that calls Private Aggregation with max filtering ID possible');


private_aggregation_promise_test(async () => {
  await addModuleOnce('resources/shared-storage-module.js');

  const data = {
    enableDebugMode: true,
    contributions: [
      {bucket: 1n, value: 2, filteringId: 1n},
      {bucket: 1n, value: 2, filteringId: 2n}
    ]
  };
  await sharedStorage.run('contribute-to-histogram', {data, keepAlive: true});

  const {reports: [report], debug_reports: [debug_report]} =
      await reportPoller.pollReportsAndAssert(
          /*expectedNumReports=*/ 1, /*expectedNumDebugReports=*/ 1);

  verifyReport(
      report, /*api=*/ 'shared-storage', /*is_debug_enabled=*/ true,
      /*debug_key=*/ undefined,
      /*expected_payload=*/
      buildExpectedPayload(
          MULTIPLE_CONTRIBUTIONS_DIFFERING_IN_FILTERING_ID_EXAMPLE,
          NUM_CONTRIBUTIONS_SHARED_STORAGE));

  verifyReportsIdenticalExceptPayload(report, debug_report);
}, 'run() that calls Private Aggregation with contributions that match buckets but not filtering IDs');
