# Policy to preference mapping tests

## Introduction

Usually, incoming policy values from the management server are mapped into a
preference, which is then used by Chrome to control the behavior influenced by
that policy/preference. Mapping is typically done by a  policy handler (see
[configuration_policy_handler_list_factory.cc](https://cs.chromium.org/chromium/src/chrome/browser/policy/configuration_policy_handler_list_factory.cc)).
 This mapping can be very simple by directly mapping a policy value to pref (see
kSimplePolicyMap) or more complex by a custom policy handler that performs
validation / re-mapping / cross-checking with other policies etc.

In order to test these policy to preference mappings, we have a range of tests.
With these tests, you can specify 0...N policies and their values and then check
1...M affected preferences and their expected values or state. This allows to
easily test:

- Default preference values when no policy is set
- Simple policy to pref mappings
- Remappings of policy values to different preference values
- Remappings of policies (e.g. when an old policy is renamed or deprecated and
  replaced by a new policy)
- Dependencies between different policies (e.g. only set a pref when two or more
  policies are set to specific values or that one policy takes precedence over
  the other)

Each policy must have such a preference mapping test or provide a reason for why
such a test is missing (see `reason_for_missing_test`).

## Running the tests

To run the preference mapping tests, use `browser_tests` command with a
`--gtest_filter` for one of the following

- `PolicyPrefsTestCoverageTest.AllPoliciesHaveATestCase`
- `PolicyPrefsTest.PolicyToPrefsMapping`
- `SigninPolicyPrefsTest.PolicyToPrefsMapping` (CrOS only)
- `PolicyTest.AllPoliciesHaveATestCase` (iOS only)
- `PolicyTest.PolicyToPrefMappings` (iOS only)

Individual policies for PolicyPrefsTest.PolicyToPrefsMapping could be filtered
by `--test_policy_to_pref_mappings_filter` flag. The flag accepts policy names
(with the .optionalTestNameSuffix) separated by colon.

## Example

The following example tests the `IdleAction` policy, i.e. its mapping to two
separate preferences, their default values, and that either `IdleActionAC` or
`IdleActionBattery` take precedence.

```
{
  ...
  "IdleAction": {
    "os": ["chromeos_ash"],
    "policy_pref_mapping_tests": [
      {
        "note": "Check default values (no policies set)",
        "policies": { },
        "prefs": {
          "power.ac_idle_action": {
            "location": "user_profile",
            "default_value": 0,
          },
          "power.battery_idle_action": {
            "location": "user_profile",
            "default_value": 0
          }
        }
      },
      {
        "note": "Check simple policy value",
        "policies": { "IdleAction": 2 },
        "prefs": {
          "power.ac_idle_action": {
            "location": "user_profile",
            "value": 2,
          },
          "power.battery_idle_action": {
            "location": "user_profile",
            "value": 2
          }
        }
      },
      {
        "note": "Check IdleActionAC and IdleActionBattery take precedence",
        "policies": {
          "IdleAction": 0,
          "IdleActionAC": 1,
          "IdleActionBattery": 2
        },
        "prefs": {
          "power.ac_idle_action": {
            "location": "user_profile",
            "value": 1,
          },
          "power.battery_idle_action": {
            "location": "user_profile",
            "value": 2
          }
        }
      },
    ]
  },
  ...
}

```

## Test format

### policy_test_cases.json

The test cases per policy are defined in
[//components/policy/test/data/policy_test_cases.json](https://cs.chromium.org/chromium/src/components/policy/test/data/policy_test_cases.json)
(for iOS, see separate
[//ios/chrome/test/data/policy/policy_test_cases.json](https://cs.chromium.org/chromium/src/ios/chrome/test/data/policy/policy_test_cases.json)).

These files are JSON files with the policy name as key and a `PolicyTestCase`
(see below) as value). Each policy must have at least one meaningful test case
per supported operating system (see `reason_for_missing_test` to bypass),
otherwise the coverage browser test
`PolicyPrefsTestCoverageTest.AllPoliciesHaveATestCase` or the
`CheckPolicyTestCases` presubmit check will fail.

In case you want to add multiple `PolicyTestCase`s for a single policy (e.g.
different tests for different operating systems), you can add more keys with the
policy name and a custom name/comment like so
`PolicyName.InsertTestNameOrCommentHere`.

Since the JSON format does not allow comments, you can use the `note` field
anywhere to add further documentation.

### PolicyTestCase

The `os` field should be a list of strings representing the operating systems
the test case should be run on. Each supported operating system (indicated by
`supported_on` in
[policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json))
needs to have at least one test case. Valid values are:

- `win`
- `linux`
- `mac`
- `chromeos_ash`
- `chromeos_lacros`
- `android`
- `fuchsia`
- `ios` (tested via separate [policy_test_cases.json](https://cs.chromium.org/chromium/src/ios/chrome/test/data/policy/policy_test_cases.json))

The boolean `official_only` field indicates whether this policy is only
supported in official builds. Defaults to `false` if not specified.

The boolean `can_be_recommended` field indicates whether the policy values
should be checked with recommended values as well. Defaults to `false` if not
specified, which means that the policy values are being set as mandatory values
only, which in turn checks that the preference is marked as managed and not
modifiable by the user. If this field is true, the policy values are also set as
recommended values and the preference(s) are checked to still be modifiable by
the user. Use `check_for_mandatory` and `check_for_recommended` (see below) to
trigger certain preference(s) to only be checked for certain policy levels. If
the policy is recommendable (indicated by `can_be_recommended` in
[policy_templates.json](https://cs.chromium.org/chromium/src/components/policy/resources/policy_templates.json)
then the preference mapping test should also check recommended values.

The `policy_pref_mapping_tests` should be a non-empty list of
`PolicyPrefMappingTest`s.

In case the policy's preference mapping can not be tested, the `PolicyTestCase`
should just define a single `reason_for_missing_test_case` with a description on
why this policy should be skipped. Adding such a reason also bypasses the "all
policies need a pref mapping test" requirement. Possible reasons for a missing
tests could be:

- Policy was removed or is no longer supported on that platform
- Policy does not map to a preference (e.g. policy is only read by the policy
  stack, only read by CrOS daemons, maps into CrosSettings, etc.)
- Policy is of type `external`, i.e. the policy will trigger download of a
  resource, which will then be set as preference. This is currently not
  supported via preference mapping tests. Please mention
  [crbug.com/1213475](https://crbug.com/1213475) in your reasoning.
- any other valid reasoning according to reviewer's discretion

### PolicyPrefMappingTest

In most cases each `PolicyPrefMappingTest` will consist of two fields:

- `policies`: a dictionary with 0...N entries, where the key is a policy name
  and the value is the policy value that should be set.
- `prefs`: a dictionary with 1...N entries, where the key is a preference name
  and the value is a `PrefTestCase`

Additionally, each `PolicyPrefMappingTest` can also have an optional
`policies_settings` field to customize how or from where the policy applies. The
field's value is a dictionary, where the key is a policy name (should be one of
the policies set in `policies`) and the value is a dictionary with `scope`
(possible values are [`user`, `machine`], defaults to `user`) and `source`
(possible values are [`enterprise_default`, `command_line`, `cloud`,
`active_directory`, `local_account_override`, `platform`, `merged`,
`cloud_from_ash`], defaults to `cloud`).

Each `PolicyPrefMappingTest` can also have a `required_buildflags`,
which defines a list of required buildflags for the test to run.
Possible values are [`USE_CUPS`]. Defaults to an empty list if not specified. If
any of the specified buildflags is not defined in the current build, the test case
is skipped.

### PolicyPrefTestCase

#### Location

Each `PolicyPrefTestCase` should define a `location` field, where the preference
is registered. The test will fail if the preference is not registered in said
location. Possible values are:

- `user_profile` (default value)
- `local_state`
- `signin_profile` (CrOS only, use when a device policy is mapped into the
  sign-in screen profile using
  [login_profile_policy_provider.cc](https://cs.chromium.org/chromium/src/chrome/browser/ash/policy/login/login_profile_policy_provider.cc))

Policies that map into CrosSettings can not be tested at the moment (see
`reason_for_missing_test`).

#### Expected value

Each preference is also checked for its expected value. The expected value the
preference should take on can be defined by exactly one of three ways:

- A `value` field. Use this to specify an explicit value the preference should
  take on when the `policies` are set. This also checks that the preference is
  managed by policy.
- A `default_value` field. Use this to specify an explicit value the preference
  should take on when either no policies are set or to indicate that the
  preference should be unaffected by the `policies`.
- Neither specifying a `value` or `default_value` field. This only works if the
  `policies` field sets exactly one policy, in which case that policy's value is
  used as expected preference value. This behavior is mostly for backwards
  compatibility to ensure that existing tests with missing `value` /
  `default_value` also perform a value check, prefer to explicitly specify the
  expected `value`.

For each preference, you can also specify `check_for_mandatory` and
`check_for_recommended` (in combination with `can_be_recommended` from above)
booleans. Both default to `true` if not specified. This allows to test custom
behavior, e.g. setting a different preference when a policy is set as
recommended compared to set as mandatory. In most cases, you will just need to
use the `PolicyTestCase`'s `can_be_recommended` though.

### Full schema
```
{
  "${policy_name}[.optionalTestNameSuffix]": {
    "os": array<string>, // subset of ["win", "linux", "mac", "chromeos_ash", "chromeos_lacros", "android", "ios"]
    "official_only": boolean, // optional, defaults to false
    "can_be_recommended": boolean, // optional, defaults to false
    "reason_for_missing_test": string // optional, should be only field then
    "policy_pref_mapping_tests": [
      {
        "policies": {
          "${policy_name_1}": ${policy_value_1},
          "${policy_name_2}": ${policy_value_2},
          "${policy_name_3}": ${policy_value_3},
          ... // 0...N policies
        },
        "policies_settings": {
          "${policy_name_1}": {
            "scope": string, // optional, one of [user, machine], defaults to "user"
            "source": string, // optional, one of [enterprise_default, command_line, cloud, active_directory, local_account_override, platform, merged, cloud_from_ash], defaults to "cloud"
          },
          ... // 0...N policies
        }, // optional
        "required_buildflags": array<string> // optional, subset of ["USE_CUPS"], defaults to empty list
        "prefs": {
          ${pref_name_1}: {
            "location": string, // optional, one of [user_profile, local_state, signin_profile], defaults to "user_profile"
            "value": ${expected_pref_value_1}, // This or |default_value| should be set
            "default_value": ${expected_pref_value_1}, // This or |value| should be set
            "check_for_mandatory": boolean, // optional, defaults to true
            "check_for_recommended": boolean // optional, defaults to true
          },
          ... // 1...M prefs
        }
      }
    ]
  },

  ... // test cases for other policies
}
```
