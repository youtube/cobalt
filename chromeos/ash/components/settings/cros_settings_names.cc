// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace ash {

const char kCrosSettingsPrefix[] = "cros.";

// All cros.accounts.* settings are stored in SignedSettings.
const char kAccountsPrefAllowGuest[] = "cros.accounts.allowBWSI";
const char kAccountsPrefAllowNewUser[] = "cros.accounts.allowGuest";
const char kAccountsPrefShowUserNamesOnSignIn[] =
    "cros.accounts.showUserNamesOnSignIn";
const char kAccountsPrefUsers[] = "cros.accounts.users";
// Only `ChromeUserManagerImpl` is allowed to directly use this setting. All
// other clients have to use `UserManager::IsEphemeralAccountId()` function to
// get ephemeral mode for account ID. Such rule is needed because there are
// new policies(e.g.kiosk ephemeral mode) that overrides behaviour of
// the current setting for some accounts.
const char kAccountsPrefEphemeralUsersEnabled[] =
    "cros.accounts.ephemeralUsersEnabled";
const char kAccountsPrefDeviceLocalAccounts[] =
    "cros.accounts.deviceLocalAccounts";
const char kAccountsPrefDeviceLocalAccountsKeyId[] = "id";
const char kAccountsPrefDeviceLocalAccountsKeyType[] = "type";
const char kAccountsPrefDeviceLocalAccountsKeyKioskAppId[] = "kiosk_app_id";
const char kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL[] =
    "kiosk_app_update_url";
const char kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage[] =
    "arc_kiosk_package";
const char kAccountsPrefDeviceLocalAccountsKeyArcKioskClass[] =
    "arc_kiosk_class";
const char kAccountsPrefDeviceLocalAccountsKeyArcKioskAction[] =
    "arc_kiosk_action";
const char kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName[] =
    "arc_kiosk_display_name";
const char kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl[] = "web_kiosk_url";
const char kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle[] =
    "web_kiosk_title";
const char kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl[] =
    "web_kiosk_icon_url";
const char kAccountsPrefDeviceLocalAccountsKeyEphemeralMode[] =
    "ephemeral_mode";
const char kAccountsPrefDeviceLocalAccountAutoLoginId[] =
    "cros.accounts.deviceLocalAccountAutoLoginId";
const char kAccountsPrefDeviceLocalAccountAutoLoginDelay[] =
    "cros.accounts.deviceLocalAccountAutoLoginDelay";
const char kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled[] =
    "cros.accounts.deviceLocalAccountAutoLoginBailoutEnabled";
const char kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline[] =
    "cros.accounts.deviceLocalAccountPromptForNetworkWhenOffline";
const char kAccountsPrefTransferSAMLCookies[] =
    "cros.accounts.transferSAMLCookies";

// A string pref that specifies a domain name for the autocomplete option during
// user sign-in flow.
const char kAccountsPrefLoginScreenDomainAutoComplete[] =
    "cros.accounts.login_screen_domain_auto_complete";

// A boolean pref indicating whether all Family Link accounts are allowed on the
// device additionally to the accounts listed in |kAccountsPrefUsers| pref.
const char kAccountsPrefFamilyLinkAccountsAllowed[] =
    "cros.accounts.family_link_allowed";

// All cros.signed.* settings are stored in SignedSettings.
const char kSignedDataRoamingEnabled[] = "cros.signed.data_roaming_enabled";

// True if auto-update was disabled by the system administrator.
const char kUpdateDisabled[] = "cros.system.updateDisabled";

// True if a target version prefix is set by the system administrator.
const char kTargetVersionPrefix[] = "cros.system.targetVersionPrefix";

// A list of strings which specifies allowed connection types for
// update.
const char kAllowedConnectionTypesForUpdate[] =
    "cros.system.allowedConnectionTypesForUpdate";

// The first constant refers to the user setting editable in the UI. The second
// refers to the timezone policy. This separation is necessary to allow the user
// to temporarily change the timezone for the current session and reset it to
// the policy's value on logout.
const char kSystemTimezone[] = "cros.system.timezone";
const char kSystemTimezonePolicy[] = "cros.system.timezone_policy";

// Value of kUse24HourClock user preference of device' owner.
// ChromeOS device uses this setting on login screen.
const char kSystemUse24HourClock[] = "cros.system.use_24hour_clock";

const char kDeviceOwner[] = "cros.device.owner";

const char kStatsReportingPref[] = "cros.metrics.reportingEnabled";

const char kReleaseChannel[] = "cros.system.releaseChannel";
const char kReleaseChannelDelegated[] = "cros.system.releaseChannelDelegated";
const char kReleaseLtsTag[] = "cros.system.releaseLtsTag";
const char kDeviceChannelDowngradeBehavior[] =
    "cros.system.channelDowngradeBehavior";

// This setting is used to enforce usage of system audio echo cancellation.
const char kDeviceSystemAecEnabled[] = "cros.audio.device_system_aec_enabled";

// A boolean pref that indicates whether OS & firmware version info should be
// reported along with device policy requests.
const char kReportDeviceVersionInfo[] =
    "cros.device_status.report_version_info";

// A boolean pref that indicates whether device activity times should be
// recorded and reported along with device policy requests.
const char kReportDeviceActivityTimes[] =
    "cros.device_status.report_activity_times";

// A boolean pref that indicates whether device sound volume should be recorded
// and reported along with device policy requests.
const char kReportDeviceAudioStatus[] =
    "cros.device_status.report_audio_status";

// A boolean pref that determines whether the board status should be
// included in status reports to the device management server.
const char kReportDeviceBoardStatus[] =
    "cros.device_status.report_board_status";

// A boolean pref that indicates whether the state of the dev mode switch at
// boot should be reported along with device policy requests.
const char kReportDeviceBootMode[] = "cros.device_status.report_boot_mode";

// A boolean pref that determines whether the device CPU information should be
// included in status reports to the device management server.
const char kReportDeviceCpuInfo[] = "cros.device_status.report_cpu_info";

// A boolean pref that determines whether the device timezone information should
// be included in status reports to the device management server.
const char kReportDeviceTimezoneInfo[] =
    "cros.device_status.report_timezone_info";

// A boolean pref that determines whether the device memory information should
// be included in status reports to the device management server.
const char kReportDeviceMemoryInfo[] = "cros.device_status.report_memory_info";

// A boolean pref that determines whether the device backlight information
// should be included in status reports to the device management server.
const char kReportDeviceBacklightInfo[] =
    "cros.device_status.report_backlight_info";

// A boolean pref that indicates whether the current location should be reported
// along with device policy requests.
const char kReportDeviceLocation[] = "cros.device_status.report_location";

// Determines whether the device reports static network configuration info such
// as MAC Address, MEID, and MEI in device status reports to the device
// management server.
const char kReportDeviceNetworkConfiguration[] =
    "cros.device_status.report_network_configuration";

// Determines whether the device reports dynamic network information such
// connection state, signal strength, and IP Address in device status reports
// and to management server.
const char kReportDeviceNetworkStatus[] =
    "cros.device_status.report_network_status";

// A boolean pref that determines whether the device peripherals should be
// included in reports to the telemetry API.
const char kReportDevicePeripherals[] = "cros.device_status.report_peripherals";

// A boolean pref that determines whether the device power status should be
// included in status reports to the device management server.
const char kReportDevicePowerStatus[] =
    "cros.device_status.report_power_status";

// A boolean pref that determines whether the storage status should be
// included in status reports to the device management server.
const char kReportDeviceStorageStatus[] =
    "cros.device_status.report_storage_status";

// A boolean pref that determines whether the security status should be
// included in status reports to the device management server.
const char kReportDeviceSecurityStatus[] =
    "cros.device_status.report_security_status";

// Determines whether the device reports recently logged in users in device
// status reports to the device management server.
const char kReportDeviceUsers[] = "cros.device_status.report_users";

// Determines whether the device reports kiosk session status (app IDs,
// versions, etc) in device status reports to the device management server.
const char kReportDeviceSessionStatus[] =
    "cros.device_status.report_session_status";

// Determines whether the device reports display and graphics statuses to the
// device_management server.
const char kReportDeviceGraphicsStatus[] =
    "cros.device_status.report_graphics_status";

// Determines whether the device reports crash report information to the device
// management server.
const char kReportDeviceCrashReportInfo[] =
    "cros.device_status.report_crash_report_info";

// Determines whether the device reports os update status (update status,
// new platform version and new required platform version of the auto
// launched kiosk app).
const char kReportOsUpdateStatus[] =
    "cros.device_status.report_os_update_status";

// Determines whether the device reports the current running kiosk app (
// its app ID, version and required platform version).
const char kReportRunningKioskApp[] =
    "cros.device_status.report_running_kiosk_app";

// How frequently device status reports are uploaded, in milliseconds.
const char kReportUploadFrequency[] =
    "cros.device_status.report_upload_frequency";

// A boolean pref that indicates whether user app information and activity times
// should be recorded and reported along with device policy requests.
const char kReportDeviceAppInfo[] = "cros.device_status.report_device_app_info";

// A boolean pref that determines whether the device Bluetooth information
// should be included in status reports to the device management server.
const char kReportDeviceBluetoothInfo[] =
    "cros.device_status.report_device_bluetooth_info";

// A boolean pref that determines whether the device fan information should be
// included in status reports to the device management server.
const char kReportDeviceFanInfo[] = "cros.device_status.report_device_fan_info";

// A boolean pref that determines whether the device's VPD information should be
// included in status reports to the device management server.
const char kReportDeviceVpdInfo[] = "cros.device_status.report_device_vpd_info";

// A boolean pref that determines whether the device's system information should
// be included in status reports to the device management server.
const char kReportDeviceSystemInfo[] =
    "cros.device_status.report_device_system_info";

// A boolean pref that determines whether the user's print job history is
// reported.
const char kReportDevicePrintJobs[] = "cros.device_status.report_print_jobs";

// A boolean pref that determines whether the login/logout events are reported.
const char kReportDeviceLoginLogout[] = "cros.reporting.report_login_logout";

// Determines whether CRD session events are reported.
const char kReportCRDSessions[] = "cros.reporting.report_crd_sessions";

// A boolean pref that determines whether the device runtime counters should be
// reported.
const char kDeviceReportRuntimeCounters[] =
    "cros.reporting.report_runtime_counters";

// Determines the device activity heartbeat collection rate (in milliseconds).
const char kDeviceActivityHeartbeatCollectionRateMs[] =
    "cros.reporting.device_activity_heartbeat_collection_rate_ms";

// Determines whether device activity state heartbeat should be reported.
const char kDeviceActivityHeartbeatEnabled[] =
    "cros.reporting.device_activity_heartbeat_enabled";

// Determines whether heartbeats should be sent to the policy service via
// the GCM channel.
const char kHeartbeatEnabled[] = "cros.device_status.heartbeat_enabled";

// How frequently heartbeats are sent up, in milliseconds.
const char kHeartbeatFrequency[] = "cros.device_status.heartbeat_frequency";

// Determines whether system logs should be sent to the management server.
const char kSystemLogUploadEnabled[] =
    "cros.device_status.system_log_upload_enabled";

// How frequently the networks health telemetry is collected.
const char kReportDeviceNetworkTelemetryCollectionRateMs[] =
    "cros.telemetry_reporting.report_network_telemetry_collection_rate_ms";

// How frequently the networks data are checked for events.
const char kReportDeviceNetworkTelemetryEventCheckingRateMs[] =
    "cros.telemetry_reporting.report_network_telemetry_event_checking_rate_ms";

// How frequently the audio data are checked for events.
const char kReportDeviceAudioStatusCheckingRateMs[] =
    "cros.telemetry_reporting.report_device_audio_status_checking_rate_ms";

// How frequently the runtime counters telemetry is collected.
const char kDeviceReportRuntimeCountersCheckingRateMs[] =
    "cros.telemetry_reporting.device_report_runtime_counters_checking_rate_ms";

// How frequently the audio data are checked for events.
const char kReportDeviceSignalStrengthEventDrivenTelemetry[] =
    "cros.telemetry_reporting.report_signal_strength_event_driven_telemetry";

// Determines whether the network events are reported.
const char kDeviceReportNetworkEvents[] =
    "cros.reporting.report_network_events";

// This policy should not appear in the protobuf ever but is used internally to
// signal that we are running in a "safe-mode" for policy recovery.
const char kPolicyMissingMitigationMode[] =
    "cros.internal.policy_mitigation_mode";

// A boolean pref that indicates whether users are allowed to redeem offers
// through Chrome OS Registration.
const char kAllowRedeemChromeOsRegistrationOffers[] =
    "cros.echo.allow_redeem_chrome_os_registration_offers";

// A list pref storing the feature flags (in the chrome://flags sense) that
// should to be applied at the login screen.
const char kFeatureFlags[] = "cros.feature_flags";

// A string pref for the restrict parameter to be appended to the Variations URL
// when pinging the Variations server.
const char kVariationsRestrictParameter[] =
    "cros.variations_restrict_parameter";

// TODO(b/285556135): Remove this pref together with AttestationEnabledForDevice
// A boolean pref that indicates whether enterprise attestation is enabled for
// the device.
const char kDeviceAttestationEnabled[] = "cros.device.attestation_enabled";

// A boolean pref that indicates whether attestation for content protection is
// enabled for the device.
const char kAttestationForContentProtectionEnabled[] =
    "cros.device.attestation_for_content_protection_enabled";

// The service account identity for device-level service accounts on
// enterprise-enrolled devices.
const char kServiceAccountIdentity[] = "cros.service_account_identity";

// A boolean pref that indicates whether the device has been disabled by its
// owner. If so, the device will show a warning screen and will not allow any
// sessions to be started.
const char kDeviceDisabled[] = "cros.device_disabled";

// A string pref containing the message that should be shown to the user when
// the device is disabled.
const char kDeviceDisabledMessage[] = "cros.disabled_state.message";

// A boolean pref that indicates whether the device automatically reboots when
// the user initiates a shutdown via an UI element.  If set to true, all
// shutdown buttons in the UI will be replaced by reboot buttons.
const char kRebootOnShutdown[] = "cros.device.reboot_on_shutdown";

// An integer pref that specifies the limit of the device's extension cache
// size in bytes.
const char kExtensionCacheSize[] = "cros.device.extension_cache_size";

// A dictionary pref that sets the display resolution.
// Pref format:
// {
//   "external_width": int,
//   "external_height": int,
//   "external_use_native": bool,
//   "external_scale_percentage": int,
//   "internal_scale_percentage": int,
//   "recommended": bool
// }
const char kDeviceDisplayResolution[] = "cros.device_display_resolution";
const char kDeviceDisplayResolutionKeyExternalWidth[] = "external_width";
const char kDeviceDisplayResolutionKeyExternalHeight[] = "external_height";
const char kDeviceDisplayResolutionKeyExternalScale[] =
    "external_scale_percentage";
const char kDeviceDisplayResolutionKeyExternalUseNative[] =
    "external_use_native";
const char kDeviceDisplayResolutionKeyInternalScale[] =
    "internal_scale_percentage";
const char kDeviceDisplayResolutionKeyRecommended[] = "recommended";

// An integer pref that sets the display rotation at startup to a certain
// value, overriding the user value:
// 0 = 0 degrees rotation
// 1 = 90 degrees clockwise rotation
// 2 = 180 degrees rotation
// 3 = 270 degrees clockwise rotation
const char kDisplayRotationDefault[] = "cros.display_rotation_default";

// A boolean pref that controls Chrome App Kiosk update behavior:
// false = legacy, CRX files are updated in the cache on startup using update
// URL from the policy, and from time to time during kiosk session the extension
// is updated using update URL from the extension manifest without populating
// the cache,
// true = CRX files are updated in the cache from time to time using update URL
// from the policy, no additional updates are made.
const char kKioskCRXManifestUpdateURLIgnored[] =
    "cros.kiosk_crx_manifest_update_url_ignored";

// An integer pref that sets the behavior of the login authentication flow.
// 0 = authentication using the default GAIA flow.
// 1 = authentication using an interstitial screen that offers the user to go
// ahead via the SAML IdP of the device's enrollment domain, or go back to the
// normal GAIA login flow.
const char kLoginAuthenticationBehavior[] =
    "cros.device.login_authentication_behavior";

// A boolean pref that indicates whether bluetooth should be allowed on the
// device.
const char kAllowBluetooth[] = "cros.device.allow_bluetooth";

// A boolean pref that indicates whether WiFi should be allowed on the
// device.
const char kDeviceWiFiAllowed[] = "cros.device.wifi_allowed";

// A boolean pref to enable any pings or requests to the Quirks Server.
const char kDeviceQuirksDownloadEnabled[] =
    "cros.device.quirks_download_enabled";

// A list pref storing the security origins allowed to access the webcam
// during SAML logins.
const char kLoginVideoCaptureAllowedUrls[] =
    "cros.device.login_video_capture_allowed_urls";

// A list pref specifying the locales allowed on the login screen. Currently
// only the first value is used, as the single locale allowed on the login
// screen.
const char kDeviceLoginScreenLocales[] = "cros.device_login_screen_locales";

// A list pref containing the input method IDs allowed on the login screen.
const char kDeviceLoginScreenInputMethods[] =
    "cros.device_login_screen_input_methods";

// A boolean pref that indicates whether the system information is forcedly
// shown (or hidden) on the login screen.
const char kDeviceLoginScreenSystemInfoEnforced[] =
    "cros.device_login_screen_system_info_enforced";

// A boolean pref that indicates whether to show numeric keyboard for entering
// password or not.
const char kDeviceShowNumericKeyboardForPassword[] =
    "cros.device_show_numeric_keyboard_for_password";

// A boolean pref that matches enable-per-user-time-zone chrome://flags value.
const char kPerUserTimezoneEnabled[] = "cros.flags.per_user_timezone_enabled";

// A boolean pref that matches enable-fine-grained-time-zone-detection
// chrome://flags value.
const char kFineGrainedTimeZoneResolveEnabled[] =
    "cros.flags.fine_grained_time_zone_detection_enabled";

// A dictionary pref containing time intervals and ignored policies.
// It's used to allow less restricted usage of Chrome OS during off-hours.
// This pref is set by an admin policy.
// Pref format:
// { "timezone" : string,
//   "intervals" : list of Intervals,
//   "ignored_policies" : string list }
// Interval dictionary format:
// { "start" : WeeklyTime,
//   "end" : WeeklyTime }
// WeeklyTime dictionary format:
// { "weekday" : int # value is from 1 to 7 (1 = Monday, 2 = Tuesday, etc.)
//   "time" : int # in milliseconds from the beginning of the day.
// }
const char kDeviceOffHours[] = "cros.device_off_hours";

// An enum specifying the access policy device printers should observe.
const char kDevicePrintersAccessMode[] = "cros.device.printers_access_mode";
// A list of strings representing device printer ids for which access is
// restricted.
const char kDevicePrintersBlocklist[] = "cros.device.printers_blocklist";
// A list of strings representing the list of device printer ids which are
// accessible.
const char kDevicePrintersAllowlist[] = "cros.device.printers_allowlist";

// A dictionary containing parameters controlling the TPM firmware update
// functionality.
const char kTPMFirmwareUpdateSettings[] = "cros.tpm_firmware_update_settings";

// A dictionary containing a list of entries in JSON form representing the
// minimum version of Chrome OS along with warning times required to allow user
// sign in or stay in session. If the list is empty no restrictions will be
// applied.
const char kDeviceMinimumVersion[] = "cros.device.min_version";

// String shown on the update required dialog on the the login screen containing
// return instructions from the device administrator. It is shown when update
// is required but the device has reached auto update expiration.
const char kDeviceMinimumVersionAueMessage[] =
    "cros.device.min_version_aue_message";

// String indicating what name should be advertised for casting to.
// If the string is empty or blank the system name will be used.
const char kCastReceiverName[] = "cros.device.cast_receiver.name";

// A boolean pref that indicates whether unaffiliated users are allowed to
// use ARC.
const char kUnaffiliatedArcAllowed[] = "cros.device.unaffiliated_arc_allowed";

// A boolean pref that indicates whether users are allowed to configure the
// device hostname.
const char kDeviceHostnameUserConfigurable[] =
    "cros.device.hostname_user_configurable";

// String that is used as a template for generating device hostname (that is
// used in DHCP requests).
// If the string contains either ASSET_ID, SERIAL_NUM or MAC_ADDR values,
// they will be substituted for real values.
// If the string is empty or blank, or the resulting hostname is not valid
// as per RFC 1035, then no hostname will be used.
const char kDeviceHostnameTemplate[] = "cros.network.hostname_template";

// A boolean pref that indicates whether running virtual machines on Chrome OS
// is allowed.
const char kVirtualMachinesAllowed[] = "cros.device.virtual_machines_allowed";

// A list of time intervals during which the admin has disallowed automatic
// update checks.
const char kDeviceAutoUpdateTimeRestrictions[] =
    "cros.system.autoUpdateTimeRestrictions";

// A boolean pref that indicates whether running Crostini on Chrome OS is
// allowed for unaffiliated user.
const char kDeviceUnaffiliatedCrostiniAllowed[] =
    "cros.device.unaffiliated_crostini_allowed";

// A boolean pref that indicates whether PluginVm is allowed to run on this
// device.
const char kPluginVmAllowed[] = "cros.device.plugin_vm_allowed";

// An enum pref specifying the case when device needs to reboot on user sign
// out.
const char kDeviceRebootOnUserSignout[] = "cros.device.reboot_on_user_signout";

// A boolean pref that indicates whether running wilco diagnostics and telemetry
// controller on Chrome OS is allowed.
const char kDeviceWilcoDtcAllowed[] = "cros.device.wilco_dtc_allowed";

// An enum pref that specifies the device dock MAC address source.
const char kDeviceDockMacAddressSource[] =
    "cros.device.device_dock_mac_address_source";

// A dictionary pref that mandates the recurring schedule for update checks. The
// schedule is followed even if the device is suspended, however, it's not
// respected when the device is shutdown.
const char kDeviceScheduledUpdateCheck[] =
    "cros.device.device_scheduled_update_check";

// An enum pref that configures the operation mode of the built-in 2nd factor
// authenticator.
const char kDeviceSecondFactorAuthenticationMode[] =
    "cros.device.device_second_factor_authentication_mode";

// A boolean pref specifying if the device is allowed to powerwash.
const char kDevicePowerwashAllowed[] = "cros.device.device_powerwash_allowed";

// A list pref storing URL patterns that are allowed for device attestation
// during SAML authentication.
extern const char kDeviceWebBasedAttestationAllowedUrls[] =
    "cros.device.device_web_based_attestation_allowed_urls";

// A dictionary containing parameters controlling the availability of
// System-proxy service and the web proxy credentials for system services
// connecting through System-proxy.
const char kSystemProxySettings[] = "cros.system_proxy_settings";
const char kSystemProxySettingsKeyEnabled[] = "system_proxy_enabled";
const char kSystemProxySettingsKeySystemServicesUsername[] =
    "system_services_username";
const char kSystemProxySettingsKeySystemServicesPassword[] =
    "system_services_password";
const char kSystemProxySettingsKeyAuthSchemes[] =
    "policy_credentials_auth_schemes";

// An enum pref that indicates whether adb sideloading is allowed on this device
const char kDeviceCrostiniArcAdbSideloadingAllowed[] =
    "cros.device.crostini_arc_adb_sideloading_allowed";

// A boolean pref controlling showing the low disk space notification.
const char kDeviceShowLowDiskSpaceNotification[] =
    "cros.device.show_low_disk_space_notification";

// Boolean pref indicating whether data access is enabled for
// Thunderbolt/USB4 peripherals. Enabling this pref disables the data access
// protection and will allow the aforementioned peripheral devices to be fully
// connected via PCIe tunneling.
const char kDevicePeripheralDataAccessEnabled[] =
    "cros.device.peripheral_data_access_enabled";

// A list of dictionaries indicating USB devices that may be used by chrome.usb.
const char kUsbDetachableAllowlist[] = "cros.device.usb_detachable_allowlist";
const char kUsbDetachableAllowlistKeyVid[] = "vid";
const char kUsbDetachableAllowlistKeyPid[] = "pid";

// A list pref storing bluetooth service UUIDs allowed to connect.
const char kDeviceAllowedBluetoothServices[] =
    "cros.device.allowed_bluetooth_services";

// A dictionary pref specifying the recurring schedule for device reboot.
const char kDeviceScheduledReboot[] = "cros.device.device_scheduled_reboot";

// A boolean specifying whether Chrome should operate in restricted managed
// guest session mode (block features that generate sensitive data and are not
// taken care of via clean-up mechanism in the managed guest session).
const char kDeviceRestrictedManagedGuestSessionEnabled[] =
    "cros.device.restricted_managed_guest_session_enabled";

// On reven board we collect hardware data of the device to provide relevant
// updates. A boolean pref specifies whether this data can be also used for
// overall improvements. This setting is available only on reven boards.
const char kRevenEnableDeviceHWDataUsage[] = "cros.reven.enable_hw_data_usage";

// A boolean that indicates whether the encrypted reporting pipeline is
// enabled or not.
const char kDeviceEncryptedReportingPipelineEnabled[] =
    "cros.device.encrypted_reporting_pipeline_enabled";

// A boolean pref that indicates whether reporting XDR events is enabled or not.
const char kDeviceReportXDREvents[] = "cros.device.device_report_xdr_events";

// String representing a template for the 'client-name' member of the
// 'client-info' IPP attribute that will be sent to IPP printers in case they
// support it. Maps to the `DevicePrintingClientNameTemplate` policy.
const char kDevicePrintingClientNameTemplate[] =
    "cros.device.printing.client_name_template";

// A boolean pref that indicates whether Hindi Inscript keyboard layout
// is available.
const char kDeviceHindiInscriptLayoutEnabled[] =
    "cros.device.hindi_inscript_layout_enabled";

// A list of strings representing DLC identifiers to be pre downloaded on the
// device.
const char kDeviceDlcPredownloadList[] =
    "cros.device.device_dlc_predownload_list";

}  // namespace ash
