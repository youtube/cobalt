// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/android_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/invert_bubble_prefs.h"
#include "chrome/browser/accessibility/page_colors.h"
#include "chrome/browser/accessibility/prefers_default_scrollbar_styles_prefs.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/component_updater/component_updater_prefs.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/login_detection/login_detection_prefs.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/memory/enterprise_memory_limit_pref_observer.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/secure_dns_util.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/policy/developer_tools_policy_handler.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/preloading/prefetch/prefetch_service/prefetch_origin_decider.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/push_messaging/push_messaging_app_identifier.h"
#include "chrome/browser/push_messaging/push_messaging_service_impl.h"
#include "chrome/browser/push_messaging/push_messaging_unsubscribed_entry.h"
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/serial/serial_policy_allowed_ports.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/webui/accessibility/accessibility_ui.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/policy/policy_ui.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/user_education/browser_user_education_storage_service.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/collaboration/public/pref_names.h"
#include "components/commerce/core/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/domain_reliability/domain_reliability_prefs.h"
#include "components/embedder_support/origin_trials/origin_trial_prefs.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_constants.h"
#include "components/fingerprinting_protection_filter/common/prefs.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/invalidation/impl/fcm_invalidation_service.h"
#include "components/invalidation/impl/invalidator_registrar_with_memory.h"
#include "components/invalidation/impl/per_user_topic_subscription_manager.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/lens/buildflags.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/media_device_salt/media_device_id_salt.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ntp_tiles/custom_links_manager_impl.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/page_info/core/merchant_trust_service.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/payments/core/payment_prefs.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/permissions/pref_names.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_blocklist_manager.h"
#include "components/policy/core/common/local_test_policy_provider.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/privacy_sandbox/tpcd_pref_names.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/services/on_device_translation/buildflags/buildflags.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/sharing_message/sharing_sync_preference.h"
#include "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/tpcd/metadata/browser/prefs.h"
#include "components/tracing/common/pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/update_client/update_client.h"
#include "components/variations/service/variations_service.h"
#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "components/webui/flags/pref_service_flags_storage.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "net/http/http_server_properties_manager.h"
#include "pdf/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/extensions/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui_prefs.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/pref_names.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/preinstalled_apps.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui.h"
#include "extensions/browser/api/audio/audio_api.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/device_name/device_name_store.h"
#include "chrome/browser/ash/extensions/extensions_permissions_tracker.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager.h"
#include "chrome/browser/ash/net/system_proxy_manager.h"
#include "chrome/browser/ash/performance/doze_mode_power_status_scheduler.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/policy/networking/euicc_status_uploader.h"
#include "chrome/browser/ash/policy/remote_commands/crd/crd_admin_session_controller.h"
#include "chrome/browser/ash/settings/hardware_data_usage_controller.h"
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "chrome/browser/ash/system_web_apps/apps/media_app/media_app_guest_ui_config.h"
#include "chrome/browser/component_updater/metadata_table_chromeos.h"
#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"
#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_login_handler.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_handler_impl.h"
#include "chromeos/ash/components/carrier_lock/carrier_lock_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_pref_names.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/accessibility/accessibility_prefs/android/accessibility_prefs_controller.h"
#include "chrome/browser/android/ntp/recent_tabs_page_prefs.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "chrome/browser/android/preferences/browser_prefs_android.h"
#include "chrome/browser/android/preferences/shared_preferences_migrator_android.h"
#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"
#include "chrome/browser/first_run/android/first_run_prefs.h"
#include "chrome/browser/lens/android/lens_prefs.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "chrome/browser/notifications/notification_channels_provider_android.h"
#include "chrome/browser/partnerbookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/password_manager/android/password_manager_android_util.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "chrome/browser/readaloud/android/prefs.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck crbug.com/1125897
#include "components/feed/core/common/pref_names.h"         // nogncheck
#include "components/feed/core/shared_prefs/pref_names.h"   // nogncheck
#include "components/feed/core/v2/ios_shared_prefs.h"       // nogncheck
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/webapps/browser/android/install_prompt_prefs.h"
#else  // BUILDFLAG(IS_ANDROID)
#include "chrome/browser/device_api/device_service_impl.h"
#include "chrome/browser/gcm/gcm_product_util.h"
#include "chrome/browser/hid/hid_policy_allowed_devices.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_prefs.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files_page_handler.h"
#include "chrome/browser/new_tab_page/modules/safe_browsing/safe_browsing_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/promos/promos_utils.h"  // nogncheck crbug.com/1125897
#include "chrome/browser/screen_ai/pref_names.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/organization/prefs.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_pref_names.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/views/side_panel/side_panel_prefs.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/theme_color_picker_handler.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "components/headless/policy/headless_mode_prefs.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_translate_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
#include "chrome/browser/devtools/devtools_window.h"
#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "chrome/browser/apps/app_discovery_service/almanac_fetcher.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/ash/account_manager/account_apps_availability.h"
#include "chrome/browser/ash/account_manager/account_manager_edu_coexistence_controller.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/ash/arc/boot_phase_monitor/arc_boot_phase_monitor_bridge.h"
#include "chrome/browser/ash/arc/policy/arc_policy_bridge.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/bluetooth/debug_logs_manager.h"
#include "chrome/browser/ash/bluetooth/hats_bluetooth_revamp_trigger_impl.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/bruschetta/bruschetta_pref_names.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_common.h"
#include "chrome/browser/ash/child_accounts/family_user_chrome_activity_metrics.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"
#include "chrome/browser/ash/child_accounts/family_user_session_metrics.h"
#include "chrome/browser/ash/child_accounts/parent_access_code/parent_access_service.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/cryptauth/client_app_metadata_provider_service.h"
#include "chrome/browser/ash/cryptauth/cryptauth_device_id_provider_impl.h"
#include "chrome/browser/ash/customization/customization_document.h"
#include "chrome/browser/ash/file_manager/file_manager_pref_names.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_system_provider/registry.h"
#include "chrome/browser/ash/first_run/first_run.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/guest_os/guest_os_terminal.h"
#include "chrome/browser/ash/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/ash/login/quick_unlock/pin_storage_prefs.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/reporting/login_logout_reporter.h"
#include "chrome/browser/ash/login/saml/saml_profile_prefs.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ash/login/signin/legacy_token_handle_fetcher.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_prefs.h"
#include "chrome/browser/ash/login/users/avatar/user_image_sync_observer.h"
#include "chrome/browser/ash/net/ash_proxy_monitor.h"
#include "chrome/browser/ash/net/network_throttling_observer.h"
#include "chrome/browser/ash/net/secure_dns_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include "chrome/browser/ash/policy/external_data/handlers/device_wallpaper_image_external_data_handler.h"
#include "chrome/browser/ash/policy/handlers/adb_sideloading_allowance_mode_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/policy/handlers/tpm_auto_update_mode_policy_handler.h"
#include "chrome/browser/ash/policy/reporting/app_install_event_log_manager_wrapper.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/reboot_notifications_scheduler.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/policy/status_collector/status_collector.h"
#include "chrome/browser/ash/power/auto_screen_brightness/metrics_reporter.h"
#include "chrome/browser/ash/power/power_metrics_reporter.h"
#include "chrome/browser/ash/preferences/preferences.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ash/printing/enterprise/enterprise_printers_provider.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/ash/scanning/chrome_scanning_app_delegate.h"
#include "chrome/browser/ash/system/automatic_reboot_manager.h"
#include "chrome/browser/ash/system/input_device_settings.h"
#include "chrome/browser/ash/system_web_apps/apps/help_app/help_app_notification_controller.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_prefs.h"
#include "chrome/browser/chromeos/enterprise/cloud_storage/pref_utils.h"
#include "chrome/browser/chromeos/extensions/echo_private/echo_private_api_util.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_prefs.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/device_identity/chromeos/device_oauth2_token_store_chromeos.h"
#include "chrome/browser/extensions/api/document_scan/profile_prefs_registry_util.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_registry_util.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/memory/oom_kills_monitor.h"
#include "chrome/browser/metrics/chromeos_metrics_provider.h"
#include "chrome/browser/policy/annotations/blocklist_handler.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_prefs.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_ui.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/upgrade_detector/upgrade_detector_chromeos.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_impl.h"
#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"
#include "chromeos/ash/components/boca/on_task/on_task_prefs.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "chromeos/ash/components/local_search_service/search_metrics_reporter.h"
#include "chromeos/ash/components/network/cellular_esim_profile_handler_impl.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "chromeos/ash/components/network/fast_transition_observer.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/policy/restriction_schedule/device_restriction_schedule_controller.h"
#include "chromeos/ash/components/policy/system_features_disable_list/system_features_disable_list_policy_utils.h"
#include "chromeos/ash/components/quickoffice/quickoffice_prefs.h"
#include "chromeos/ash/components/report/report_controller.h"
#include "chromeos/ash/components/scheduler_config/scheduler_configuration_manager.h"
#include "chromeos/ash/components/settings/device_settings_cache.h"
#include "chromeos/ash/components/timezone/timezone_resolver.h"
#include "chromeos/ash/experiences/arc/arc_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/bluetooth_config/bluetooth_power_controller_impl.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager_impl.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_prefs.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/ui/wm/fullscreen/pref_names.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/onc/onc_pref_names.h"  // nogncheck
#include "components/quirks/quirks_manager.h"
#include "components/user_manager/user_manager_impl.h"
#if BUILDFLAG(USE_CUPS)
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#endif  // BUILDFLAG(USE_CUPS)
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/font_prewarmer_tab_helper.h"
#include "chrome/browser/media/cdm_pref_service_helper.h"
#include "chrome/browser/media/media_foundation_service_monitor.h"
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#include "chrome/browser/win/conflicts/third_party_conflicts_manager.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"
#include "components/os_crypt/sync/os_crypt.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "components/device_signals/core/browser/pref_names.h"  // nogncheck due to crbug.com/1125897
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/startup/first_run_service.h"
#endif

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
#include "chrome/browser/downgrade/downgrade_prefs.h"
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_service_log.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/color/system_theme.h"
#endif

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
#include "chrome/browser/on_device_translation/pref_names.h"
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "components/enterprise/data_controls/core/browser/prefs.h"
#endif

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/common/file_type_policies_prefs.h"
#endif

namespace {

// Please keep the list of deprecated prefs in chronological order. i.e. Add to
// the bottom of the list, not here at the top.

#if BUILDFLAG(IS_WIN)
// Deprecated 06/2024
constexpr char kOsCryptAppBoundFixedData3PrefName[] =
    "os_crypt.app_bound_fixed_data3";
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 05/2024.
// A preference to keep track of the device registered time.
constexpr char kDeviceRegisteredTime[] = "DeviceRegisteredTime";
constexpr char kArcKioskDictionaryName[] = "arc-kiosk";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
// Deprecated 05/2024
inline constexpr char kSearchEnginesStudyGroup[] =
    "search_engines.client_side_study_group";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Deprecated 05/2024
// Pref name for the whether whats new refresh page has been shown
// successfully.
inline constexpr char kHasShownRefreshWhatsNew[] =
    "browser.has_shown_refresh_2023_whats_new";
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 05/2024
// A boolean pref which determines if you can pause mouse keys with a
// keyboard shortcut.
inline constexpr char kAccessibilityMouseKeysShortcutToPauseEnabled[] =
    "settings.a11y.mouse_keys.ctrl_to_pause_enabled";
// A boolean pref which determines if mouse keys is automatically disabled in
// text fields.
inline constexpr char kAccessibilityMouseKeysDisableInTextFields[] =
    "settings.a11y.mouse_keys.disable_in_text_fields";
// A boolean pref which determines whether screen magnifier should center
// the text input focus.
inline constexpr char kAccessibilityScreenMagnifierCenterFocus[] =
    "settings.a11y.screen_magnifier_center_focus";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Deprecated 05/2024.
constexpr char kBlockTruncatedCookies[] = "profile.cookie_block_truncated";

// Deprecated 05/2024
inline constexpr char kDefaultSearchProviderChoiceLocationPrefName[] =
    "default_search_provider_data.choice_location";

// Deprecated 05/2024.
inline constexpr char kSyncCachedTrustedVaultAutoUpgradeDebugInfo[] =
    "sync.cached_trusted_vault_auto_upgrade_debug_info";

// Deprecated 05/2024.
inline constexpr char kAutologinEnabled[] = "autologin.enabled";
inline constexpr char kReverseAutologinRejectedEmailList[] =
    "reverse_autologin.rejected_email_list";

// Deprecated 06/2024.
inline constexpr char kTrackingProtectionOnboardingNoticeFirstRequested[] =
    "tracking_protection.tracking_protection_onboarding_notice_first_requested";
inline constexpr char kTrackingProtectionOnboardingNoticeLastRequested[] =
    "tracking_protection.tracking_protection_onboarding_notice_last_requested";

// Deprecated 06/2024.
#if !BUILDFLAG(IS_ANDROID)
inline constexpr char kAccessibilityReadAnythingOmniboxIconLabelShownCount[] =
    "settings.a11y.read_anything.omnibox_icon_label_shown_count";

// Deprecated 06/2024.
inline constexpr char kAccessibilityPdfOcrAlwaysActive[] =
    "settings.a11y.pdf_ocr_always_active";
#endif

// Deprecated 06/2024.
inline constexpr char kTrackingProtectionOffboarded[] =
    "tracking_protection.tracking_protection_offboarded";
inline constexpr char kTrackingProtectionOffboardedSince[] =
    "tracking_protection.tracking_protection_offboarded_since";
inline constexpr char kTrackingProtectionOffboardingAckAction[] =
    "tracking_protection.tracking_protection_offboarding_ack_action";

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 06/2024.
constexpr std::array<const char*, 12u>
    kHoldingSpaceWallpaperNudgeTimesOfFirstInteraction = {
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "DroppedFileOnHoldingSpace.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "DroppedFileOnWallpaper.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "DraggedFileOverWallpaper.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time.OpenedHoldingSpace."
        "first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "PinnedFileFromAnySource.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "PinnedFileFromContextMenu.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "PinnedFileFromFilesApp.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "PinnedFileFromHoldingSpaceDrop.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "PinnedFileFromPinButton.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time."
        "PinnedFileFromWallpaperDrop.first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time.UsedOtherItem."
        "first_time",
        "ash.holding_space.wallpaper_nudge.interaction_time.UsedPinnedItem."
        "first_time",
};

// Deprecated 06/2024.
constexpr char kHoldingSpaceWallpaperNudgeLastTimeNudgeShownCounterfactual[] =
    "ash.holding_space.wallpaper_nudge.last_shown_time_counterfactual";
constexpr char kHoldingSpaceWallpaperNudgeLastTimeNudgeShown[] =
    "ash.holding_space.wallpaper_nudge.last_shown_time";
constexpr char kHoldingSpaceWallpaperNudgeNudgeShownCountCounterfactual[] =
    "ash.holding_space.wallpaper_nudge.shown_count_counterfactual";
constexpr char kHoldingSpaceWallpaperNudgeNudgeShownCount[] =
    "ash.holding_space.wallpaper_nudge.shown_count";
constexpr char kHoldingSpaceWallpaperNudgeUserEligibleForNudge[] =
    "ash.holding_space.wallpaper_nudge.user_eligible";
constexpr char kHoldingSpaceWallpaperNudgeUserFirstEligibleSessionTime[] =
    "ash.holding_space.wallpaper_nudge.first_eligible_session_time";

// Deprecated 06/2024.
constexpr char kLocalUserFilesMigrationEnabled[] =
    "filebrowser.local_user_files_migration_enabled";

// Deprecated 06/2024.
constexpr char kBirchUseRecentTabs[] = "ash.birch.use_recent_tabs";
constexpr char kBirchUseLastActive[] = "ash.birch.use_last_active";
constexpr char kBirchUseMostVisited[] = "ash.birch.use_most_visited";
constexpr char kBirchUseSelfShare[] = "ash.birch.use_self_share";
#endif

// Deprecated 06/2024
constexpr char kDefaultSearchProviderChoicePending[] =
    "default_search_provider.engine_choice_pending";

// Deprecated 07/2024
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
inline constexpr char kFirstRunStudyGroup[] = "browser.first_run_study_group";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Deprecated 07/2024
constexpr char kNtpRecipesDismissedTasks[] = "NewTabPage.DismissedRecipeTasks";
constexpr char kNtpModulesFirstShownTime[] = "NewTabPage.ModulesFirstShownTime";
constexpr char kNtpModulesFreVisible[] = "NewTabPage.ModulesFreVisible";
constexpr char kNtpModulesShownCount[] = "NewTabPage.ModulesShownCount";
constexpr char kNtpPhotosLastDismissedTimePrefName[] =
    "NewTabPage.Photos.LastDimissedTime";
constexpr char kNtpPhotosOptInAcknowledgedPrefName[] =
    "NewTabPage.Photos.OptInAcknowledged";
constexpr char kNtpPhotosLastMemoryOpenTimePrefName[] =
    "NewTabPage.Photos.LastMemoryOpenTime";
constexpr char kNtpPhotosSoftOptOutCountPrefName[] =
    "NewTabPage.Photos.SoftOptOutCount";
constexpr char kNtpPhotosLastSoftOptedOutTimePrefName[] =
    "NewTabPage.Photos.LastSoftOptedoutTime";
// Deprecated 08/2024
constexpr char kDismissedTabsPrefName[] =
    "NewTabPage.TabResumption.DismissedTabs";
#endif

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 07/2024.
constexpr char kAppDeduplicationServiceLastGetTimestamp[] =
    "apps.app_deduplication_service.last_get_data_from_server_timestamp";
// Deprecated 07/2024.
constexpr char kShowTunaScreenEnabled[] = "ash.tuna_screen_oobe_enabled";
// Deprecated 08/2024.
inline constexpr char kStandaloneWindowMigrationNudgeShown[] =
    "standalone_window_migration_nudge_shown";
#endif

#if BUILDFLAG(IS_ANDROID)
// All of these were deprecated 08/24.
constexpr char kObsoleteUnenrolledFromGoogleMobileServicesAfterApiErrorCode[] =
    "unenrolled_from_google_mobile_services_after_api_error_code";
constexpr char kObsoleteRequiresMigrationAfterSyncStatusChange[] =
    "requires_migration_after_sync_status_change";
constexpr char
    kObsoleteUnenrolledFromGoogleMobileServicesWithErrorListVersion[] =
        "unenrolled_from_google_mobile_services_with_error_list_version";
constexpr char kObsoleteTimesReenrolledToGoogleMobileServices[] =
    "times_reenrolled_to_google_mobile_services";
constexpr char kObsoleteTimesAttemptedToReenrollToGoogleMobileServices[] =
    "times_attempted_to_reenroll_to_google_mobile_services";
constexpr char kObsoleteUserReceivedGMSCoreError[] =
    "user_received_gmscore_error";
#endif

// Deprecated 08/2024.
constexpr char kSafeBrowsingEsbOptInWithFriendlierSettings[] =
    "safebrowsing.esb_opt_in_with_friendlier_settings";

// Deprecated 08/2024.
#if BUILDFLAG(IS_CHROMEOS)
constexpr char kDeviceDMTokenV1[] = "device_dm_token";
constexpr char kDeviceDMTokenV2[] = "device_dm_token_v2";
#endif

// Deprecated 08/2024
#if BUILDFLAG(IS_ANDROID)
constexpr char kBackoffEntryKey[] = "query_tiles.backoff_entry_key";
constexpr char kFirstScheduleTimeKey[] = "query_tiles.first_schedule_time_key";
#endif

// Deprecated 09/2024.
constexpr char kContentSettingsWindowLastTabIndex[] =
    "content_settings_window.last_tab_index";
constexpr char kSyncPasswordHash[] = "profile.sync_password_hash";
constexpr char kSyncPasswordLengthAndHashSalt[] =
    "profile.sync_password_length_and_hash_salt";

// Deprecated 09/2024
#if BUILDFLAG(IS_CHROMEOS)
constexpr char kDemoModeResourcesRemoved[] = "demo_mode_resources_removed";
constexpr char kAccumulatedUsagePref[] =
    "demo_mode_resources_remover.accumulated_device_usage_s";
#endif

// Deprecated 09/2024
#if !BUILDFLAG(IS_ANDROID)
constexpr char kPasswordGenerationNudgePasswordDismissCount[] =
    "password_generation_nudge_password_dismiss_count";
#endif  // !BUILDFLAG(IS_ANDROID)

// Deprecated 09/2024
#if !BUILDFLAG(IS_ANDROID)
const char kTranslateKitRootDir[] =
    "on_device_translation.translate_kit_root_dir";
#endif

// Deprecated 09/2024
#if BUILDFLAG(IS_ANDROID)
constexpr char kPrivacySandboxActivityTypeRecord[] =
    "privacy_sandbox.activity_type.record";
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 09/2024.
#if !BUILDFLAG(IS_ANDROID)
const char kTabResumeDismissedTabsPrefName[] =
    "NewTabPage.MostRelevantTabResumption.DismissedTabs";
#endif  // !BUILDFLAG(IS_ANDROID)

// Deprecated 10/2024.
constexpr char kLiveCaptionBubblePinned[] =
    "accessibility.captions.live_caption_bubble_pinned";

// Deprecated 10/2024.
#if BUILDFLAG(IS_CHROMEOS)
const char kMigrationStep[] = "ash.browser_data_migrator.migration_step";
const char kMoveMigrationResumeStepPref[] =
    "ash.browser_data_migrator.move_migration_resume_step";
const char kMoveMigrationResumeCountPref[] =
    "ash.browser_data_migrator.move_migration_resume_count";
const char kLacrosSecondaryProfilesAllowed[] =
    "lacros_secondary_profiles_allowed";
constexpr char kDataVerPref[] = "lacros.data_version";
constexpr char kMigrationAttemptCountPref[] =
    "ash.browser_data_migrator.migration_attempt_count";
constexpr char kProfileMigrationCompletedForUserPref[] =
    "lacros.profile_migration_completed_for_user";
constexpr char kProfileMoveMigrationCompletedForUserPref[] =
    "lacros.profile_move_migration_completed_for_user";
constexpr char kProfileMigrationCompletedForNewUserPref[] =
    "lacros.profile_migration_completed_for_new_user";
const char kProfileDataBackwardMigrationCompletedForUserPref[] =
    "lacros.profile_data_backward_migration_completed_for_user";
const char kGotoFilesPref[] = "lacros.goto_files";
const char kProfileMigrationCompletionTimeForUserPref[] =
    "lacros.profile_migration_completion_time_for_user";
const char kLacrosDataBackwardMigrationMode[] =
    "lacros_data_backward_migration_mode";
#endif

#if !BUILDFLAG(IS_ANDROID)
// Deprecated 10/2024
// Pref name for the percent threshold to show HaTS on the What's New page.
inline constexpr char kWhatsNewHatsActivationThreshold[] =
    "browser.whats_new_hats_activation_threshold";
#endif

// Deprecated 10/2024.
const char kModelExecutionMainToggleSettingState[] =
    "optimization_guide.model_execution_main_toggle_setting_state";

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 10/2024
// An integer pref which determines how much FaceGaze should smooth cursor
// movements.
inline constexpr char kAccessibilityFaceGazeCursorSmoothing[] =
    "settings.a11y.face_gaze.cursor_smoothing";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Deprecated 10/2024.
const char kBeforeunloadEventCancelByPreventDefaultEnabled[] =
    "policy.beforeunload_event_cancel_by_prevent_default_enabled";

// Deprecated 10/2024.
inline constexpr char kDocumentSuggestEnabled[] = "documentsuggest.enabled";

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 10/2024
inline constexpr char kWallpaperSeaPenMigrationStatus[] =
    "ash.wallpaper.sea_pen.migration_status";
#endif

// Deprecated 10/2024
inline constexpr char kFirstTimeInterstitialBannerState[] =
    "profile.managed.banner_state";

// Deprecated 10/2024
inline constexpr char kSidePanelCompanionEntryPinnedToToolbar[] =
    "side_panel.companion_pinned_to_toolbar";
inline constexpr char kMsbbPromoDeclinedCountPref[] =
    "Companion.Promo.MSBB.Declined.Count";
inline constexpr char kSigninPromoDeclinedCountPref[] =
    "Companion.Promo.Signin.Declined.Count";
inline constexpr char kExpsPromoDeclinedCountPref[] =
    "Companion.Promo.Exps.Declined.Count";
inline constexpr char kExpsPromoShownCountPref[] =
    "Companion.Promo.Exps.Shown.Count";
inline constexpr char kPcoPromoShownCountPref[] =
    "Companion.Promo.PCO.Shown.Count";
inline constexpr char kPcoPromoDeclinedCountPref[] =
    "Companion.Promo.PCO.Declined.Count";
inline constexpr char kExpsOptInStatusGrantedPref[] =
    "Companion.Exps.OptIn.Status.Granted";
inline constexpr char kHasNavigatedToExpsSuccessPage[] =
    "Companion.HasNavigatedToExpsSuccessPage";

// Deprecated 11/2024.
#if BUILDFLAG(IS_CHROMEOS)
constexpr char kNoteTakingAppEnabledOnLockScreen[] =
    "settings.note_taking_app_enabled_on_lock_screen";
constexpr char kNoteTakingAppsLockScreenAllowlist[] =
    "settings.note_taking_apps_lock_screen_whitelist";
constexpr char kNoteTakingAppsLockScreenToastShown[] =
    "settings.note_taking_apps_lock_screen_toast_shown";
constexpr char kRestoreLastLockScreenNote[] =
    "settings.restore_last_lock_screen_note";
constexpr char kLockScreenDataPrefKey[] = "lockScreenDataItems";
inline constexpr char kSyncableVersionedWallpaperInfo[] =
    "syncable_versioned_wallpaper_info";
constexpr char kLacrosProxyControllingExtension[] =
    "ash.lacros_proxy_controlling_extension";
#endif

// Deprecated 11/2024
constexpr char kPrefixedVideoFullscreenApiAvailability[] =
    "media.prefixed_fullscreen_video_api_availability";

// Deprecated 11/2024
constexpr char kOnDeviceModelTimeoutCount[] =
    "optimization_guide.on_device.timeout_count";

#if !BUILDFLAG(IS_ANDROID)
// Deprecated 11/2024
inline constexpr char kCartModuleHidden[] = "cart_module_hidden";
inline constexpr char kCartModuleWelcomeSurfaceShownTimes[] =
    "cart_module_welcome_surface_shown_times";
inline constexpr char kCartDiscountAcknowledged[] =
    "cart_discount_acknowledged";
inline constexpr char kCartDiscountEnabled[] = "cart_discount_enabled";
inline constexpr char kCartUsedDiscounts[] = "cart_used_discounts";
inline constexpr char kCartDiscountLastFetchedTime[] =
    "cart_discount_last_fetched_time";
inline constexpr char kCartDiscountConsentShown[] =
    "cart_discount_consent_shown";
inline constexpr char kDiscountConsentDecisionMadeIn[] =
    "discount_consent_decision_made_in";
inline constexpr char kDiscountConsentDismissedIn[] =
    "discount_consent_dismissed_in";
inline constexpr char kDiscountConsentLastDimissedTime[] =
    "discount_consent_last_dimissed_time";
inline constexpr char kDiscountConsentLastShownInVariation[] =
    "discount_consent_last_shown_in";
inline constexpr char kDiscountConsentPastDismissedCount[] =
    "discount_consent_dismissed_count";
inline constexpr char kDiscountConsentShowInterest[] =
    "discount_consent_show_interest";
inline constexpr char kDiscountConsentShowInterestIn[] =
    "discount_consent_show_interest_in";
#endif  // !BUILDFLAG(IS_ANDROID)

// Deprecated 11/2024
constexpr char kQuietNotificationPermissionPromoWasShown[] =
    "profile.content_settings.quiet_permission_ui_promo.was_shown."
    "notifications";
constexpr char kQuietNotificationPermissionShouldShowPromo[] =
    "profile.content_settings.quiet_permission_ui_promo.should_show."
    "notifications";
inline constexpr char kQuietNotificationPermissionUiEnablingMethod[] =
    "profile.content_settings.enable_quiet_permission_ui_enabling_method."
    "notifications";

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 11/2024
inline constexpr char kHatsPrivacyHubPostLaunchIsSelected[] =
    "hats_privacy_hub_postlaunch_is_selected";
inline constexpr char kHatsPrivacyHubPostLaunchCycleEndTs[] =
    "hats_privacy_hub_postlaunch_end_timestamp";
#endif

// Deprecated 12/2024.
inline constexpr char kDeleteTimePeriodV2[] =
    "browser.clear_data.time_period_v2";
inline constexpr char kDeleteTimePeriodV2Basic[] =
    "browser.clear_data.time_period_v2_basic";

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 12/2024
inline const char kCryptAuthDeviceSyncIsRecoveringFromFailure[] =
    "cryptauth.device_sync.is_recovering_from_failure";
inline const char kCryptAuthDeviceSyncLastSyncTimeSeconds[] =
    "cryptauth.device_sync.last_device_sync_time_seconds";
inline const char kCryptAuthDeviceSyncReason[] = "cryptauth.device_sync.reason";
inline const char kCryptAuthDeviceSyncUnlockKeys[] =
    "cryptauth.device_sync.unlock_keys";
inline const char kCryptAuthEnrollmentIsRecoveringFromFailure[] =
    "cryptauth.enrollment.is_recovering_from_failure";
inline const char kCryptAuthEnrollmentLastEnrollmentTimeSeconds[] =
    "cryptauth.enrollment.last_enrollment_time_seconds";
inline const char kCryptAuthEnrollmentReason[] = "cryptauth.enrollment.reason";
inline const char kCryptAuthEnrollmentUserPublicKey[] =
    "cryptauth.enrollment.user_public_key";
inline const char kCryptAuthEnrollmentUserPrivateKey[] =
    "cryptauth.enrollment.user_private_key";
inline const char kLacrosLaunchOnLogin[] = "lacros.launch_on_login";
inline const char kLacrosLaunchSwitch[] = "lacros_launch_switch";
inline const char kLacrosSelection[] = "lacros_selection";
#endif

// Deprecated 12/2024.
inline constexpr char kPageContentCollectionEnabled[] =
    "page_content_collection.enabled";

// Deprecated 01/2025.
inline constexpr char kCompactModeEnabled[] = "compact_mode";

// Deprecated 01/2025.
inline constexpr char kSafeBrowsingAutomaticDeepScanningIPHSeen[] =
    "safebrowsing.automatic_deep_scanning_iph_seen";
inline constexpr char kSafeBrowsingAutomaticDeepScanPerformed[] =
    "safe_browsing.automatic_deep_scan_performed";

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 01/2025.
inline constexpr char kUsedPolicyCertificates[] =
    "policy.used_policy_certificates";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Deprecated 02/2025.
inline constexpr char kUserAgentClientHintsGREASEUpdateEnabled[] =
    "policy.user_agent_client_hints_grease_update_enabled";

// Deprecated 02/2025.
inline constexpr char kDefaultSearchProviderKeywordsUseExtendedList[] =
    "default_search_provider.keywords_use_extended_list";

#if BUILDFLAG(IS_ANDROID)
// Deprecated 2/2025.
inline constexpr char kLocalPasswordsMigrationWarningShownTimestamp[] =
    "local_passwords_migration_warning_shown_timestamp";
inline constexpr char kLocalPasswordMigrationWarningShownAtStartup[] =
    "local_passwords_migration_warning_shown_at_startup";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 2/2025.
inline constexpr char kLiveCaptionUserMicrophoneEnabled[] =
    "accessibility.captions.user_microphone_captioning_enabled";
inline constexpr char kUserMicrophoneCaptionLanguageCode[] =
    "accessibility.captions.user_microphone_language_code";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Deprecated 03/2025.
inline constexpr char kPasswordChangeFlowNoticeAgreement[] =
    "password_manager.password_change_flow_notice_agreement";

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 02/2025.
constexpr char kScannerFeedbackEnabled[] = "ash.scanner.feedback_enabled";
constexpr char kHmrFeedbackAllowed[] = "settings.mahi_feedback_allowed";
constexpr char kSharedStorage[] = "shared_storage";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Deprecated 02/2025.
inline constexpr char kRootSecretPrefName[] =
    "webauthn.authenticator_root_secret";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 03/2025.
inline constexpr char kShouldAutoEnroll[] = "ShouldAutoEnroll";
inline constexpr char kShouldRetrieveDeviceState[] =
    "ShouldRetrieveDeviceState";
inline constexpr char kAutoEnrollmentPowerLimit[] = "AutoEnrollmentPowerLimit";
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
// Deprecated 03/2025.
inline constexpr char kDeviceRestrictionScheduleHighestSeenTime[] =
    "device_restriction_schedule_highest_seen_time";
constexpr char kSunfishEnabled[] = "ash.capture_mode.sunfish_enabled";
#endif  // BUILDFLAG(IS_CHROMEOS)

// Deprecated 03/2025.
inline constexpr char kRecurrentSSLInterstitial[] =
    "profile.ssl_recurrent_interstitial";

// Deprecated 04/2025.
inline constexpr char kDefaultSearchProviderChoiceScreenShuffleMilestone[] =
    "default_search_provider.choice_screen_shuffle_milestone";
#if !BUILDFLAG(IS_ANDROID)
inline char kPerformanceInterventionNotificationAcceptHistoryDeprecated[] =
    "performance_tuning.intervention_notification.accept_history";
#endif  // !BUILDFLAG(IS_ANDROID)

// Deprecated 04/2025.
inline constexpr char kAddedBookmarkSincePowerBookmarksLaunch[] =
    "bookmarks.added_since_power_bookmarks_launch";
inline constexpr char kGlicRolloutEligibility[] = "glic.rollout_eligibility";

// Deprecated 04/2025.
inline constexpr char kManagedAccessToGetAllScreensMediaAllowedForUrls[] =
    "profile.managed_access_to_get_all_screens_media_allowed_for_urls";

#if BUILDFLAG(IS_ANDROID)
// Deprecated 04/2025.
constexpr char kObsoleteUserAcknowledgedLocalPasswordsMigrationWarning[] =
    "user_acknowledged_local_passwords_migration_warning";

// Deprecated 04/2025.
constexpr char kObsoleteLocalPasswordMigrationWarningPrefsVersion[] =
    "local_passwords_migration_warning_reset_count";
#endif

// Deprecated 04/2025.
inline constexpr char kSuggestionGroupVisibility[] =
    "omnibox.suggestionGroupVisibility";

#if BUILDFLAG(IS_ANDROID)
// Deprecated 05/2025.
inline constexpr char kWipedWebAPkDataForMigration[] =
    "sync.wiped_web_apk_data_for_migration";
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 05/2025.
inline constexpr char kSyncCacheGuid[] = "sync.cache_guid";
inline constexpr char kSyncBirthday[] = "sync.birthday";
inline constexpr char kSyncBagOfChips[] = "sync.bag_of_chips";
inline constexpr char kSyncLastSyncedTime[] = "sync.last_synced_time";
inline constexpr char kSyncLastPollTime[] = "sync.last_poll_time";
inline constexpr char kSyncPollInterval[] = "sync.short_poll_interval";

// Register local state used only for migration (clearing or moving to a new
// key).
void RegisterLocalStatePrefsForMigration(PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_WIN)
  // Deprecated 06/2024.
  registry->RegisterStringPref(kOsCryptAppBoundFixedData3PrefName,
                               std::string());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 05/2024.
  registry->RegisterTimePref(kDeviceRegisteredTime, base::Time());
  registry->RegisterDictionaryPref(kArcKioskDictionaryName);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 05/2024.
  registry->RegisterStringPref(kSearchEnginesStudyGroup, std::string());
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 05/2024.
  registry->RegisterBooleanPref(kHasShownRefreshWhatsNew, false);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 06/2024.
  registry->RegisterBooleanPref(kLocalUserFilesMigrationEnabled, false);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Deprecated 07/2024.
  registry->RegisterStringPref(kFirstRunStudyGroup, std::string());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 08/2024.
  registry->RegisterStringPref(kDeviceDMTokenV1, std::string());
  registry->RegisterStringPref(kDeviceDMTokenV2, std::string());
  registry->RegisterBooleanPref(kDemoModeResourcesRemoved, false);
  registry->RegisterIntegerPref(kAccumulatedUsagePref, 0);

  // Deprecated 10/2024.
  registry->RegisterIntegerPref(kMigrationStep, 0);
  registry->RegisterDictionaryPref(kMoveMigrationResumeStepPref);
  registry->RegisterDictionaryPref(kMoveMigrationResumeCountPref);
  registry->RegisterDictionaryPref(kDataVerPref);
  registry->RegisterDictionaryPref(kMigrationAttemptCountPref);
  registry->RegisterDictionaryPref(kProfileMigrationCompletedForUserPref);
  registry->RegisterDictionaryPref(kProfileMoveMigrationCompletedForUserPref);
  registry->RegisterDictionaryPref(kProfileMigrationCompletedForNewUserPref);
  registry->RegisterDictionaryPref(
      kProfileDataBackwardMigrationCompletedForUserPref);
  registry->RegisterListPref(kGotoFilesPref);
  registry->RegisterDictionaryPref(kProfileMigrationCompletionTimeForUserPref);
  registry->RegisterStringPref(kLacrosDataBackwardMigrationMode, "");
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 10/2024
  registry->RegisterIntegerPref(kWhatsNewHatsActivationThreshold, 100);
#endif

  // Deprecated 10/2024.
  registry->RegisterBooleanPref(kBeforeunloadEventCancelByPreventDefaultEnabled,
                                true);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 11/2024.
  registry->RegisterDictionaryPref(kLockScreenDataPrefKey);
#endif

  // Deprecated 11/2024.
  registry->RegisterIntegerPref(kOnDeviceModelTimeoutCount, 0);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 12/2024.
  registry->RegisterIntegerPref(kLacrosLaunchSwitch, 0);
  registry->RegisterIntegerPref(kLacrosSelection, 0);
#endif

  // Deprecated 02/2025.
  registry->RegisterBooleanPref(kUserAgentClientHintsGREASEUpdateEnabled, true);

#if BUILDFLAG(IS_ANDROID)
  // Deprecated 02/2025.
  registry->RegisterStringPref(kRootSecretPrefName, std::string());
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 03/2025.
  registry->RegisterBooleanPref(kShouldRetrieveDeviceState, false);
  registry->RegisterBooleanPref(kShouldAutoEnroll, false);
  registry->RegisterIntegerPref(kAutoEnrollmentPowerLimit, -1);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 03/2025.
  registry->RegisterTimePref(kDeviceRestrictionScheduleHighestSeenTime,
                             base::Time());
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 04/2025.
  registry->RegisterListPref(
      kPerformanceInterventionNotificationAcceptHistoryDeprecated);
#endif
}

// Register prefs used only for migration (clearing or moving to a new key).
void RegisterProfilePrefsForMigration(
    user_prefs::PrefRegistrySyncable* registry) {
  chrome_browser_net::secure_dns::RegisterProbesSettingBackupPref(registry);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 05/2024.
  registry->RegisterBooleanPref(kAccessibilityMouseKeysShortcutToPauseEnabled,
                                true);
  registry->RegisterBooleanPref(kAccessibilityMouseKeysDisableInTextFields,
                                true);
  registry->RegisterBooleanPref(kAccessibilityScreenMagnifierCenterFocus, true);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Deprecated 05/2024.
  registry->RegisterBooleanPref(kBlockTruncatedCookies, true);

  // Deprecated 05/2024
  registry->RegisterIntegerPref(
      kDefaultSearchProviderChoiceLocationPrefName,
      static_cast<int>(search_engines::ChoiceMadeLocation::kOther));

  // Deprecated 05/2024.
  registry->RegisterStringPref(kSyncCachedTrustedVaultAutoUpgradeDebugInfo, "");

  // Deprecated 05/2024.
  registry->RegisterBooleanPref(kAutologinEnabled, true);
  registry->RegisterListPref(kReverseAutologinRejectedEmailList);

  // Deprecated 06/2024.
  registry->RegisterTimePref(kTrackingProtectionOnboardingNoticeFirstRequested,
                             base::Time());
  registry->RegisterTimePref(kTrackingProtectionOnboardingNoticeLastRequested,
                             base::Time());

// Deprecated 06/2024.
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(
      kAccessibilityReadAnythingOmniboxIconLabelShownCount, 0);

  // Deprecated 06/2024.
  registry->RegisterBooleanPref(kAccessibilityPdfOcrAlwaysActive, true);
#endif

  // Deprecated 06/2024.
  registry->RegisterBooleanPref(kTrackingProtectionOffboarded, false);
  registry->RegisterTimePref(kTrackingProtectionOffboardedSince, base::Time());
  registry->RegisterIntegerPref(kTrackingProtectionOffboardingAckAction, 0);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 06/2024.
  for (const char* pref : kHoldingSpaceWallpaperNudgeTimesOfFirstInteraction) {
    registry->RegisterTimePref(pref, base::Time());
  }

  // Deprecated 06/2024.
  registry->RegisterBooleanPref(kHoldingSpaceWallpaperNudgeUserEligibleForNudge,
                                false);
  registry->RegisterTimePref(
      kHoldingSpaceWallpaperNudgeLastTimeNudgeShownCounterfactual,
      base::Time());
  registry->RegisterTimePref(kHoldingSpaceWallpaperNudgeLastTimeNudgeShown,
                             base::Time());
  registry->RegisterTimePref(
      kHoldingSpaceWallpaperNudgeUserFirstEligibleSessionTime, base::Time());
  registry->RegisterUint64Pref(
      kHoldingSpaceWallpaperNudgeNudgeShownCountCounterfactual, 0u);
  registry->RegisterUint64Pref(kHoldingSpaceWallpaperNudgeNudgeShownCount, 0u);

  // Deprecated 06/2024
  registry->RegisterBooleanPref(kBirchUseRecentTabs, true);
  registry->RegisterBooleanPref(kBirchUseLastActive, true);
  registry->RegisterBooleanPref(kBirchUseMostVisited, true);
  registry->RegisterBooleanPref(kBirchUseSelfShare, true);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Deprecated 06/2024.
  registry->RegisterBooleanPref(kDefaultSearchProviderChoicePending, false);

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 07/2024
  registry->RegisterListPref(kNtpRecipesDismissedTasks);
  registry->RegisterBooleanPref(kNtpModulesFreVisible, true);
  registry->RegisterIntegerPref(kNtpModulesShownCount, 0);
  registry->RegisterTimePref(kNtpModulesFirstShownTime, base::Time());
  registry->RegisterTimePref(kNtpPhotosLastDismissedTimePrefName, base::Time());
  registry->RegisterBooleanPref(kNtpPhotosOptInAcknowledgedPrefName, false);
  registry->RegisterTimePref(kNtpPhotosLastMemoryOpenTimePrefName,
                             base::Time());
  registry->RegisterTimePref(kNtpPhotosLastSoftOptedOutTimePrefName,
                             base::Time());
  registry->RegisterIntegerPref(kNtpPhotosSoftOptOutCountPrefName, 0);
  // Deprecated 08/2024
  registry->RegisterListPref(kDismissedTabsPrefName);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 07/2024.
  registry->RegisterTimePref(kAppDeduplicationServiceLastGetTimestamp,
                             base::Time());
  registry->RegisterBooleanPref(kShowTunaScreenEnabled, true);

  // Deprecated 08/2024
  registry->RegisterBooleanPref(kStandaloneWindowMigrationNudgeShown, false);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
  // All of these were deprecated 08/2024.
  registry->RegisterIntegerPref(
      kObsoleteUnenrolledFromGoogleMobileServicesAfterApiErrorCode, 0);
  registry->RegisterBooleanPref(kObsoleteRequiresMigrationAfterSyncStatusChange,
                                false);
  registry->RegisterIntegerPref(
      kObsoleteUnenrolledFromGoogleMobileServicesWithErrorListVersion, 0);
  registry->RegisterIntegerPref(kObsoleteTimesReenrolledToGoogleMobileServices,
                                0);
  registry->RegisterIntegerPref(
      kObsoleteTimesAttemptedToReenrollToGoogleMobileServices, 0);
  registry->RegisterBooleanPref(kObsoleteUserReceivedGMSCoreError, false);
#endif
  // Deprecated 08/2024.
  registry->RegisterBooleanPref(kSafeBrowsingEsbOptInWithFriendlierSettings,
                                false);

#if BUILDFLAG(IS_ANDROID)
  // Deprecated 08/2024
  registry->RegisterListPref(kBackoffEntryKey);
  registry->RegisterTimePref(kFirstScheduleTimeKey, base::Time());
#endif

  // Deprecated 09/2024.
  registry->RegisterIntegerPref(kContentSettingsWindowLastTabIndex, 0);
  registry->RegisterStringPref(kSyncPasswordHash, std::string());
  registry->RegisterStringPref(kSyncPasswordLengthAndHashSalt, std::string());

// Deprecated 09/2024.
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(kPasswordGenerationNudgePasswordDismissCount,
                                0);
#endif  // !BUILDFLAG(IS_ANDROID)

// Deprecated 09/2024.
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterFilePathPref(kTranslateKitRootDir, base::FilePath());
#endif

// Deprecated 09/2024
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterListPref(kPrivacySandboxActivityTypeRecord);
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 09/2024
#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterListPref(kTabResumeDismissedTabsPrefName,
                             base::Value::List());
#endif  // !BUILDFLAG(IS_ANDROID)

  // Deprecated 10/2024.
  registry->RegisterIntegerPref(kModelExecutionMainToggleSettingState, 0);

// Deprecated 10/2024
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(kLacrosSecondaryProfilesAllowed, true);
  registry->RegisterIntegerPref(kAccessibilityFaceGazeCursorSmoothing, 7);
  registry->RegisterIntegerPref(kWallpaperSeaPenMigrationStatus, 0);
#endif

  // Deprecated 10/2024
  registry->RegisterBooleanPref(kLiveCaptionBubblePinned, false);

  // Deprecated 10/2024
  registry->RegisterBooleanPref(kDocumentSuggestEnabled, true);

  // Deprecated 10/2024
  registry->RegisterIntegerPref(kFirstTimeInterstitialBannerState, 0);

  // Deprecated 10/2024
  registry->RegisterBooleanPref(kSidePanelCompanionEntryPinnedToToolbar, false);
  registry->RegisterIntegerPref(kMsbbPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kSigninPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kExpsPromoDeclinedCountPref, 0);
  registry->RegisterIntegerPref(kExpsPromoShownCountPref, 0);
  registry->RegisterIntegerPref(kPcoPromoShownCountPref, 0);
  registry->RegisterIntegerPref(kPcoPromoDeclinedCountPref, 0);
  registry->RegisterBooleanPref(kExpsOptInStatusGrantedPref, false);
  registry->RegisterBooleanPref(kHasNavigatedToExpsSuccessPage, false);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 11/2024
  registry->RegisterBooleanPref(kNoteTakingAppEnabledOnLockScreen, false);
  registry->RegisterListPref(kNoteTakingAppsLockScreenAllowlist,
                             base::Value::List());
  registry->RegisterDictionaryPref(kNoteTakingAppsLockScreenToastShown);
  registry->RegisterBooleanPref(kRestoreLastLockScreenNote, false);
  registry->RegisterDictionaryPref(kSyncableVersionedWallpaperInfo);
  registry->RegisterDictionaryPref(kLacrosProxyControllingExtension);
#endif

  // Deprecated 11/2024
  registry->RegisterStringPref(kPrefixedVideoFullscreenApiAvailability, "");

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 11/2024
  registry->RegisterBooleanPref(kCartModuleHidden, false);
  registry->RegisterIntegerPref(kCartModuleWelcomeSurfaceShownTimes, 0);
  registry->RegisterBooleanPref(kCartDiscountAcknowledged, false);
  registry->RegisterBooleanPref(kCartDiscountEnabled, false);
  registry->RegisterDictionaryPref(kCartUsedDiscounts);
  registry->RegisterTimePref(kCartDiscountLastFetchedTime, base::Time());
  registry->RegisterBooleanPref(kCartDiscountConsentShown, false);
  registry->RegisterTimePref(kDiscountConsentLastDimissedTime, base::Time());
  registry->RegisterIntegerPref(kDiscountConsentPastDismissedCount, 0);
  registry->RegisterIntegerPref(kDiscountConsentDecisionMadeIn, 0);
  registry->RegisterIntegerPref(kDiscountConsentDismissedIn, 0);
  registry->RegisterIntegerPref(kDiscountConsentLastShownInVariation, 0);
  registry->RegisterBooleanPref(kDiscountConsentShowInterest, false);
  registry->RegisterIntegerPref(kDiscountConsentShowInterestIn, 0);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Deprecated 11/2024
  optimization_guide::model_execution::prefs::
      RegisterLegacyUsagePrefsForMigration(registry);

  // Deprecated 11/2024
  registry->RegisterBooleanPref(kQuietNotificationPermissionPromoWasShown,
                                true);
  registry->RegisterBooleanPref(kQuietNotificationPermissionShouldShowPromo,
                                true);
  registry->RegisterIntegerPref(kQuietNotificationPermissionUiEnablingMethod,
                                0);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 11/2024
  registry->RegisterBooleanPref(kHatsPrivacyHubPostLaunchIsSelected, false);
  registry->RegisterInt64Pref(kHatsPrivacyHubPostLaunchCycleEndTs, 0);
#endif

  // Deprecated 12/2024.
  registry->RegisterIntegerPref(kDeleteTimePeriodV2, -1);
  registry->RegisterIntegerPref(kDeleteTimePeriodV2Basic, -1);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 12/2024
  registry->RegisterDoublePref(kCryptAuthDeviceSyncLastSyncTimeSeconds, 0.0);
  registry->RegisterBooleanPref(kCryptAuthDeviceSyncIsRecoveringFromFailure,
                                false);
  registry->RegisterIntegerPref(kCryptAuthDeviceSyncReason,
                                cryptauth::INVOCATION_REASON_UNKNOWN);
  registry->RegisterListPref(kCryptAuthDeviceSyncUnlockKeys);
  registry->RegisterBooleanPref(kCryptAuthEnrollmentIsRecoveringFromFailure,
                                false);
  registry->RegisterDoublePref(kCryptAuthEnrollmentLastEnrollmentTimeSeconds,
                               0.0);
  registry->RegisterIntegerPref(kCryptAuthEnrollmentReason,
                                cryptauth::INVOCATION_REASON_UNKNOWN);
  registry->RegisterStringPref(kCryptAuthEnrollmentUserPublicKey,
                               std::string());
  registry->RegisterStringPref(kCryptAuthEnrollmentUserPrivateKey,
                               std::string());
  registry->RegisterBooleanPref(kLacrosLaunchOnLogin, false);
#endif

  // Deprecated 12/2024.
  registry->RegisterBooleanPref(kPageContentCollectionEnabled, false);

  // Deprecated 01/2025.
  registry->RegisterBooleanPref(kCompactModeEnabled, false);

  // Deprecated 01/2025.
  registry->RegisterBooleanPref(kSafeBrowsingAutomaticDeepScanningIPHSeen,
                                false);
  registry->RegisterBooleanPref(kSafeBrowsingAutomaticDeepScanPerformed, false);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 01/2025.
  registry->RegisterBooleanPref(kUsedPolicyCertificates, false);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Deprecated 02/2025.
  registry->RegisterBooleanPref(kDefaultSearchProviderKeywordsUseExtendedList,
                                false);

#if BUILDFLAG(IS_ANDROID)
  // Deprecated 02/2025.
  registry->RegisterTimePref(kLocalPasswordsMigrationWarningShownTimestamp,
                             base::Time());
  registry->RegisterBooleanPref(kLocalPasswordMigrationWarningShownAtStartup,
                                false);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 02/2025.
  registry->RegisterBooleanPref(kLiveCaptionUserMicrophoneEnabled, false);
  registry->RegisterStringPref(kUserMicrophoneCaptionLanguageCode, "");
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 02/2025.
  registry->RegisterBooleanPref(kScannerFeedbackEnabled, true);
  registry->RegisterBooleanPref(kHmrFeedbackAllowed, true);
  registry->RegisterDictionaryPref(kSharedStorage);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Deprecated 03/2025.
  registry->RegisterBooleanPref(kPasswordChangeFlowNoticeAgreement, false);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 03/2025.
  registry->RegisterBooleanPref(kSunfishEnabled, true);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Deprecated 03/2025
  registry->RegisterDictionaryPref(kRecurrentSSLInterstitial);

  // Deprecated 04/2025.
  registry->RegisterIntegerPref(
      kDefaultSearchProviderChoiceScreenShuffleMilestone, 0);

  // Deprecated 04/2025.
  registry->RegisterBooleanPref(kAddedBookmarkSincePowerBookmarksLaunch, false);

  // Deprecated 04/2025.
  registry->RegisterIntegerPref(kGlicRolloutEligibility, 0);

  // Deprecated 04/2025.
  registry->RegisterListPref(kManagedAccessToGetAllScreensMediaAllowedForUrls);

#if BUILDFLAG(IS_ANDROID)
  // Deprecated 04/2025.
  registry->RegisterBooleanPref(
      kObsoleteUserAcknowledgedLocalPasswordsMigrationWarning, false);

  // Deprecated 04/2025.
  registry->RegisterIntegerPref(
      kObsoleteLocalPasswordMigrationWarningPrefsVersion, 0);
#endif

  // Deprecated 04/2025.
  registry->RegisterDictionaryPref(kSuggestionGroupVisibility);

  // Deprecated 05/2025.
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(kWipedWebAPkDataForMigration, false);
#endif  // BUILDFLAG(IS_ANDROID)

  // Deprecated 05/2025.
  registry->RegisterStringPref(kSyncCacheGuid, std::string());
  registry->RegisterStringPref(kSyncBirthday, std::string());
  registry->RegisterStringPref(kSyncBagOfChips, std::string());
  registry->RegisterTimePref(kSyncLastSyncedTime, base::Time());
  registry->RegisterTimePref(kSyncLastPollTime, base::Time());
  registry->RegisterTimeDeltaPref(kSyncPollInterval, base::TimeDelta());
}

}  // namespace

std::string GetCountry() {
  if (!g_browser_process || !g_browser_process->variations_service()) {
    // This should only happen in tests. Ideally this would be guarded by
    // CHECK_IS_TEST, but that is not set on Android, so no specific guard.
    return std::string();
  }
  return std::string(
      g_browser_process->variations_service()->GetStoredPermanentCountry());
}

void RegisterLocalState(PrefRegistrySimple* registry) {
  // Call outs to individual subsystems that register Local State (browser-wide)
  // prefs en masse. See RegisterProfilePrefs for per-profile prefs. Please
  // keep this list alphabetized.
#if BUILDFLAG(IS_ANDROID)
  accessibility::AccessibilityPrefsController::RegisterLocalStatePrefs(
      registry);
#endif
  autofill::prefs::RegisterLocalStatePrefs(registry);
  breadcrumbs::RegisterPrefs(registry);
  browser_shutdown::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterLocalStatePrefs(registry);
  chrome_labs_prefs::RegisterLocalStatePrefs(registry);
  chrome_urls::RegisterPrefs(registry);
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  ChromeSigninClient::RegisterLocalStatePrefs(registry);
  enterprise_connectors::RegisterLocalStatePrefs(registry);
  enterprise_util::RegisterLocalStatePrefs(registry);
  component_updater::RegisterPrefs(registry);
  domain_reliability::RegisterPrefs(registry);
  embedder_support::OriginTrialPrefs::RegisterPrefs(registry);
  enterprise_reporting::RegisterLocalStatePrefs(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  GpuModeManager::RegisterPrefs(registry);
  signin::IdentityManager::RegisterLocalStatePrefs(registry);
  invalidation::FCMInvalidationService::RegisterPrefs(registry);
  invalidation::InvalidatorRegistrarWithMemory::RegisterPrefs(registry);
  invalidation::PerUserTopicSubscriptionManager::RegisterPrefs(registry);
  language::GeoLanguageProvider::RegisterLocalStatePrefs(registry);
  language::UlpLanguageCodeLocator::RegisterLocalStatePrefs(registry);
  memory::EnterpriseMemoryLimitPrefObserver::RegisterPrefs(registry);
  metrics::RegisterDemographicsLocalStatePrefs(registry);
  metrics::TabStatsTracker::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  optimization_guide::prefs::RegisterLocalStatePrefs(registry);
  optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(registry);
  password_manager::PasswordManager::RegisterLocalPrefs(registry);
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::LocalTestPolicyProvider::RegisterLocalStatePrefs(registry);
  policy::ManagementService::RegisterLocalStatePrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileAttributesEntry::RegisterLocalStatePrefs(registry);
  ProfileAttributesStorage::RegisterPrefs(registry);
  ProfileNetworkContextService::RegisterLocalStatePrefs(registry);
  profiles::RegisterPrefs(registry);
#if BUILDFLAG(IS_ANDROID)
  PushMessagingServiceImpl::RegisterPrefs(registry);
#endif
  RegisterScreenshotPrefs(registry);
  safe_browsing::RegisterLocalStatePrefs(registry);
  search_engines::SearchEngineChoiceService::RegisterLocalStatePrefs(registry);
  secure_origin_allowlist::RegisterPrefs(registry);
  segmentation_platform::SegmentationPlatformService::RegisterLocalStatePrefs(
      registry);
  SerialPolicyAllowedPorts::RegisterPrefs(registry);
#if !BUILDFLAG(IS_ANDROID)
  HidPolicyAllowedDevices::RegisterLocalStatePrefs(registry);
#endif
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  signin::ActivePrimaryAccountsMetricsRecorder::RegisterLocalStatePrefs(
      registry);
  SSLConfigServiceManager::RegisterPrefs(registry);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(
      registry, subresource_filter::kSafeBrowsingRulesetConfig.filter_tag);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(
      registry,
      fingerprinting_protection_filter::kFingerprintingProtectionRulesetConfig
          .filter_tag);
  SystemNetworkContextManager::RegisterPrefs(registry);
  tpcd::experiment::RegisterLocalStatePrefs(registry);
  tpcd::metadata::RegisterLocalStatePrefs(registry);
  tracing::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);

  // Individual preferences. If you have multiple preferences that should
  // clearly be grouped together, please group them together into a helper
  // function called above. Please keep this list alphabetized.
  registry->RegisterBooleanPref(
      policy::policy_prefs::kIntensiveWakeUpThrottlingEnabled, false);
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  registry->RegisterBooleanPref(prefs::kFeatureNotificationsEnabled, true);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(policy::policy_prefs::kBackForwardCacheEnabled,
                                true);
  registry->RegisterBooleanPref(policy::policy_prefs::kReadAloudEnabled, true);
#endif  // BUILDFLAG(IS_ANDROID)

  // Below this point is for platform-specific and compile-time conditional
  // calls. Please follow the helper-function-first-then-direct-calls pattern
  // established above, and keep things alphabetized.

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  BackgroundModeManager::RegisterPrefs(registry);
#endif

#if BUILDFLAG(IS_ANDROID)
  ::android::RegisterPrefs(registry);

  registry->RegisterIntegerPref(first_run::kTosDialogBehavior, 0);
  registry->RegisterBooleanPref(lens::kLensCameraAssistedSearchEnabled, true);
#else  // BUILDFLAG(IS_ANDROID)
  gcm::RegisterPrefs(registry);
  headless::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  media_router::RegisterLocalStatePrefs(registry);
  performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(registry);
  PerformanceInterventionMetricsReporter::RegisterLocalStatePrefs(registry);
  RegisterBrowserPrefs(registry);
  speech::SodaInstaller::RegisterLocalStatePrefs(registry);
  StartupBrowserCreator::RegisterLocalStatePrefs(registry);
  task_manager::TaskManagerInterface::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
  registry->RegisterIntegerPref(prefs::kLastWhatsNewVersion, 0);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  on_device_translation::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  WhatsNewUI::RegisterLocalStatePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  FirstRunService::RegisterLocalStatePrefs(registry);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  arc::prefs::RegisterLocalStatePrefs(registry);
  ChromeOSMetricsProvider::RegisterPrefs(registry);
  ash::AudioDevicesPrefHandlerImpl::RegisterPrefs(registry);
  ash::carrier_lock::CarrierLockManager::RegisterLocalPrefs(registry);
  ash::cert_provisioning::RegisterLocalStatePrefs(registry);
  ash::CellularESimProfileHandlerImpl::RegisterLocalStatePrefs(registry);
  ash::ManagedCellularPrefHandler::RegisterLocalStatePrefs(registry);
  ash::ChromeSessionManager::RegisterPrefs(registry);
  user_manager::UserManagerImpl::RegisterPrefs(registry);
  ash::CupsPrintersManager::RegisterLocalStatePrefs(registry);
  ash::bluetooth_config::BluetoothPowerControllerImpl::RegisterLocalStatePrefs(
      registry);
  ash::bluetooth_config::DeviceNameManagerImpl::RegisterLocalStatePrefs(
      registry);
  ash::demo_mode::RegisterLocalStatePrefs(registry);
  ash::DeviceNameStore::RegisterLocalStatePrefs(registry);
  ash::DozeModePowerStatusScheduler::RegisterLocalStatePrefs(registry);
  chromeos::DeviceOAuth2TokenStoreChromeOS::RegisterPrefs(registry);
  ash::device_settings_cache::RegisterPrefs(registry);
  ash::EnableAdbSideloadingScreen::RegisterPrefs(registry);
  ash::report::ReportController::RegisterPrefs(registry);
  ash::EnableDebuggingScreenHandler::RegisterPrefs(registry);
  ash::FastTransitionObserver::RegisterPrefs(registry);
  ash::HWDataUsageController::RegisterLocalStatePrefs(registry);
  ash::KerberosCredentialsManager::RegisterLocalStatePrefs(registry);
  ash::KioskController::RegisterLocalStatePrefs(registry);
  ash::language_prefs::RegisterPrefs(registry);
  ash::local_search_service::SearchMetricsReporter::RegisterLocalStatePrefs(
      registry);
  ash::login::SecurityTokenSessionController::RegisterLocalStatePrefs(registry);
  ash::reporting::LoginLogoutReporter::RegisterPrefs(registry);
  ash::reporting::RegisterLocalStatePrefs(registry);
  ash::NetworkMetadataStore::RegisterPrefs(registry);
  ash::NetworkThrottlingObserver::RegisterPrefs(registry);
  ash::PowerMetricsReporter::RegisterLocalStatePrefs(registry);
  ash::platform_keys::KeyPermissionsManagerImpl::RegisterLocalStatePrefs(
      registry);
  ash::power::auto_screen_brightness::MetricsReporter::RegisterLocalStatePrefs(
      registry);
  ash::Preferences::RegisterPrefs(registry);
  ash::ResetScreen::RegisterPrefs(registry);
  ash::SchedulerConfigurationManager::RegisterLocalStatePrefs(registry);
  ash::SecureDnsManager::RegisterLocalStatePrefs(registry);
  ash::ServicesCustomizationDocument::RegisterPrefs(registry);
  ash::StartupUtils::RegisterPrefs(registry);
  ash::StatsReportingController::RegisterLocalStatePrefs(registry);
  ash::system::AutomaticRebootManager::RegisterPrefs(registry);
  ash::TimeZoneResolver::RegisterPrefs(registry);
  ash::UserImageManagerImpl::RegisterPrefs(registry);
  ash::UserSessionManager::RegisterPrefs(registry);
  component_updater::MetadataTable::RegisterPrefs(registry);
  ash::CryptAuthDeviceIdProviderImpl::RegisterLocalPrefs(registry);
  extensions::ExtensionAssetsManagerChromeOS::RegisterPrefs(registry);
  extensions::ExtensionsPermissionsTracker::RegisterLocalStatePrefs(registry);
  extensions::login_api::RegisterLocalStatePrefs(registry);
  ::onc::RegisterPrefs(registry);
  policy::AdbSideloadingAllowanceModePolicyHandler::RegisterPrefs(registry);
  policy::BrowserPolicyConnectorAsh::RegisterPrefs(registry);
  policy::CrdAdminSessionController::RegisterLocalStatePrefs(registry);
  policy::DeviceCloudPolicyManagerAsh::RegisterPrefs(registry);
  policy::DeviceRestrictionScheduleController::RegisterLocalStatePrefs(
      registry);
  policy::DeviceStatusCollector::RegisterPrefs(registry);
  policy::DeviceWallpaperImageExternalDataHandler::RegisterPrefs(registry);
  policy::EnrollmentRequisitionManager::RegisterPrefs(registry);
  policy::EnrollmentStateFetcher::RegisterPrefs(registry);
  policy::EuiccStatusUploader::RegisterLocalStatePrefs(registry);
  policy::MinimumVersionPolicyHandler::RegisterPrefs(registry);
  policy::TPMAutoUpdateModePolicyHandler::RegisterPrefs(registry);
  quirks::QuirksManager::RegisterPrefs(registry);
  UpgradeDetectorChromeos::RegisterPrefs(registry);
  RegisterNearbySharingLocalPrefs(registry);
  chromeos::echo_offer::RegisterPrefs(registry);
  memory::OOMKillsMonitor::RegisterPrefs(registry);
  policy::RegisterDisabledSystemFeaturesPrefs(registry);
  policy::DlpRulesManagerImpl::RegisterPrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
  confirm_quit::RegisterLocalState(registry);
  QuitWithAppsController::RegisterPrefs(registry);
  system_media_permissions::RegisterSystemMediaPermissionStatesPrefs(registry);
  AppShimRegistry::Get()->RegisterLocalPrefs(registry);

  // The default value is not signicant as this preference is only consulted if
  // it is explicitly set by an enterprise policy.
  registry->RegisterBooleanPref(prefs::kWebAppsUseAdHocCodeSigningForAppShims,
                                false);
#endif

#if BUILDFLAG(IS_WIN)
  OSCrypt::RegisterLocalPrefs(registry);
  registry->RegisterBooleanPref(prefs::kRendererCodeIntegrityEnabled, true);
  registry->RegisterBooleanPref(prefs::kRendererAppContainerEnabled, true);
  registry->RegisterBooleanPref(prefs::kBlockBrowserLegacyExtensionPoints,
                                true);
  registry->RegisterIntegerPref(prefs::kDynamicCodeSettings, /*Default=*/0);
  registry->RegisterBooleanPref(prefs::kApplicationBoundEncryptionEnabled,
                                true);
  registry->RegisterBooleanPref(prefs::kPrintingLPACSandboxEnabled, true);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kNativeWindowOcclusionEnabled, true);
  MediaFoundationServiceMonitor::RegisterPrefs(registry);
  os_crypt_async::AppBoundEncryptionProviderWin::RegisterLocalPrefs(registry);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  IncompatibleApplicationsUpdater::RegisterLocalStatePrefs(registry);
  ThirdPartyConflictsManager::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  downgrade::RegisterPrefs(registry);
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  RegisterDefaultBrowserPromptPrefs(registry);
  DeviceOAuth2TokenStoreDesktop::RegisterPrefs(registry);
#endif

#if !BUILDFLAG(IS_ANDROID)
  screen_ai::RegisterLocalStatePrefs(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  PlatformAuthPolicyObserver::RegisterPrefs(registry);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

  // Platform-specific and compile-time conditional individual preferences.
  // If you have multiple preferences that should clearly be grouped together,
  // please group them together into a helper function called above. Please
  // keep this list alphabetized.

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  registry->RegisterBooleanPref(prefs::kOopPrintDriversAllowedByPolicy, true);
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // TODO(b/328668317): Default pref should be set to true once this is
  // launched.
  registry->RegisterBooleanPref(prefs::kOsUpdateHandlerEnabled, false);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(ENABLE_PDF)
  registry->RegisterBooleanPref(prefs::kPdfViewerOutOfProcessIframeEnabled,
                                true);
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kChromeForTestingAllowed, true);
#endif

#if BUILDFLAG(IS_WIN)
  registry->RegisterBooleanPref(prefs::kUiAutomationProviderEnabled, false);
#endif

  registry->RegisterBooleanPref(prefs::kQRCodeGeneratorEnabled, true);

  registry->RegisterIntegerPref(prefs::kChromeDataRegionSetting, 0);

#if BUILDFLAG(ENABLE_GLIC)
  glic::prefs::RegisterLocalStatePrefs(registry);
#endif

  registry->RegisterIntegerPref(prefs::kToastAlertLevel, 0);

  // This is intentionally last.
  RegisterLocalStatePrefsForMigration(registry);
}

// Register prefs applicable to all profiles.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                          const std::string& locale) {
  TRACE_EVENT0("browser", "chrome::RegisterProfilePrefs");
  // User prefs. Please keep this list alphabetized.
  AccessibilityLabelsService::RegisterProfilePrefs(registry);
  AccessibilityUIMessageHandler::RegisterProfilePrefs(registry);
  AnnouncementNotificationService::RegisterProfilePrefs(registry);
  autofill::prefs::RegisterProfilePrefs(registry);
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);
  capture_policy::RegisterProfilePrefs(registry);
  certificate_transparency::prefs::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterProfilePrefs(registry);
  chrome_labs_prefs::RegisterProfilePrefs(registry);
  ChromeLocationBarModelDelegate::RegisterProfilePrefs(registry);
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
  ChromeVersionService::RegisterProfilePrefs(registry);
  chrome_browser_net::NetErrorTabHelper::RegisterProfilePrefs(registry);
  chrome_prefs::RegisterProfilePrefs(registry);
  collaboration::prefs::RegisterProfilePrefs(registry);
  commerce::RegisterPrefs(registry);
  enterprise::RegisterIdentifiersProfilePrefs(registry);
  enterprise_connectors::RegisterProfilePrefs(registry);
  enterprise_reporting::RegisterProfilePrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  DownloadPrefs::RegisterProfilePrefs(registry);
  fingerprinting_protection_filter::prefs::RegisterProfilePrefs(registry);
#if BUILDFLAG(ENABLE_GLIC)
  glic::prefs::RegisterProfilePrefs(registry);
#endif
  permissions::PermissionHatsTriggerHelper::RegisterProfilePrefs(registry);
  history_clusters::prefs::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  image_fetcher::ImageCache::RegisterProfilePrefs(registry);
  site_engagement::ImportantSitesUtil::RegisterProfilePrefs(registry);
  IncognitoModePrefs::RegisterProfilePrefs(registry);
  invalidation::PerUserTopicSubscriptionManager::RegisterProfilePrefs(registry);
  invalidation::InvalidatorRegistrarWithMemory::RegisterProfilePrefs(registry);
  language::LanguagePrefs::RegisterProfilePrefs(registry);
  login_detection::prefs::RegisterProfilePrefs(registry);
  lookalikes::RegisterProfilePrefs(registry);
  media_prefs::RegisterUserPrefs(registry);
  MediaCaptureDevicesDispatcher::RegisterProfilePrefs(registry);
  media_device_salt::MediaDeviceIDSalt::RegisterProfilePrefs(registry);
  MediaEngagementService::RegisterProfilePrefs(registry);
  MediaStorageIdSalt::RegisterProfilePrefs(registry);
  page_info::MerchantTrustService::RegisterProfilePrefs(registry);
  metrics::RegisterDemographicsProfilePrefs(registry);
  NotificationDisplayServiceImpl::RegisterProfilePrefs(registry);
  NotifierStateTracker::RegisterProfilePrefs(registry);
  ntp_tiles::CustomLinksManagerImpl::RegisterProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
  optimization_guide::prefs::RegisterProfilePrefs(registry);
  optimization_guide::model_execution::prefs::RegisterProfilePrefs(registry);
  PageColors::RegisterProfilePrefs(registry);
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  performance_manager::user_tuning::prefs::RegisterProfilePrefs(registry);
  permissions::RegisterProfilePrefs(registry);
  PermissionBubbleMediaAccessHandler::RegisterProfilePrefs(registry);
  PlatformNotificationServiceImpl::RegisterProfilePrefs(registry);
  plus_addresses::prefs::RegisterProfilePrefs(registry);
  policy::DeveloperToolsPolicyHandler::RegisterProfilePrefs(registry);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
  PolicyUI::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  prefetch::RegisterPredictionOptionsProfilePrefs(registry);
  PrefetchOriginDecider::RegisterPrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry, locale);
  privacy_sandbox::RegisterProfilePrefs(registry);
  privacy_sandbox::PrivacySandboxNoticeStorage::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  ProfileNetworkContextService::RegisterProfilePrefs(registry);
  custom_handlers::ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  PushMessagingAppIdentifier::RegisterProfilePrefs(registry);
  PushMessagingUnsubscribedEntry::RegisterProfilePrefs(registry);
  QuietNotificationPermissionUiState::RegisterProfilePrefs(registry);
  regional_capabilities::prefs::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  RegisterGeminiSettingsPrefs(registry);
  RegisterPrefersDefaultScrollbarStylesPrefs(registry);
  RegisterSafetyHubProfilePrefs(registry);
#if BUILDFLAG(IS_CHROMEOS)
  settings::ResetSettingsHandler::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::file_type::RegisterProfilePrefs(registry);
#endif
  safe_browsing::RegisterProfilePrefs(registry);
  SearchPrefetchService::RegisterProfilePrefs(registry);
  blocked_content::SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(
      registry);
  security_interstitials::InsecureFormBlockingPage::RegisterProfilePrefs(
      registry);
  segmentation_platform::SegmentationPlatformService::RegisterProfilePrefs(
      registry);
  segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
      registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  SharingSyncPreference::RegisterProfilePrefs(registry);
  SigninPrefs::RegisterProfilePrefs(registry);
  site_engagement::SiteEngagementService::RegisterProfilePrefs(registry);
  supervised_user::RegisterProfilePrefs(registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(registry);
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  syncer::SyncTransportDataPrefs::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  tab_groups::prefs::RegisterProfilePrefs(registry);
  tpcd::experiment::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  visited_url_ranking::GroupSuggestionsServiceImpl::RegisterProfilePrefs(
      registry);
  omnibox::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  RegisterSessionServiceLogProfilePrefs(registry);
  SessionDataService::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ActivityLog::RegisterProfilePrefs(registry);
  extensions::PermissionsManager::RegisterProfilePrefs(registry);
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);
  extensions::RuntimeAPI::RegisterPrefs(registry);
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::util::RegisterProfilePrefs(registry);
  extensions_ui_prefs::RegisterProfilePrefs(registry);
  ExtensionWebUI::RegisterProfilePrefs(registry);
  update_client::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  RegisterAnimationPolicyPrefs(registry);
  extensions::AudioAPI::RegisterUserPrefs(registry);
  // TODO(devlin): This would be more inline with the other calls here if it
  // were nested in either a class or separate namespace with a simple
  // Register[Profile]Prefs() name.
  extensions::RegisterSettingsOverriddenUiPrefs(registry);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
  registry->RegisterListPref(prefs::kPdfLocalFileAccessAllowedForDomains,
                             base::Value::List());
  registry->RegisterBooleanPref(prefs::kPdfUseSkiaRendererEnabled, true);
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PolicySettings::RegisterProfilePrefs(registry);
  printing::PrintPreviewStickySettings::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerDelegate::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(IS_ANDROID)
  feed::prefs::RegisterFeedSharedProfilePrefs(registry);
  feed::RegisterProfilePrefs(registry);
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
  KnownInterceptionDisclosureInfoBarDelegate::RegisterProfilePrefs(registry);
  MediaDrmOriginIdManager::RegisterProfilePrefs(registry);
  NotificationChannelsProviderAndroid::RegisterProfilePrefs(registry);
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  OomInterventionDecider::RegisterProfilePrefs(registry);
  PartnerBookmarksShim::RegisterProfilePrefs(registry);
  permissions::GeolocationPermissionContextAndroid::RegisterProfilePrefs(
      registry);
  readaloud::RegisterProfilePrefs(registry);
  RecentTabsPagePrefs::RegisterProfilePrefs(registry);
  usage_stats::UsageStatsBridge::RegisterProfilePrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  webapps::InstallPromptPrefs::RegisterProfilePrefs(registry);
#else  // BUILDFLAG(IS_ANDROID)
  bookmarks_webui::RegisterProfilePrefs(registry);
  browser_sync::ForeignSessionHandler::RegisterProfilePrefs(registry);
  BrowserUserEducationStorageService::RegisterProfilePrefs(registry);
  captions::LiveCaptionController::RegisterProfilePrefs(registry);
  captions::LiveTranslateController::RegisterProfilePrefs(registry);
  ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(registry);
  commerce::CommerceUiTabHelper::RegisterProfilePrefs(registry);
  DeviceServiceImpl::RegisterProfilePrefs(registry);
  DriveService::RegisterProfilePrefs(registry);
  extensions::TabsCaptureVisibleTabFunction::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  gcm::RegisterProfilePrefs(registry);
  GoogleCalendarPageHandler::RegisterProfilePrefs(registry);
  HatsServiceDesktop::RegisterProfilePrefs(registry);
  lens::prefs::RegisterProfilePrefs(registry);
  NtpCustomBackgroundService::RegisterProfilePrefs(registry);
  media_router::RegisterAccessCodeProfilePrefs(registry);
  media_router::RegisterProfilePrefs(registry);
  MicrosoftAuthPageHandler::RegisterProfilePrefs(registry);
  MicrosoftFilesPageHandler::RegisterProfilePrefs(registry);
  NewTabFooterUI::RegisterProfilePrefs(registry);
  NewTabPageHandler::RegisterProfilePrefs(registry);
  NewTabPageUI::RegisterProfilePrefs(registry);
  ntp::SafeBrowsingHandler::RegisterProfilePrefs(registry);
  OutlookCalendarPageHandler::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  promos_utils::RegisterProfilePrefs(registry);
  PromoService::RegisterProfilePrefs(registry);
  RegisterReadAnythingProfilePrefs(registry);
  settings::SettingsUI::RegisterProfilePrefs(registry);
  send_tab_to_self::RegisterProfilePrefs(registry);
  signin::RegisterProfilePrefs(registry);
  StartupBrowserCreator::RegisterProfilePrefs(registry);
  MostRelevantTabResumptionPageHandler::RegisterProfilePrefs(registry);
  tab_groups::saved_tab_groups::prefs::RegisterProfilePrefs(registry);
  tab_organization_prefs::RegisterProfilePrefs(registry);
  tab_search_prefs::RegisterProfilePrefs(registry);
  ThemeColorPickerHandler::RegisterProfilePrefs(registry);
  toolbar::RegisterProfilePrefs(registry);
  UnifiedAutoplayConfig::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
  DevToolsWindow::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)

#if BUILDFLAG(IS_CHROMEOS)
  extensions::DocumentScanRegisterProfilePrefs(registry);
  extensions::login_api::RegisterProfilePrefs(registry);
  extensions::platform_keys::EnterprisePlatformKeysRegisterProfilePrefs(
      registry);
  certificate_manager::RegisterProfilePrefs(registry);
  chromeos::cloud_storage::RegisterProfilePrefs(registry);
  chromeos::cloud_upload::RegisterProfilePrefs(registry);
  policy::NetworkAnnotationBlocklistHandler::RegisterPrefs(registry);
  quickoffice::RegisterProfilePrefs(registry);
  registry->RegisterBooleanPref(prefs::kDeskAPIThirdPartyAccessEnabled, false);
  registry->RegisterBooleanPref(prefs::kDeskAPIDeskSaveAndShareEnabled, false);
  registry->RegisterListPref(prefs::kDeskAPIThirdPartyAllowlist);
  registry->RegisterBooleanPref(prefs::kInsightsExtensionEnabled, false);
  registry->RegisterBooleanPref(prefs::kEssentialSearchEnabled, false);
  registry->RegisterBooleanPref(prefs::kLastEssentialSearchValue, false);
  // By default showing Sync Consent is set to true. It can changed by policy.
  registry->RegisterBooleanPref(prefs::kEnableSyncConsent, true);
  registry->RegisterListPref(
      chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList,
      PrefRegistry::PUBLIC);
  registry->RegisterBooleanPref(policy::policy_prefs::kFloatingWorkspaceEnabled,
                                false);
  ::reporting::RegisterProfilePrefs(registry);
  registry->RegisterBooleanPref(prefs::kFloatingSsoEnabled, false);
  registry->RegisterListPref(prefs::kFloatingSsoDomainBlocklist);
  registry->RegisterListPref(prefs::kFloatingSsoDomainBlocklistExceptions);
#if BUILDFLAG(USE_CUPS)
  extensions::PrintingAPIHandler::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(USE_CUPS)
  app_list::AppListSyncableService::RegisterProfilePrefs(registry);
  apps::AlmanacFetcher::RegisterProfilePrefs(registry);
  apps::AppPlatformMetricsService::RegisterProfilePrefs(registry);
  apps::AppPreloadService::RegisterProfilePrefs(registry);
  apps::webapk_prefs::RegisterProfilePrefs(registry);
  arc::prefs::RegisterProfilePrefs(registry);
  ArcAppListPrefs::RegisterProfilePrefs(registry);
  arc::ArcBootPhaseMonitorBridge::RegisterProfilePrefs(registry);
  ash::AccountAppsAvailability::RegisterPrefs(registry);
  account_manager::AccountManager::RegisterPrefs(registry);
  ash::ApkWebAppService::RegisterProfilePrefs(registry);
  ash::app_time::AppActivityRegistry::RegisterProfilePrefs(registry);
  ash::app_time::AppTimeController::RegisterProfilePrefs(registry);
  ash::assistant::prefs::RegisterProfilePrefs(registry);
  ash::auth::AuthFactorConfig::RegisterPrefs(registry);
  ash::bluetooth::DebugLogsManager::RegisterPrefs(registry);
  ash::bluetooth_config::BluetoothPowerControllerImpl::RegisterProfilePrefs(
      registry);
  ash::HatsBluetoothRevampTriggerImpl::RegisterProfilePrefs(registry);
  user_manager::UserManagerImpl::RegisterProfilePrefs(registry);
  ash::ClientAppMetadataProviderService::RegisterProfilePrefs(registry);
  ash::CupsPrintersManager::RegisterProfilePrefs(registry);
  ash::device_sync::RegisterProfilePrefs(registry);
  ash::FamilyUserChromeActivityMetrics::RegisterProfilePrefs(registry);
  ash::FamilyUserMetricsService::RegisterProfilePrefs(registry);
  ash::FamilyUserSessionMetrics::RegisterProfilePrefs(registry);
  ash::InlineLoginHandlerImpl::RegisterProfilePrefs(registry);
  ash::first_run::RegisterProfilePrefs(registry);
  ash::file_system_provider::RegisterProfilePrefs(registry);
  ash::full_restore::RegisterProfilePrefs(registry);
  ash::KerberosCredentialsManager::RegisterProfilePrefs(registry);
  ash::multidevice_setup::MultiDeviceSetupService::RegisterProfilePrefs(
      registry);
  ash::NetworkMetadataStore::RegisterPrefs(registry);
  ash::ReleaseNotesStorage::RegisterProfilePrefs(registry);
  ash::HelpAppNotificationController::RegisterProfilePrefs(registry);
  ash::quick_unlock::FingerprintStorage::RegisterProfilePrefs(registry);
  ash::quick_unlock::PinStoragePrefs::RegisterProfilePrefs(registry);
  ash::Preferences::RegisterProfilePrefs(registry);
  ash::EnterprisePrintersProvider::RegisterProfilePrefs(registry);
  ash::parent_access::ParentAccessService::RegisterProfilePrefs(registry);
  quick_answers::prefs::RegisterProfilePrefs(registry);
  ash::quick_unlock::RegisterProfilePrefs(registry);
  ash::RegisterSamlProfilePrefs(registry);
  ash::ScreenTimeController::RegisterProfilePrefs(registry);
  ash::EduCoexistenceConsentInvalidationController::RegisterProfilePrefs(
      registry);
  ash::EduCoexistenceLoginHandler::RegisterProfilePrefs(registry);
  ash::SigninErrorNotifier::RegisterPrefs(registry);
  ash::ServicesCustomizationDocument::RegisterProfilePrefs(registry);
  ash::SecureDnsManager::RegisterProfilePrefs(registry);
  ash::settings::OSSettingsUI::RegisterProfilePrefs(registry);
  ash::StartupUtils::RegisterOobeProfilePrefs(registry);
  ash::user_image::prefs::RegisterProfilePrefs(registry);
  ash::UserImageSyncObserver::RegisterProfilePrefs(registry);
  ChromeMetricsServiceClient::RegisterProfilePrefs(registry);
  crostini::prefs::RegisterProfilePrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterProfilePrefs(registry);
  guest_os::prefs::RegisterProfilePrefs(registry);
  plugin_vm::prefs::RegisterProfilePrefs(registry);
  policy::ArcAppInstallEventLogger::RegisterProfilePrefs(registry);
  policy::AppInstallEventLogManagerWrapper::RegisterProfilePrefs(registry);
  policy::StatusCollector::RegisterProfilePrefs(registry);
  ash::SystemProxyManager::RegisterProfilePrefs(registry);
  ChromeShelfPrefs::RegisterProfilePrefs(registry);
  ::onc::RegisterProfilePrefs(registry);
  ash::cert_provisioning::RegisterProfilePrefs(registry);
  borealis::prefs::RegisterProfilePrefs(registry);
  ash::ChromeScanningAppDelegate::RegisterProfilePrefs(registry);
  ProjectorAppClientImpl::RegisterProfilePrefs(registry);
  ash::floating_workspace_util::RegisterProfilePrefs(registry);
  policy::RebootNotificationsScheduler::RegisterProfilePrefs(registry);
  ash::KioskController::RegisterProfilePrefs(registry);
  file_manager::file_tasks::RegisterProfilePrefs(registry);
  file_manager::prefs::RegisterProfilePrefs(registry);
  bruschetta::prefs::RegisterProfilePrefs(registry);
  wallpaper_handlers::prefs::RegisterProfilePrefs(registry);
  ash::reporting::RegisterProfilePrefs(registry);
  ChromeMediaAppGuestUIDelegate::RegisterProfilePrefs(registry);
  ash::boca::RegisterOnTaskProfilePrefs(registry);
  ash::boca::BabelOrcaManager::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
  CdmPrefServiceHelper::RegisterProfilePrefs(registry);
  FontPrewarmerTabHelper::RegisterProfilePrefs(registry);
  NetworkProfileBubble::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  device_signals::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  browser_switcher::BrowserSwitcherPrefs::RegisterProfilePrefs(registry);
  enterprise_signin::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && !BUILDFLAG(IS_CHROMEOS)
  preinstalled_apps::RegisterProfilePrefs(registry);
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  sharing_hub::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  accessibility_prefs::RegisterInvertBubbleUserPrefs(registry);
  side_search_prefs::RegisterProfilePrefs(registry);
  RegisterBrowserViewProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP)
  registry->RegisterBooleanPref(
      prefs::kLensRegionSearchEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kLensDesktopNTPSearchEnabled, true);
#endif

  registry->RegisterListPref(
      webauthn::pref_names::kRemoteDesktopAllowedOrigins);

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      webauthn::pref_names::kRemoteProxiedRequestsAllowed, false);

  registry->RegisterStringPref(
      webauthn::pref_names::kLastUsedPairingFromSyncPublicKey, "");

  registry->RegisterIntegerPref(
      webauthn::pref_names::kEnclaveFailedPINAttemptsCount, 0);

  side_panel_prefs::RegisterProfilePrefs(registry);

  tabs::RegisterProfilePrefs(registry);

  CertificateManagerPageHandler::RegisterProfilePrefs(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

  registry->RegisterBooleanPref(webauthn::pref_names::kAllowWithBrokenCerts,
                                false);

  registry->RegisterBooleanPref(prefs::kPrivacyGuideViewed, false);

  RegisterProfilePrefsForMigration(registry);

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(prefs::kMemorySaverChipExpandedCount, 0);
  registry->RegisterTimePref(prefs::kLastMemorySaverChipExpandedTimestamp,
                             base::Time());
  registry->RegisterBooleanPref(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // TODO(crbug.com/400455013): Add LNA support on Android
  registry->RegisterBooleanPref(
      prefs::kManagedLocalNetworkAccessRestrictionsEnabled, false);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kVirtualKeyboardResizesLayoutByDefault,
                                false);
#endif

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  data_controls::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

#if BUILDFLAG(IS_WIN)
  registry->RegisterBooleanPref(prefs::kNativeHostsExecutablesLaunchDirectly,
                                false);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_COMPOSE)
  registry->RegisterBooleanPref(prefs::kPrefHasCompletedComposeFRE, false);
  registry->RegisterBooleanPref(prefs::kEnableProactiveNudge, true);
  registry->RegisterDictionaryPref(prefs::kProactiveNudgeDisabledSitesWithTime);
#endif

  registry->RegisterIntegerPref(prefs::kChromeDataRegionSetting, 0);

  registry->RegisterIntegerPref(prefs::kLensOverlayStartCount, 0);

  registry->RegisterDictionaryPref(prefs::kReportingEndpoints);

  registry->RegisterBooleanPref(prefs::kViewSourceLineWrappingEnabled, false);
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterUserProfilePrefs(registry, g_browser_process->GetApplicationLocale());
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                              const std::string& locale) {
  RegisterProfilePrefs(registry, locale);

#if BUILDFLAG(IS_ANDROID)
  ::android::RegisterUserProfilePrefs(registry);
#endif
#if BUILDFLAG(IS_CHROMEOS)
  ash::RegisterUserProfilePrefs(registry, locale);
  ash::LegacyTokenHandleFetcher::RegisterPrefs(registry);
#endif
}

void RegisterScreenshotPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
}

void RegisterGeminiSettingsPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kGeminiSettings, 0);
}

#if BUILDFLAG(IS_CHROMEOS)
void RegisterSigninProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                                std::string_view country) {
  RegisterProfilePrefs(registry, g_browser_process->GetApplicationLocale());
  ash::RegisterSigninProfilePrefs(registry, country);
}

#endif

// This method should be periodically pruned of year+ old migrations.
// See chrome/browser/prefs/README.md for details.
void MigrateObsoleteLocalStatePrefs(PrefService* local_state) {
  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.

  // BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS
  // Please don't delete the preceding line. It is used by PRESUBMIT.py.

#if BUILDFLAG(IS_WIN)
  // Deprecated 06/2024.
  local_state->ClearPref(kOsCryptAppBoundFixedData3PrefName);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Added 05/2024.
  local_state->ClearPref(kDeviceRegisteredTime);
  local_state->ClearPref(kArcKioskDictionaryName);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  // Added 05/2024.
  local_state->ClearPref(kSearchEnginesStudyGroup);
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Added 05/2024.
  local_state->ClearPref(kHasShownRefreshWhatsNew);
#endif

// Added 06/2024.
#if BUILDFLAG(IS_CHROMEOS)
  local_state->ClearPref(kLocalUserFilesMigrationEnabled);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Added 07/2024.
  local_state->ClearPref(kFirstRunStudyGroup);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Added 08/2024.
  local_state->ClearPref(kDeviceDMTokenV1);
  local_state->ClearPref(kDeviceDMTokenV2);
  local_state->ClearPref(kDemoModeResourcesRemoved);
  local_state->ClearPref(kAccumulatedUsagePref);

  // Added 10/2024.
  local_state->ClearPref(kMigrationStep);
  local_state->ClearPref(kMoveMigrationResumeStepPref);
  local_state->ClearPref(kMoveMigrationResumeCountPref);
  local_state->ClearPref(kDataVerPref);
  local_state->ClearPref(kMigrationAttemptCountPref);
  local_state->ClearPref(kProfileMigrationCompletedForUserPref);
  local_state->ClearPref(kProfileMoveMigrationCompletedForUserPref);
  local_state->ClearPref(kProfileMigrationCompletedForNewUserPref);
  local_state->ClearPref(kProfileDataBackwardMigrationCompletedForUserPref);
  local_state->ClearPref(kGotoFilesPref);
  local_state->ClearPref(kProfileMigrationCompletionTimeForUserPref);
  local_state->ClearPref(kLacrosDataBackwardMigrationMode);
#endif

#if !BUILDFLAG(IS_ANDROID)
  // Added 10/2024
  local_state->ClearPref(kWhatsNewHatsActivationThreshold);
#endif

  // Added 10/2024.
  local_state->ClearPref(kBeforeunloadEventCancelByPreventDefaultEnabled);

  // Added 11/2024
#if BUILDFLAG(IS_CHROMEOS)
  local_state->ClearPref(kLockScreenDataPrefKey);
#endif

  // Added 11/2024
  local_state->ClearPref(kOnDeviceModelTimeoutCount);

  // Added 11/2024
  optimization_guide::model_execution::prefs::MigrateLegacyUsagePrefs(
      local_state);

  // Added 12/2024
#if BUILDFLAG(IS_CHROMEOS)
  local_state->ClearPref(kLacrosLaunchSwitch);
  local_state->ClearPref(kLacrosSelection);
#endif

  // Added 02/2025.
  local_state->ClearPref(kUserAgentClientHintsGREASEUpdateEnabled);

  // Added 02/2025
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  local_state->ClearPref(prefs::kDefaultBrowserPromptRefreshStudyGroup);
#endif

  // Added 02/2025
#if BUILDFLAG(IS_ANDROID)
  local_state->ClearPref(kRootSecretPrefName);
#endif  // BUILDFLAG(IS_ANDROID)

  // Added 03/2025.
#if BUILDFLAG(IS_CHROMEOS)
  local_state->ClearPref(kShouldRetrieveDeviceState);
  local_state->ClearPref(kShouldAutoEnroll);
  local_state->ClearPref(kAutoEnrollmentPowerLimit);
#endif

  // Added 03/2025.
#if BUILDFLAG(IS_CHROMEOS)
  local_state->ClearPref(kDeviceRestrictionScheduleHighestSeenTime);
#endif

#if !BUILDFLAG(IS_ANDROID)
  local_state->ClearPref(
      kPerformanceInterventionNotificationAcceptHistoryDeprecated);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Please don't delete the following line. It is used by PRESUBMIT.py.
  // END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS

  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.
}

// This method should be periodically pruned of year+ old migrations.
// See chrome/browser/prefs/README.md for details.
void MigrateObsoleteProfilePrefs(PrefService* profile_prefs,
                                 const base::FilePath& profile_path) {
  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.

  // BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS
  // Please don't delete the preceding line. It is used by PRESUBMIT.py.

  privacy_sandbox::PrivacySandboxNoticeStorage::UpdateNoticeSchemaV2(
      profile_prefs);

  // Check MigrateDeprecatedAutofillPrefs() to see if this is safe to remove.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(profile_prefs);

  // Added 3/2020.
  // TODO(crbug.com/40122991): Remove this once the privacy settings redesign
  // is fully launched.
  chrome_browser_net::secure_dns::MigrateProbesSettingToOrFromBackup(
      profile_prefs);

  // TODO(326079444): After experiment is over, update the deprecated date and
  // allow this to be cleaned up.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  MigrateDefaultBrowserLastDeclinedPref(profile_prefs);
#endif

#if BUILDFLAG(IS_ANDROID)
  // Added 11/2023, but DO NOT REMOVE after the usual year!
  // TODO(crbug.com/40268177): The pref kPasswordsUseUPMLocalAndSeparateStores
  // and this call (to compute said pref) should be removed once
  // kUnifiedPasswordManagerLocalPasswordsAndroidWithMigration is launched and
  // enough clients have migrated. UsesSplitStoresAndUPMForLocal() should be
  // updated to check the GmsCoreVersion directly instead of the pref, or
  // might be removed entirely, depending how the outdated GmsCore case is
  // handled.
  password_manager_android_util::SetUsesSplitStoresAndUPMForLocal(
      profile_prefs, profile_path,
      std::make_unique<
          password_manager_android_util::PasswordManagerUtilBridge>());
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Added 05/2024.
  profile_prefs->ClearPref(kAccessibilityMouseKeysShortcutToPauseEnabled);
  profile_prefs->ClearPref(kAccessibilityMouseKeysDisableInTextFields);
  profile_prefs->ClearPref(kAccessibilityScreenMagnifierCenterFocus);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Added 05/2024.
  profile_prefs->ClearPref(kBlockTruncatedCookies);

  // Added 05/2024
  profile_prefs->ClearPref(kDefaultSearchProviderChoiceLocationPrefName);

  // Added 05/2024.
  profile_prefs->ClearPref(kSyncCachedTrustedVaultAutoUpgradeDebugInfo);

  // Added 05/2024.
  profile_prefs->ClearPref(kAutologinEnabled);
  profile_prefs->ClearPref(kReverseAutologinRejectedEmailList);

  // Added 06/2024.
  profile_prefs->ClearPref(kTrackingProtectionOnboardingNoticeFirstRequested);
  profile_prefs->ClearPref(kTrackingProtectionOnboardingNoticeLastRequested);

  // Added 06/2024.
#if !BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(
      kAccessibilityReadAnythingOmniboxIconLabelShownCount);

  // Added 06/2024.
  profile_prefs->ClearPref(kAccessibilityPdfOcrAlwaysActive);
#endif

  // Added 06/2024.
  profile_prefs->ClearPref(kTrackingProtectionOffboarded);
  profile_prefs->ClearPref(kTrackingProtectionOffboardedSince);
  profile_prefs->ClearPref(kTrackingProtectionOffboardingAckAction);

#if BUILDFLAG(IS_CHROMEOS)
  // Added 06/2024.
  for (const char* pref : kHoldingSpaceWallpaperNudgeTimesOfFirstInteraction) {
    profile_prefs->ClearPref(pref);
  }

  // Added 06/2024.
  profile_prefs->ClearPref(kHoldingSpaceWallpaperNudgeUserEligibleForNudge);
  profile_prefs->ClearPref(
      kHoldingSpaceWallpaperNudgeLastTimeNudgeShownCounterfactual);
  profile_prefs->ClearPref(kHoldingSpaceWallpaperNudgeLastTimeNudgeShown);
  profile_prefs->ClearPref(
      kHoldingSpaceWallpaperNudgeUserFirstEligibleSessionTime);
  profile_prefs->ClearPref(
      kHoldingSpaceWallpaperNudgeNudgeShownCountCounterfactual);
  profile_prefs->ClearPref(kHoldingSpaceWallpaperNudgeNudgeShownCount);

  // Added 06/2024.
  profile_prefs->ClearPref(kBirchUseRecentTabs);
  profile_prefs->ClearPref(kBirchUseLastActive);
  profile_prefs->ClearPref(kBirchUseMostVisited);
  profile_prefs->ClearPref(kBirchUseSelfShare);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Added 06/2024.
  syncer::SyncPrefs::MaybeMigrateAutofillToPerAccountPref(profile_prefs);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_ANDROID)
  // Added 06/2024
  feed::prefs::MigrateObsoleteFeedExperimentPref_Jun_2024(profile_prefs);
#endif  // BUILDFLAG(IS_ANDROID)

  // Added 06/2024.
  profile_prefs->ClearPref(kDefaultSearchProviderChoicePending);

#if !BUILDFLAG(IS_ANDROID)
  // Added 07/2024.
  profile_prefs->ClearPref(kNtpRecipesDismissedTasks);
  profile_prefs->ClearPref(kNtpModulesFirstShownTime);
  profile_prefs->ClearPref(kNtpModulesFreVisible);
  profile_prefs->ClearPref(kNtpModulesShownCount);
  profile_prefs->ClearPref(kNtpPhotosLastDismissedTimePrefName);
  profile_prefs->ClearPref(kNtpPhotosOptInAcknowledgedPrefName);
  profile_prefs->ClearPref(kNtpPhotosLastMemoryOpenTimePrefName);
  profile_prefs->ClearPref(kNtpPhotosLastSoftOptedOutTimePrefName);
  profile_prefs->ClearPref(kNtpPhotosSoftOptOutCountPrefName);
  // Added 08/2024
  profile_prefs->ClearPref(kDismissedTabsPrefName);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Added 07/2024.
  profile_prefs->ClearPref(kAppDeduplicationServiceLastGetTimestamp);
  profile_prefs->ClearPref(kShowTunaScreenEnabled);

  // Added 08/2024.
  profile_prefs->ClearPref(kStandaloneWindowMigrationNudgeShown);
#endif

#if BUILDFLAG(IS_ANDROID)
  // All of these were added 08/2024.
  profile_prefs->ClearPref(
      kObsoleteUnenrolledFromGoogleMobileServicesAfterApiErrorCode);
  profile_prefs->ClearPref(kObsoleteRequiresMigrationAfterSyncStatusChange);
  profile_prefs->ClearPref(
      kObsoleteUnenrolledFromGoogleMobileServicesWithErrorListVersion);
  profile_prefs->ClearPref(kObsoleteTimesReenrolledToGoogleMobileServices);
  profile_prefs->ClearPref(
      kObsoleteTimesAttemptedToReenrollToGoogleMobileServices);
  profile_prefs->ClearPref(kObsoleteUserReceivedGMSCoreError);
#endif
  // Added 08/2024.
  profile_prefs->ClearPref(kSafeBrowsingEsbOptInWithFriendlierSettings);

#if !BUILDFLAG(IS_ANDROID)
  // Added 08/2024, but DO NOT REMOVE after the usual year.
  // TODO(crbug.com/356148174): Remove once kMoveThemePrefsToSpecifics has been
  // enabled for an year.
  MigrateSyncingThemePrefsToNonSyncingIfNeeded(profile_prefs);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Added 08/2024.
#if BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kBackoffEntryKey);
  profile_prefs->ClearPref(kFirstScheduleTimeKey);
#endif

  // Added 09/2024.
  profile_prefs->ClearPref(kContentSettingsWindowLastTabIndex);
  profile_prefs->ClearPref(kSyncPasswordHash);
  profile_prefs->ClearPref(kSyncPasswordLengthAndHashSalt);

// Added 09/2024
#if !BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kPasswordGenerationNudgePasswordDismissCount);
#endif  // !BUILDFLAG(IS_ANDROID)

// Added 09/2024.
#if !BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kTranslateKitRootDir);
#endif

// Added 09/2024
#if BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kPrivacySandboxActivityTypeRecord);
#endif  // BUILDFLAG(IS_ANDROID)

// Added 09/2024
#if !BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kTabResumeDismissedTabsPrefName);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Added 10/2024.
  profile_prefs->ClearPref(kModelExecutionMainToggleSettingState);

// Added 10/2024
#if BUILDFLAG(IS_CHROMEOS)
  profile_prefs->ClearPref(kLacrosSecondaryProfilesAllowed);
  profile_prefs->ClearPref(kAccessibilityFaceGazeCursorSmoothing);
  profile_prefs->ClearPref(kWallpaperSeaPenMigrationStatus);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Added 10/2024
  profile_prefs->ClearPref(kLiveCaptionBubblePinned);

  // Added 10/2024
  profile_prefs->ClearPref(kDocumentSuggestEnabled);

  // Added 10/2024
  profile_prefs->ClearPref(kFirstTimeInterstitialBannerState);

  // Added 10/2024
  profile_prefs->ClearPref(kSidePanelCompanionEntryPinnedToToolbar);
  profile_prefs->ClearPref(kMsbbPromoDeclinedCountPref);
  profile_prefs->ClearPref(kSigninPromoDeclinedCountPref);
  profile_prefs->ClearPref(kExpsPromoDeclinedCountPref);
  profile_prefs->ClearPref(kExpsPromoShownCountPref);
  profile_prefs->ClearPref(kPcoPromoShownCountPref);
  profile_prefs->ClearPref(kPcoPromoDeclinedCountPref);
  profile_prefs->ClearPref(kExpsOptInStatusGrantedPref);
  profile_prefs->ClearPref(kHasNavigatedToExpsSuccessPage);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 11/2024
  profile_prefs->ClearPref(kNoteTakingAppEnabledOnLockScreen);
  profile_prefs->ClearPref(kNoteTakingAppsLockScreenAllowlist);
  profile_prefs->ClearPref(kNoteTakingAppsLockScreenToastShown);
  profile_prefs->ClearPref(kRestoreLastLockScreenNote);
  profile_prefs->ClearPref(kSyncableVersionedWallpaperInfo);
  profile_prefs->ClearPref(kLacrosProxyControllingExtension);
#endif

  // Added 11/2024
  profile_prefs->ClearPref(kPrefixedVideoFullscreenApiAvailability);

// Added 11/2024
#if !BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kCartModuleHidden);
  profile_prefs->ClearPref(kCartModuleWelcomeSurfaceShownTimes);
  profile_prefs->ClearPref(kCartDiscountAcknowledged);
  profile_prefs->ClearPref(kCartDiscountEnabled);
  profile_prefs->ClearPref(kCartUsedDiscounts);
  profile_prefs->ClearPref(kCartDiscountLastFetchedTime);
  profile_prefs->ClearPref(kCartDiscountConsentShown);
  profile_prefs->ClearPref(kDiscountConsentDecisionMadeIn);
  profile_prefs->ClearPref(kDiscountConsentDismissedIn);
  profile_prefs->ClearPref(kDiscountConsentLastDimissedTime);
  profile_prefs->ClearPref(kDiscountConsentLastShownInVariation);
  profile_prefs->ClearPref(kDiscountConsentPastDismissedCount);
  profile_prefs->ClearPref(kDiscountConsentShowInterest);
  profile_prefs->ClearPref(kDiscountConsentShowInterestIn);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Added 11/2024
  profile_prefs->ClearPref(kQuietNotificationPermissionPromoWasShown);
  profile_prefs->ClearPref(kQuietNotificationPermissionShouldShowPromo);
  profile_prefs->ClearPref(kQuietNotificationPermissionUiEnablingMethod);

#if BUILDFLAG(IS_CHROMEOS)
  // Added 11/2024
  profile_prefs->ClearPref(kHatsPrivacyHubPostLaunchIsSelected);
  profile_prefs->ClearPref(kHatsPrivacyHubPostLaunchCycleEndTs);
#endif

  // Added 12/2024.
  profile_prefs->ClearPref(kDeleteTimePeriodV2);
  profile_prefs->ClearPref(kDeleteTimePeriodV2Basic);

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 12/2024
  profile_prefs->ClearPref(kCryptAuthDeviceSyncLastSyncTimeSeconds);
  profile_prefs->ClearPref(kCryptAuthDeviceSyncIsRecoveringFromFailure);
  profile_prefs->ClearPref(kCryptAuthDeviceSyncReason);
  profile_prefs->ClearPref(kCryptAuthDeviceSyncUnlockKeys);
  profile_prefs->ClearPref(kCryptAuthEnrollmentIsRecoveringFromFailure);
  profile_prefs->ClearPref(kCryptAuthEnrollmentLastEnrollmentTimeSeconds);
  profile_prefs->ClearPref(kCryptAuthEnrollmentReason);
  profile_prefs->ClearPref(kCryptAuthEnrollmentUserPublicKey);
  profile_prefs->ClearPref(kCryptAuthEnrollmentUserPrivateKey);
  profile_prefs->ClearPref(kLacrosLaunchOnLogin);
#endif

  // Added 12/2024.
  profile_prefs->ClearPref(kPageContentCollectionEnabled);

#if !BUILDFLAG(IS_ANDROID)
  // Added 01/2025.
  password_manager::features_util::MigrateDefaultProfileStorePref(
      profile_prefs);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Added 01/2025.
  profile_prefs->ClearPref(kCompactModeEnabled);

  // Added 01/2025.
  profile_prefs->ClearPref(kSafeBrowsingAutomaticDeepScanPerformed);
  profile_prefs->ClearPref(kSafeBrowsingAutomaticDeepScanningIPHSeen);

#if BUILDFLAG(IS_CHROMEOS)
  // Added 01/2025.
  profile_prefs->ClearPref(kUsedPolicyCertificates);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Added 02/2025.
  profile_prefs->ClearPref(kDefaultSearchProviderKeywordsUseExtendedList);

#if BUILDFLAG(IS_ANDROID)
  // Added 02/2025.
  profile_prefs->ClearPref(kLocalPasswordsMigrationWarningShownTimestamp);
  profile_prefs->ClearPref(kLocalPasswordMigrationWarningShownAtStartup);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  // Deprecated 02/2025.
  profile_prefs->ClearPref(kLiveCaptionUserMicrophoneEnabled);
  profile_prefs->ClearPref(kUserMicrophoneCaptionLanguageCode);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
  // Added 02/2025.
  profile_prefs->ClearPref(kScannerFeedbackEnabled);
  profile_prefs->ClearPref(kHmrFeedbackAllowed);
  profile_prefs->ClearPref(kSharedStorage);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Added 03/2025.
  profile_prefs->ClearPref(kPasswordChangeFlowNoticeAgreement);

#if !BUILDFLAG(IS_CHROMEOS)
  // Added 03/2025.
  profile_prefs->ClearPref(prefs::kChildAccountStatusKnown);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Added 03/2025.
  profile_prefs->ClearPref(kSunfishEnabled);
#endif

  // Added 03/2025.
  profile_prefs->ClearPref(kRecurrentSSLInterstitial);

  // Added 04/2025.
  profile_prefs->ClearPref(kDefaultSearchProviderChoiceScreenShuffleMilestone);

  // Added 04/2025.
  profile_prefs->ClearPref(kAddedBookmarkSincePowerBookmarksLaunch);

  // Added 04/2025.
  profile_prefs->ClearPref(kGlicRolloutEligibility);

  // Added 04/2025
  profile_prefs->ClearPref(kManagedAccessToGetAllScreensMediaAllowedForUrls);

#if BUILDFLAG(IS_ANDROID)
  // Added 04/2025
  profile_prefs->ClearPref(
      kObsoleteUserAcknowledgedLocalPasswordsMigrationWarning);

  // Added 04/2025.
  profile_prefs->ClearPref(kObsoleteLocalPasswordMigrationWarningPrefsVersion);
#endif

  // Added 04/2025.
  profile_prefs->ClearPref(kSuggestionGroupVisibility);

#if BUILDFLAG(IS_ANDROID)
  // Added 05/2025.
  profile_prefs->ClearPref(kWipedWebAPkDataForMigration);
#endif  // BUILDFLAG(IS_ANDROID)

  // Added 05/2025.
  profile_prefs->ClearPref(kSyncCacheGuid);
  profile_prefs->ClearPref(kSyncBirthday);
  profile_prefs->ClearPref(kSyncBagOfChips);
  profile_prefs->ClearPref(kSyncLastSyncedTime);
  profile_prefs->ClearPref(kSyncLastPollTime);
  profile_prefs->ClearPref(kSyncPollInterval);

  // Please don't delete the following line. It is used by PRESUBMIT.py.
  // END_MIGRATE_OBSOLETE_PROFILE_PREFS

  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.
}
