// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import './routine_group.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {ArcDnsResolutionProblem, ArcHttpProblem, ArcPingProblem, CaptivePortalProblem, DnsLatencyProblem, DnsResolutionProblem, DnsResolverPresentProblem, GatewayCanBePingedProblem, HasSecureWiFiConnectionProblem, HttpFirewallProblem, HttpsFirewallProblem, HttpsLatencyProblem, RoutineProblems, RoutineType, RoutineVerdict, SignalStrengthProblem, VideoConferencingProblem} from 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-webui.js';

import {getNetworkDiagnosticsService} from './mojo_interface_provider.js';
import {getTemplate} from './network_diagnostics.html.js';
import {Routine, RoutineGroup, RoutineResponse} from './network_diagnostics_types.js';

/**
 * @fileoverview Polymer element for interacting with Network Diagnostics.
 */

/**
 * Helper function to create a routine object.
 * @param {string} name
 * @param {!RoutineType} type
 * @param {!RoutineGroup} group
 * @param {!function()} func
 * @return {!Routine} Routine object
 */
function createRoutine(name, type, group, func) {
  return {
    name: name,
    type: type,
    group: group,
    func: func,
    running: false,
    resultMsg: '',
    result: null,
    ariaDescription: '',
  };
}

Polymer({
  _template: getTemplate(),
  is: 'network-diagnostics',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * List of Diagnostics Routines
     * @private {!Array<!Routine>}
     */
    routines_: {
      type: Array,
      value: function() {
        const routineGroups = [
          {
            group: RoutineGroup.CONNECTION,
            routines: [
              {
                name: 'NetworkDiagnosticsLanConnectivity',
                type: RoutineType.kLanConnectivity,
                func: () => getNetworkDiagnosticsService().runLanConnectivity(),
              },
            ],
          },
          {
            group: RoutineGroup.WIFI,
            routines: [
              {
                name: 'NetworkDiagnosticsSignalStrength',
                type: RoutineType.kSignalStrength,
                func: () => getNetworkDiagnosticsService().runSignalStrength(),
              },
              {
                name: 'NetworkDiagnosticsHasSecureWiFiConnection',
                type: RoutineType.kHasSecureWiFiConnection,
                func: () =>
                    getNetworkDiagnosticsService().runHasSecureWiFiConnection(),
              },
            ],
          },
          {
            group: RoutineGroup.PORTAL,
            routines: [
              {
                name: 'NetworkDiagnosticsCaptivePortal',
                type: RoutineType.kCaptivePortal,
                func: () => getNetworkDiagnosticsService().runCaptivePortal(),
              },
            ],
          },
          {
            group: RoutineGroup.GATEWAY,
            routines: [
              {
                name: 'NetworkDiagnosticsGatewayCanBePinged',
                type: RoutineType.kGatewayCanBePinged,
                func: () =>
                    getNetworkDiagnosticsService().runGatewayCanBePinged(),
              },
            ],
          },
          {
            group: RoutineGroup.FIREWALL,
            routines: [
              {
                name: 'NetworkDiagnosticsHttpFirewall',
                type: RoutineType.kHttpFirewall,
                func: () => getNetworkDiagnosticsService().runHttpFirewall(),
              },
              {
                name: 'NetworkDiagnosticsHttpsFirewall',
                type: RoutineType.kHttpsFirewall,
                func: () => getNetworkDiagnosticsService().runHttpsFirewall(),

              },
              {
                name: 'NetworkDiagnosticsHttpsLatency',
                type: RoutineType.kHttpsLatency,
                func: () => getNetworkDiagnosticsService().runHttpsLatency(),
              },
            ],
          },
          {
            group: RoutineGroup.DNS,
            routines: [
              {
                name: 'NetworkDiagnosticsDnsResolverPresent',
                type: RoutineType.kDnsResolverPresent,
                func: () =>
                    getNetworkDiagnosticsService().runDnsResolverPresent(),
              },
              {
                name: 'NetworkDiagnosticsDnsLatency',
                type: RoutineType.kDnsLatency,
                func: () => getNetworkDiagnosticsService().runDnsLatency(),
              },
              {
                name: 'NetworkDiagnosticsDnsResolution',
                type: RoutineType.kDnsResolution,
                func: () => getNetworkDiagnosticsService().runDnsResolution(),
              },
            ],
          },
          {
            group: RoutineGroup.GOOGLE_SERVICES,
            routines: [
              {
                name: 'NetworkDiagnosticsVideoConferencing',
                type: RoutineType.kVideoConferencing,
                // A null stun_server_hostname will use the routine
                // default.
                func: () => getNetworkDiagnosticsService().runVideoConferencing(
                    /*stun_server_hostname=*/ null),
              },
            ],
          },
          {
            group: RoutineGroup.ARC,
            routines: [
              {
                name: 'ArcNetworkDiagnosticsPing',
                type: RoutineType.kArcPing,
                func: () => getNetworkDiagnosticsService().runArcPing(),
              },
              {
                name: 'ArcNetworkDiagnosticsHttp',
                type: RoutineType.kArcHttp,
                func: () => getNetworkDiagnosticsService().runArcHttp(),
              },
              {
                name: 'ArcNetworkDiagnosticsDnsResolution',
                type: RoutineType.kArcDnsResolution,
                func: () =>
                    getNetworkDiagnosticsService().runArcDnsResolution(),
              },
            ],
          },
        ];
        const routines = [];

        for (const group of routineGroups) {
          for (const routine of group.routines) {
            routines[routine.type] = createRoutine(
                routine.name, routine.type, group.group, routine.func);
          }
        }

        return routines;
      },
    },

    /**
     * Enum of Routine Groups
     * @private {Object}
     */
    RoutineGroup_: {
      type: Object,
      value: RoutineGroup,
    },
  },

  /**
   * Runs all supported network diagnostics routines.
   * @public
   */
  runAllRoutines() {
    for (const routine of this.routines_) {
      this.runRoutine_(routine.type);
    }
  },

  /**
   * Runs all supported network diagnostics routines.
   * @param {!PolymerDeepPropertyChange} routines
   * @param {Number} group
   * @return {!Array<!Routine>}
   * @private
   */
  getRoutineGroup_(routines, group) {
    return routines.base.filter(r => r.group === group);
  },

  /**
   * @param {!RoutineType} type
   * @private
   */
  runRoutine_(type) {
    this.set(`routines_.${type}.running`, true);
    this.set(`routines_.${type}.resultMsg`, '');
    this.set(`routines_.${type}.result`, null);
    this.set(
        `routines_.${type}.ariaDescription`,
        this.i18n('NetworkDiagnosticsRunning'));

    this.routines_[type].func().then(
        result => this.evaluateRoutine_(type, result));
  },

  /**
   * @param {!RoutineType} type
   * @param {!RoutineResponse} response
   * @private
   */
  evaluateRoutine_(type, response) {
    const routine = `routines_.${type}`;
    this.set(routine + '.running', false);
    this.set(routine + '.result', response.result);

    const resultMsg = this.getRoutineResult_(this.routines_[type]);
    this.set(routine + '.resultMsg', resultMsg);
    this.set(routine + '.ariaDescription', resultMsg);
  },

  /**
   * Helper function to generate the routine result string.
   * @param {Routine} routine
   * @return {string}
   * @private
   */
  getRoutineResult_(routine) {
    let verdict = '';
    switch (routine.result.verdict) {
      case RoutineVerdict.kNoProblem:
        verdict = this.i18n('NetworkDiagnosticsPassed');
        break;
      case RoutineVerdict.kProblem:
        verdict = this.i18n('NetworkDiagnosticsFailed');
        break;
      case RoutineVerdict.kNotRun:
        verdict = this.i18n('NetworkDiagnosticsNotRun');
        break;
    }

    const problemStrings = this.getRoutineProblemsString_(
        routine.type, routine.result.problems, true);

    if (problemStrings.length) {
      return this.i18n(
          'NetworkDiagnosticsResultPlaceholder', verdict, ...problemStrings);
    } else if (routine.result) {
      return verdict;
    }

    return '';
  },

  /**
   * @param {!RoutineType} type The type of routine
   * @param {!RoutineProblems} problems The list of
   *     problems for the routine
   * @param {boolean} translate Flag to return a translated string
   * @return {!Array<string>} List of network diagnostic problem strings
   * @private
   */
  getRoutineProblemsString_(type, problems, translate) {
    const getString = s => translate ? this.i18n(s) : s;

    let problemStrings = [];
    switch (type) {
      case RoutineType.kSignalStrength:
        if (!problems.signalStrengthProblems) {
          break;
        }

        for (const problem of problems.signalStrengthProblems) {
          switch (problem) {
            case SignalStrengthProblem.kWeakSignal:
              problemStrings.push(getString('SignalStrengthProblem_Weak'));
              break;
          }
        }
        break;

      case RoutineType.kGatewayCanBePinged:
        if (!problems.gatewayCanBePingedProblems) {
          break;
        }

        for (const problem of problems.gatewayCanBePingedProblems) {
          switch (problem) {
            case GatewayCanBePingedProblem.kUnreachableGateway:
              problemStrings.push(getString('GatewayPingProblem_Unreachable'));
              break;
            case GatewayCanBePingedProblem.kFailedToPingDefaultNetwork:
              problemStrings.push(
                  getString('GatewayPingProblem_NoDefaultPing'));
              break;
            case GatewayCanBePingedProblem.kDefaultNetworkAboveLatencyThreshold:
              problemStrings.push(
                  getString('GatewayPingProblem_DefaultLatency'));
              break;
            case GatewayCanBePingedProblem.kUnsuccessfulNonDefaultNetworksPings:
              problemStrings.push(
                  getString('GatewayPingProblem_NoNonDefaultPing'));
              break;
            case GatewayCanBePingedProblem
                .kNonDefaultNetworksAboveLatencyThreshold:
              problemStrings.push(
                  getString('GatewayPingProblem_NonDefaultLatency'));
              break;
          }
        }
        break;

      case RoutineType.kHasSecureWiFiConnection:
        if (!problems.hasSecureWifiConnectionProblems) {
          break;
        }

        for (const problem of problems.hasSecureWifiConnectionProblems) {
          switch (problem) {
            case HasSecureWiFiConnectionProblem.kSecurityTypeNone:
              problemStrings.push(getString('SecureWifiProblem_None'));
              break;
            case HasSecureWiFiConnectionProblem.kSecurityTypeWep8021x:
              problemStrings.push(getString('SecureWifiProblem_8021x'));
              break;
            case HasSecureWiFiConnectionProblem.kSecurityTypeWepPsk:
              problemStrings.push(getString('SecureWifiProblem_PSK'));
              break;
            case HasSecureWiFiConnectionProblem.kUnknownSecurityType:
              problemStrings.push(getString('SecureWifiProblem_Unknown'));
              break;
          }
        }
        break;

      case RoutineType.kDnsResolverPresent:
        if (!problems.dnsResolverPresentProblems) {
          break;
        }

        for (const problem of problems.dnsResolverPresentProblems) {
          switch (problem) {
            case DnsResolverPresentProblem.kNoNameServersFound:
              problemStrings.push(
                  getString('DnsResolverProblem_NoNameServers'));
              break;
            case DnsResolverPresentProblem.kMalformedNameServers:
              problemStrings.push(
                  getString('DnsResolverProblem_MalformedNameServers'));
              break;
          }
        }
        break;

      case RoutineType.kDnsLatency:
        if (!problems.dnsLatencyProblems) {
          break;
        }

        for (const problem of problems.dnsLatencyProblems) {
          switch (problem) {
            case DnsLatencyProblem.kHostResolutionFailure:
              problemStrings.push(
                  getString('DnsLatencyProblem_FailedResolveHosts'));
              break;
            case DnsLatencyProblem.kSlightlyAboveThreshold:
              problemStrings.push(
                  getString('DnsLatencyProblem_LatencySlightlyAbove'));
              break;
            case DnsLatencyProblem.kSignificantlyAboveThreshold:
              problemStrings.push(
                  getString('DnsLatencyProblem_LatencySignificantlyAbove'));
              break;
          }
        }
        break;

      case RoutineType.kDnsResolution:
        if (!problems.dnsResolutionProblems) {
          break;
        }

        for (const problem of problems.dnsResolutionProblems) {
          switch (problem) {
            case DnsResolutionProblem.kFailedToResolveHost:
              problemStrings.push(
                  getString('DnsResolutionProblem_FailedResolve'));
              break;
          }
        }
        break;

      case RoutineType.kHttpFirewall:
        if (!problems.httpFirewallProblems) {
          break;
        }

        for (const problem of problems.httpFirewallProblems) {
          switch (problem) {
            case HttpFirewallProblem.kDnsResolutionFailuresAboveThreshold:
              problemStrings.push(
                  getString('FirewallProblem_DnsResolutionFailureRate'));
              break;
            case HttpFirewallProblem.kFirewallDetected:
              problemStrings.push(
                  getString('FirewallProblem_FirewallDetected'));
              break;
            case HttpFirewallProblem.kPotentialFirewall:
              problemStrings.push(
                  getString('FirewallProblem_FirewallSuspected'));
              break;
          }
        }
        break;

      case RoutineType.kHttpsFirewall:
        if (!problems.httpsFirewallProblems) {
          break;
        }

        for (const problem of problems.httpsFirewallProblems) {
          switch (problem) {
            case HttpsFirewallProblem.kHighDnsResolutionFailureRate:
              problemStrings.push(
                  getString('FirewallProblem_DnsResolutionFailureRate'));
              break;
            case HttpsFirewallProblem.kFirewallDetected:
              problemStrings.push(
                  getString('FirewallProblem_FirewallDetected'));
              break;
            case HttpsFirewallProblem.kPotentialFirewall:
              problemStrings.push(
                  getString('FirewallProblem_FirewallSuspected'));
              break;
          }
        }
        break;

      case RoutineType.kHttpsLatency:
        if (!problems.httpsLatencyProblems) {
          break;
        }

        for (const problem of problems.httpsLatencyProblems) {
          switch (problem) {
            case HttpsLatencyProblem.kFailedDnsResolutions:
              problemStrings.push(
                  getString('HttpsLatencyProblem_FailedDnsResolution'));
              break;
            case HttpsLatencyProblem.kFailedHttpsRequests:
              problemStrings.push(
                  getString('HttpsLatencyProblem_FailedHttpsRequests'));
              break;
            case HttpsLatencyProblem.kHighLatency:
              problemStrings.push(getString('HttpsLatencyProblem_HighLatency'));
              break;
            case HttpsLatencyProblem.kVeryHighLatency:
              problemStrings.push(
                  getString('HttpsLatencyProblem_VeryHighLatency'));
              break;
          }
        }
        break;

      case RoutineType.kCaptivePortal:
        if (!problems.captivePortalProblems) {
          break;
        }

        for (const problem of problems.captivePortalProblems) {
          switch (problem) {
            case CaptivePortalProblem.kNoActiveNetworks:
              problemStrings.push(
                  getString('CaptivePortalProblem_NoActiveNetworks'));
              break;
            case CaptivePortalProblem.kUnknownPortalState:
              problemStrings.push(
                  getString('CaptivePortalProblem_UnknownPortalState'));
              break;
            case CaptivePortalProblem.kPortalSuspected:
              problemStrings.push(
                  getString('CaptivePortalProblem_PortalSuspected'));
              break;
            case CaptivePortalProblem.kPortal:
              problemStrings.push(getString('CaptivePortalProblem_Portal'));
              break;
            case CaptivePortalProblem.kProxyAuthRequired:
              problemStrings.push(
                  getString('CaptivePortalProblem_ProxyAuthRequired'));
              break;
            case CaptivePortalProblem.kNoInternet:
              problemStrings.push(getString('CaptivePortalProblem_NoInternet'));
              break;
          }
        }
        break;

      case RoutineType.kVideoConferencing:
        if (!problems.videoConferencingProblems) {
          break;
        }

        for (const problem of problems.videoConferencingProblems) {
          switch (problem) {
            case VideoConferencingProblem.kUdpFailure:
              problemStrings.push(
                  getString('VideoConferencingProblem_UdpFailure'));
              break;
            case VideoConferencingProblem.kTcpFailure:
              problemStrings.push(
                  getString('VideoConferencingProblem_TcpFailure'));
              break;
            case VideoConferencingProblem.kMediaFailure:
              problemStrings.push(
                  getString('VideoConferencingProblem_MediaFailure'));
              break;
          }
        }
        break;

      case RoutineType.kArcPing:
        if (!problems.arcPingProblems) {
          break;
        }
        problemStrings = problemStrings.concat(
            this.getArcPingProblemStringIds(problems.arcPingProblems)
                .map(getString));
        break;

      case RoutineType.kArcDnsResolution:
        if (!problems.arcDnsResolutionProblems) {
          break;
        }
        problemStrings = problemStrings.concat(
            this.getArcDnsProblemStringIds(problems.arcDnsResolutionProblems)
                .map(getString));
        break;

      case RoutineType.kArcHttp:
        if (!problems.arcHttpProblems) {
          break;
        }
        problemStrings = problemStrings.concat(
            this.getArcHttpProblemStringIds(problems.arcHttpProblems)
                .map(getString));
        break;
    }

    return problemStrings;
  },

  /**
   * Converts a collection ArcPingProblem into string identifiers for display.
   *
   * @param {!Array<ArcPingProblem>} problems A collection of
   *     ArcPingProblem.
   * @returns {!Array<string>} A collection of string identifiers for each
   *     problem in the input.
   * @private
   */
  getArcPingProblemStringIds(problems) {
    const problemStringIds = [];

    for (const problem of problems) {
      switch (problem) {
        case ArcPingProblem.kFailedToGetArcServiceManager:
        case ArcPingProblem.kGetManagedPropertiesTimeoutFailure:
          problemStringIds.push('ArcRoutineProblem_InternalError');
          break;
        case ArcPingProblem.kFailedToGetNetInstanceForPingTest:
          problemStringIds.push('ArcRoutineProblem_ArcNotRunning');
          break;
        case ArcPingProblem.kUnreachableGateway:
          problemStringIds.push('GatewayPingProblem_Unreachable');
          break;
        case ArcPingProblem.kFailedToPingDefaultNetwork:
          problemStringIds.push('GatewayPingProblem_NoDefaultPing');
          break;
        case ArcPingProblem.kDefaultNetworkAboveLatencyThreshold:
          problemStringIds.push('GatewayPingProblem_DefaultLatency');
          break;
        case ArcPingProblem.kUnsuccessfulNonDefaultNetworksPings:
          problemStringIds.push('GatewayPingProblem_NoNonDefaultPing');
          break;
        case ArcPingProblem.kNonDefaultNetworksAboveLatencyThreshold:
          problemStringIds.push('GatewayPingProblem_NonDefaultLatency');
      }
    }

    return problemStringIds;
  },

  /**
   * Converts a collection ArcDnsResolutionProblem into string identifiers for
   * display.
   *
   * @param {!Array<ArcDnsResolutionProblem>} problems A
   *     collection of ArcDnsResolutionProblem.
   * @returns {!Array<string>} A collection of string identifiers for each
   *     problem in the input.
   * @private
   */
  getArcDnsProblemStringIds(problems) {
    const problemStringIds = [];

    for (const problem of problems) {
      switch (problem) {
        case ArcDnsResolutionProblem.kFailedToGetArcServiceManager:
          problemStringIds.push('ArcRoutineProblem_InternalError');
          break;
        case ArcDnsResolutionProblem
            .kFailedToGetNetInstanceForDnsResolutionTest:
          problemStringIds.push('ArcRoutineProblem_ArcNotRunning');
          break;
        case ArcDnsResolutionProblem.kHighLatency:
          problemStringIds.push('DnsLatencyProblem_LatencySlightlyAbove');
          break;
        case ArcDnsResolutionProblem.kVeryHighLatency:
          problemStringIds.push('DnsLatencyProblem_LatencySignificantlyAbove');
          break;
        case ArcDnsResolutionProblem.kFailedDnsQueries:
          problemStringIds.push('DnsResolutionProblem_FailedResolve');
          break;
      }
    }

    return problemStringIds;
  },

  /**
   * Converts a collection ArcHttpProblem into string identifiers for display.
   *
   * @param {!Array<ArcHttpProblem>} problems A collection of
   *     ArcHttpProblem.
   * @returns {!Array<string>} A collection of string identifiers for each
   *     problem in the input.
   * @private
   */
  getArcHttpProblemStringIds(problems) {
    const problemStringIds = [];

    for (const problem of problems) {
      switch (problem) {
        case ArcHttpProblem.kFailedToGetArcServiceManager:
          problemStringIds.push('ArcRoutineProblem_InternalError');
          break;
        case ArcHttpProblem.kFailedToGetNetInstanceForHttpTest:
          problemStringIds.push('ArcRoutineProblem_ArcNotRunning');
          break;
        case ArcHttpProblem.kHighLatency:
          problemStringIds.push('ArcHttpProblem_HighLatency');
          break;
        case ArcHttpProblem.kVeryHighLatency:
          problemStringIds.push('ArcHttpProblem_VeryHighLatency');
          break;
        case ArcHttpProblem.kFailedHttpRequests:
          problemStringIds.push('ArcHttpProblem_FailedHttpRequests');
          break;
      }
    }

    return problemStringIds;
  },

  /**
   * @param {!RoutineVerdict} verdict
   * @return {string} Untranslated string for a network diagnostic verdict
   * @private
   */
  getRoutineVerdictRawString_(verdict) {
    switch (verdict) {
      case RoutineVerdict.kNoProblem:
        return 'Passed';
      case RoutineVerdict.kNotRun:
        return 'Not Run';
      case RoutineVerdict.kProblem:
        return 'Failed';
    }
    return 'Unknown';
  },
});
