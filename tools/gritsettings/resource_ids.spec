# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This file is used to assign starting resource ids for resources and strings
# used by Chromium.  This is done to ensure that resource ids are unique
# across all the grd files.  If you are adding a new grd file, please add
# a new entry to this file.
#
# The entries below are organized into sections. When adding new entries,
# please use the right section. Try to keep entries in alphabetical order.
#
# - chrome/app/
# - chrome/browser/
# - chrome/ WebUI
# - chrome/ miscellaneous
# - chromeos/
# - components/
# - ios/ (overlaps with chrome/)
# - content/
# - everything else
#
# The range of ID values, which is used by pak files, is from 0 to 2^16 - 1.
#
# IMPORTANT: For update instructions, see README.md.
{
  # The first entry in the file, SRCDIR, is special: It is a relative path from
  # this file to the base of your checkout.
  "SRCDIR": "../..",

  # START chrome/app section.
  #
  # chrome/ and ios/chrome/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "chrome/app/chromium_strings.grd": {
    "messages": [800],
  },
  "chrome/app/google_chrome_strings.grd": {
    "messages": [800],
  },

  # Leave lots of space for generated_resources since it has most of our
  # strings.
  "chrome/app/generated_resources.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"join": 2, "align": 200},
    "messages": [1000],
  },

  "chrome/app/resources/locale_settings.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 1000},
    "messages": [2000],
  },

  # These each start with the same resource id because we only use one
  # file for each build (chromiumos, google_chromeos, linux, mac, or win).
  "chrome/app/resources/locale_settings_chromiumos.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 100},
    "messages": [2100],
  },
  "chrome/app/resources/locale_settings_google_chromeos.grd": {
    "messages": [2100],
  },
  "chrome/app/resources/locale_settings_linux.grd": {
    "messages": [2100],
  },
  "chrome/app/resources/locale_settings_mac.grd": {
    "messages": [2100],
  },
  "chrome/app/resources/locale_settings_win.grd": {
    "messages": [2100],
  },

  "chrome/app/theme/chrome_unscaled_resources.grd": {
    "META": {"join": 5},
    "includes": [2120],
  },

  # Leave space for theme_resources since it has many structures.
  "chrome/app/theme/theme_resources.grd": {
    "structures": [2140],
  },
  # END chrome/app section.

  # START chrome/browser section.
  "chrome/browser/dev_ui_browser_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [2200],
  },
  "chrome/browser/browser_resources.grd": {
    "includes": [2220],
    "structures": [2240],
  },
  "chrome/browser/nearby_sharing/internal/nearby_share_internal_strings.grd": {
    "messages": [2260],
  },
  "chrome/browser/recent_tabs/internal/android/java/strings/android_restore_tabs_strings.grd": {
    "messages": [2280],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/accessibility/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2300],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/feedback/resources.grd": {
    "META": {"sizes": {"includes": [30],}},
    "includes": [2320],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/feed_internals/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2340],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/app_service_internals/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [2360],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/bookmarks/resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2380],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/browser_switch/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2400],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/certificate_viewer/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [2420],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/add_supervision/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2440],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/arc_account_picker/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2460],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/assistant_optin/assistant_optin_resources.grd": {
    "META": {"sizes": {"includes": [80]}},
    "includes": [2480],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/cloud_upload/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [2500],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/desk_api/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2520],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/edu_coexistence/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [2540],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/gaia_action_buttons/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2560],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/emoji_picker/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [2580],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/kerberos/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [2600],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/launcher_internals/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [2620],
  },
   "chrome/browser/resources/chromeos/mako/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [2640],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/parent_access/resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [2660],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/remote_maintenance_curtain/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [2680],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/sensor_info/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [2700],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/supervision/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2720],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/predictors/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [2740],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/profile_internals/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [2760],
  },
  "chrome/browser/resources/app_icon/app_icon_resources.grd": {
    "structures": [2780],
  },
  "chrome/browser/resources/chromeos/app_icon/app_icon_resources.grd": {
    "structures": [2800],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/login/oobe_conditional_resources.grd": {
    "META": {"sizes": {"includes": [150], "structures": [300]}},
    "includes": [2820],
    "structures": [2840],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/lock_screen_reauth/resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [2860],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/login/oobe_unconditional_resources.grd": {
    "META": {"sizes": {"includes": [350]}},
    "includes": [2880],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/multidevice_internals/resources.grd": {
    "META": {"sizes": {"includes": [35]}},
    "includes": [2900],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/multidevice_setup/multidevice_setup_resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [2920],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/notification_tester/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [2940],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/password_change/resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [2960],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/vc_tray_tester/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [2980],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/web_app_install/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [3000],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/commander/resources.grd": {
    "META": {"sizes": {"includes": [15]}},
    "includes": [3020],
  },
  "chrome/browser/resources/component_extension_resources.grd": {
    "includes": [3040],
    "structures": [3060],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/access_code_cast/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [3080],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/compose/resources.grd": {
    "META": {"sizes": {"includes": [15]}},
    "includes": [3100],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/connectors_internals/resources.grd": {
    "META": {"sizes": {"includes": [15]}},
    "includes": [3120],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/discards/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [3140],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/dlp_internals/resources.grd": {
    "META": {"sizes": {"includes": [15]}},
    "includes": [3160],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/downloads/resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [3180],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/engagement/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [3200],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/extensions/resources.grd": {
    "META": {"sizes": {"includes": [90],}},
    "includes": [3220],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/history/resources.grd": {
    "META": {"sizes": {"includes": [40]}},
    "includes": [3240],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/identity_internals/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [3260],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/internals/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [3280],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/intro/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [3300],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/location_internals/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [3320],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/management/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [3340],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/memory_internals/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [3360],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/new_tab_page_instant/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [3380],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/nearby_internals/nearby_internals_resources.grd": {
    "META": {"sizes": {"includes": [40]}},
    "includes": [3400],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/nearby_share/resources.grd": {
    "META": {"sizes": {"includes": [100]}},
    "includes": [3420],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/media_router/cast_feedback/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [3440],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/media_router/internals/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [3460],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/new_tab_page/resources.grd": {
    "META": {"sizes": {"includes": [200]}},
    "includes": [3480],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/new_tab_page_third_party/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [3500],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/omnibox_popup/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [3520],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/password_manager/resources.grd": {
    "META": {"sizes": {"includes": [200]}},
    "includes": [3540],
  },
  "chrome/browser/resources/preinstalled_web_apps/resources.grd": {
    "includes": [3560],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/pdf/resources.grd": {
    "META": {"sizes": {"includes": [200]}},
    "includes": [3580],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/print_preview/resources.grd": {
    "META": {"sizes": {"includes": [500],}},
    "includes": [3600],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/privacy_sandbox/resources.grd": {
    "META": {"sizes": {"includes": [30],}},
    "includes": [3620],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/sandbox_internals/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [3640],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/segmentation_internals/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [3660],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/bookmarks/resources.grd": {
    "META": {"sizes": {"includes": [45],}},
    "includes": [3680],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/commerce/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [3700],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/companion/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [3720],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/customize_chrome/resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [3740],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/history_clusters/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [3760],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/read_anything/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [3780],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/reading_list/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [3800],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/shared/resources.grd": {
    "META": {"sizes": {"includes": [15],}},
    "includes": [3820],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/user_notes/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [3840],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/side_panel/performance_controls/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [3860],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/ash/settings/resources.grd": {
    "META": {"sizes": {"includes": [1000],}},
    "includes": [3880],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/search_engine_choice/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [3900],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/settings/resources.grd": {
    "META": {"sizes": {"includes": [500],}},
    "includes": [3920],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/settings_shared/resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [3940],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/signin/profile_picker/resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [3960],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/signin/resources.grd": {
    "META": {"sizes": {"includes": [60],}},
    "includes": [3980],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/support_tool/resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [4000],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/tab_search/resources.grd": {
    "META": {"sizes": {"includes": [60]}},
    "includes": [4020],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/tab_strip/resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [4040],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/welcome/resources.grd": {
    "META": {"sizes": {"includes": [60]}},
    "includes": [4060],
  },
  "chrome/browser/test_dummy/internal/android/resources/resources.grd": {
    "includes": [4080],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/webui_gallery/resources.grd": {
    "META": {"sizes": {"includes": [90]}},
    "includes": [4100],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/web_app_internals/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4120],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/whats_new/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4140],
  },
  # END chrome/browser section.

  # START chrome/ WebUI resources section
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/browsing_topics/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4160],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/bluetooth_internals/resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [4180],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/audio/resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [4200],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/bluetooth_pairing_dialog/bluetooth_pairing_dialog_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [4220],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/borealis_installer/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [4240],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/chromebox_for_meetings/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [4260],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/healthd_internals/resources.grd": {
    "META": {"sizes": {"includes": [50]}},
    "includes": [4280],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/internet_config_dialog/internet_config_dialog_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [4300],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/internet_detail_dialog/internet_detail_dialog_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [4320],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/network_ui/network_ui_resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4340],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/commerce/core/internals/resources/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4360],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/flags_ui/resources/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4380],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/history_clusters/history_clusters_internals/resources/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4400],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/download/resources/download_internals/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4420],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/optimization_guide/optimization_guide_internals/resources/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4440],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/policy/resources/webui/resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [4460],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/version_ui/resources/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [4480],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/about_sys/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4500],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/app_home/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4520],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/components/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [4540],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/gaia_auth_host/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [4560],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/invalidations/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [4580],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/media/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [4600],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/net_internals/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4620],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/omnibox/resources.grd": {
    "META": {"sizes": {"includes": [30]}},
    "includes": [4640],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/suggest_internals/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4660],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/quota_internals/quota_internals_resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4680],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/sync_file_system_internals/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4700],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/usb_internals/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4720],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/webapks/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4740],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/webui_js_error/resources.grd": {
   "META": {"sizes": {"includes": [10],}},
   "includes": [4760],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/app_settings/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [4780],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/sync/service/resources/resources.grd": {
   "META": {"sizes": {"includes": [30],}},
    "includes": [4800],
  },
  "components/resources/dev_ui_components_resources.grd": {
    "includes": [4820],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/aggregation_service/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4840],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/attribution_reporting/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4860],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/gpu/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4880],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/histograms/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [4900],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/indexed_db/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [4920],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/media/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [4940],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/net/resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [4960],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/process/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [4980],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/service_worker/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [5000],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/quota/resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [5020],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/traces_internals/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [5040],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/resources/webxr_internals/resources.grd": {
    "META": {"sizes": {"includes": [20,],}},
    "includes": [5060],
  },
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/webrtc/resources/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [5080],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/feed/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [5100],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/manage_mirrorsync/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [5120],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/inline_login/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [5140],
  },
  "<(SHARED_INTERMEDIATE_DIR)/components/metrics/debug/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [5160],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/lens/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [5180],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/lens/untrusted_resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [5200],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/chromeos/enterprise_reporting/resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [5220],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/hats/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [5240],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/browser/resources/on_device_internals/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [5260],
  },
  # END chrome/ WebUI resources section

  # START chrome/ miscellaneous section.
  "chrome/common/common_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "includes": [5300],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/common/chromeos/extensions/chromeos_system_extensions_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [5320],
  },
  "chrome/credential_provider/gaiacp/gaia_resources.grd": {
    "includes": [5340],
    "messages": [5360],
  },
  "chrome/renderer/resources/renderer_resources.grd": {
    "includes": [5380],
    "structures": [5400],
  },
  "<(SHARED_INTERMEDIATE_DIR)/chrome/test/data/webui/resources.grd": {
    "META": {"sizes": {"includes": [2000],}},
    "includes": [5420],
  },
  # END chrome/ miscellaneous section.

  # START chromeos/ section.
  "chromeos/chromeos_strings.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "messages": [5500],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/ambient/resources/lottie_resources.grd": {
    "META": {"sizes": {"includes": [100],}},
    "includes": [5520],
  },
  "chromeos/ash/resources/ash_resources.grd": {
    "includes": [5540],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/camera_app_ui/ash_camera_app_resources.grd": {
    "META": {"sizes": {"includes": [300],}},
    "includes": [5560],
  },
  "ash/webui/camera_app_ui/resources/strings/camera_strings.grd": {
    "messages": [5580],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/color_internals/resources/resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [5600],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/common/resources/office_fallback/resources.grd": {
    "META": {"sizes": {"includes": [5]}},
    "includes": [5620],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/connectivity_diagnostics/resources/connectivity_diagnostics_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [5640],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/diagnostics_ui/resources/ash_diagnostics_app_resources.grd": {
    "META": {"sizes": {"includes": [200],}},
    "includes": [5660],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/file_manager/resources/file_manager_swa_resources.grd": {
    "META": {"sizes": {"includes": [200]}},
    "includes": [5680],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/file_manager/untrusted_resources/file_manager_untrusted_resources.grd": {
    "META": {"sizes": {"includes": [20]}},
    "includes": [5700],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/files_internals/ash_files_internals_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [5720],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/guest_os_installer/resources/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [5740],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/common/resources/resources.grd": {
    "META": {"sizes": {"includes": [700]}},
    "includes": [5760],
  },
  "ash/webui/help_app_ui/resources/help_app_resources.grd": {
    "includes": [5780],
  },
  # Both help_app_kids_magazine_bundle_resources.grd and
  # help_app_kids_magazine_bundle_mock_resources.grd start with the same id
  # because only one of them is built depending on if src_internal is available.
  # Lower bound for number of resource ids is the number of files, which is 3 in
  # in this case (HTML, JS and CSS file).
  "ash/webui/help_app_ui/resources/prod/help_app_kids_magazine_bundle_resources.grd": {
    "META": {"sizes": {"includes": [15],}},
    "includes": [5800],
  },
  "ash/webui/help_app_ui/resources/mock/help_app_kids_magazine_bundle_mock_resources.grd": {
    "includes": [5800],
  },
  # Both help_app_bundle_resources.grd and help_app_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available. Lower bound is that we bundle ~100 images for
  # offline articles with the app, as well as strings in every language (74),
  # and bundled content in the top 25 languages (25 x 2).
  "ash/webui/help_app_ui/resources/prod/help_app_bundle_resources.grd": {
    "META": {"sizes": {"includes": [300],}},  # Relies on src-internal.
    "includes": [5820],
  },
  "ash/webui/help_app_ui/resources/mock/help_app_bundle_mock_resources.grd": {
    "includes": [5820],
  },
  "ash/webui/media_app_ui/resources/media_app_resources.grd": {
    "META": {"join": 2},
    "includes": [5840],
  },
  # Both media_app_bundle_resources.grd and media_app_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available. Lower bound for number of resource ids is number
  # of languages (74).
  "ash/webui/media_app_ui/resources/prod/media_app_bundle_resources.grd": {
    "META": {"sizes": {"includes": [130],}},  # Relies on src-internal.
    "includes": [5860],
  },
  "ash/webui/media_app_ui/resources/mock/media_app_bundle_mock_resources.grd": {
    "includes": [5860],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/print_management/resources/resources.grd": {
    "META": {"join": 2, "sizes": {"includes": [20]}},
    "includes": [5880],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/sample_system_web_app_ui/resources/trusted/ash_sample_system_web_app_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [5900],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/sample_system_web_app_ui/resources/untrusted/ash_sample_system_web_app_untrusted_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [5920],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/scanning/resources/resources.grd": {
    "META": {"sizes": {"includes": [100],}},
    "includes": [5940],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/status_area_internals/resources/resources.grd": {
    "META": {"sizes": {"includes": [30],}},
    "includes": [5960],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/system_extensions_internals_ui/ash_system_extensions_internals_resources.grd": {
    "META": {"sizes": {"includes": [10],}},
    "includes": [5980],
  },
  "chromeos/resources/chromeos_resources.grd": {
    "includes": [6000],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/eche_app_ui/ash_eche_app_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [6020],
  },
  # Both ash_eche_bundle_resources.grd and ash_eche_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available.
  "ash/webui/eche_app_ui/resources/prod/ash_eche_bundle_resources.grd": {
    "META": {"sizes": {"includes": [120],}},
    "includes": [6040],
  },
  "ash/webui/eche_app_ui/resources/mock/ash_eche_bundle_mock_resources.grd": {
    "META": {"sizes": {"includes": [120],}},
    "includes": [6040],
  },
  "ash/webui/multidevice_debug/resources/multidevice_debug_resources.grd": {
    "META": {"join": 2},
    "includes": [6060],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/personalization_app/resources/resources.grd": {
    "META": {"sizes": {"includes": [200],}},
    "includes": [6080],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/demo_mode_app_ui/ash_demo_mode_app_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
   "includes": [6100],
  },

 "<(SHARED_INTERMEDIATE_DIR)/ash/webui/projector_app/resources/app/untrusted/ash_projector_app_untrusted_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [6120],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/projector_app/resources/annotator/untrusted/ash_projector_annotator_untrusted_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [6140],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/projector_app/resources/common/ash_projector_common_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [6160],
  },

  # Both projector_app_bundle_resources.grd and projector_app_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available. Lower bound for number of resource ids is number
  # of languages (79).
  "ash/webui/projector_app/resources/prod/projector_app_bundle_resources.grd": {
    "META": {"sizes": {"includes": [120],}}, # Relies on src-internal.
    "includes": [6180],
  },
  "ash/webui/projector_app/resources/mock/projector_app_bundle_mock_resources.grd": {
    "includes": [6180],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/face_ml_app_ui/resources/trusted/ash_face_ml_app_resources.grd": {
    "META": {"join": 2, "sizes": {"includes": [50],}},
    "includes": [6200],
  },
  # Both face_ml_app_bundle_resources.grd and face_ml_app_bundle_mock_resources.grd
  # start with the same id because only one of them is built depending on if
  # src_internal is available.
  "ash/webui/face_ml_app_ui/resources/prod/face_ml_app_bundle_resources.grd": {
    "META": {"sizes": {"includes": [120],}},  # Relies on src-internal.
    "includes": [6220],
  },
  "ash/webui/face_ml_app_ui/resources/mock/face_ml_app_bundle_mock_resources.grd": {
    "includes": [6220],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/face_ml_app_ui/resources/untrusted/ash_face_ml_app_untrusted_resources.grd": {
    "META": {"join": 2, "sizes": {"includes": [50],}},
    "includes": [6240],
  },
  # END chromeos/ section.

  # START components/ section.
  # TODO(b/207518736): Input overlay resources will be changed to proto soon,
  # thus not rushing to update it for now.
  "ash/components/arc/input_overlay/resources/input_overlay_resources.grd": {
    # Big alignment at start of section.
    "META": {"align": 1000},
    "includes": [7000],
  },
  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "components/components_chromium_strings.grd": {
    "messages": [7020],
  },
  "components/components_google_chrome_strings.grd": {
    "messages": [7020],
  },
  "components/components_locale_settings.grd": {
    "META": {"join": 2},
    "includes": [7040],
    "messages": [7060],
  },
  "components/components_strings.grd": {
    "messages": [7080],
  },
  "components/headless/command_handler/headless_command.grd": {
    "includes": [7100],
  },
  "components/omnibox/resources/omnibox_pedal_synonyms.grd": {
    "messages": [7120],
  },
  # components/policy/resources/policy_templates.grd and
  # components/policy/resources/policy_templates.build.grd must share the same
  # id because they are based on the same structure, however they are used in
  # different pipelines.
  "components/policy/resources/policy_templates.grd": {
    "structures": [7140],
  },
  "components/policy/resources/policy_templates.build.grd": {
    "structures": [7140],
  },
  "components/resources/components_resources.grd": {
    "includes": [7160],
  },
  "components/resources/components_scaled_resources.grd": {
    "structures": [7180],
  },
  "components/embedder_support/android/java/strings/web_contents_delegate_android_strings.grd": {
    "messages": [7200],
  },
  "components/autofill/core/browser/autofill_address_rewriter_resources.grd":{
    "includes": [7220]
  },
  # END components/ section.

  # START ios/ section.
  #
  # chrome/ and ios/chrome/ must start at the same id.
  # App only use one file depending on whether it is iOS or other platform.
  "ios/chrome/app/resources/ios_resources.grd": {
    "includes": [800],
    "structures": [820],
  },

  # Chromium strings and Google Chrome strings must start at the same id.
  # We only use one file depending on whether we're building Chromium or
  # Google Chrome.
  "ios/chrome/app/strings/ios_chromium_strings.grd": {
    # Big alignment to make start IDs look nicer.
    "META": {"align": 100},
    "messages": [900],
  },
  "ios/chrome/app/strings/ios_google_chrome_strings.grd": {
    "messages": [900],
  },

  "ios/chrome/app/strings/ios_strings.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"join": 2, "align": 200},
    "messages": [1000],
  },
  "ios/chrome/app/theme/ios_theme_resources.grd": {
    # Big alignment since strings (previous item) are frequently added.
    "META": {"align": 100},
    "structures": [1100],
  },
  "ios/chrome/browser/ui/whats_new/strings/ios_whats_new_strings.grd": {
    "messages": [1120],
  },
  "ios/chrome/share_extension/strings/ios_share_extension_strings.grd": {
    "messages": [1140],
  },
  "ios/chrome/open_extension/strings/ios_open_extension_chromium_strings.grd": {
    "messages": [1160],
  },
  "ios/chrome/open_extension/strings/ios_open_extension_google_chrome_strings.grd": {
    "messages": [1160],
  },
  "ios/chrome/search_widget_extension/strings/ios_search_widget_extension_chromium_strings.grd": {
    "META": {"join": 2},
    "messages": [1180],
  },
  "ios/chrome/search_widget_extension/strings/ios_search_widget_extension_google_chrome_strings.grd": {
    "messages": [1180],
  },
  "ios/chrome/content_widget_extension/strings/ios_content_widget_extension_chromium_strings.grd": {
    "META": {"join": 2},
    "messages": [1200],
  },
  "ios/chrome/content_widget_extension/strings/ios_content_widget_extension_google_chrome_strings.grd": {
    "messages": [1200],
  },
  "ios/chrome/credential_provider_extension/strings/ios_credential_provider_extension_strings.grd": {
    "META": {"join": 2},
    "messages": [1220],
  },
  "ios/chrome/widget_kit_extension/strings/ios_widget_kit_extension_strings.grd": {
    "messages": [1240],
  },
  "ios/web/ios_web_resources.grd": {
    "includes": [1260],
  },
  "ios/web/test/test_resources.grd": {
    "includes": [1280],
  },
  # END ios/ section.

  # START ios_internal/ section.
  "ios_internal/chrome/app/ios_internal_strings.grd": {
    "messages": [1300],
  },
  "ios_internal/chrome/app/theme/mobile_theme_resources.grd": {
    "structures": [1320],
  },
  "ios_internal/chrome/app/ios_internal_chromium_strings.grd": {
    "META": {"join": 2},
    "messages": [7240],
  },
  "ios_internal/chrome/app/ios_internal_google_chrome_strings.grd": {
    "messages": [7240],
  },
  # END ios_internal/ section.

  # START content/ section.
  "content/content_resources.grd": {
    # Big alignment at start of section.
    "META": {"join": 2, "align": 100},
    "includes": [7300],
  },
  "content/shell/shell_resources.grd": {
    "includes": [7320],
  },
  "content/test/web_ui_mojo_test_resources.grd": {
    "includes": [7340],
  },

  # This file is generated during the build.
  "<(SHARED_INTERMEDIATE_DIR)/content/browser/tracing/tracing_resources.grd": {
    "META": {"sizes": {"includes": [20],}},
    "includes": [7360],
  },
  # END content/ section.

  # START "everything else" section.
  # Everything but chrome/, chromeos/, components/, content/, and ios/
  "ash/ash_strings.grd": {
    # Big alignment at start of section.
    "META": {"align": 100},
    "messages": [7400],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/os_feedback_ui/resources/ash_os_feedback_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [7420],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/os_feedback_ui/untrusted_resources/ash_os_feedback_untrusted_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [7440],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/firmware_update_ui/resources/resources.grd": {
    "META": {"sizes": {"includes": [200],}},
    "includes": [7460],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/shortcut_customization_ui/resources/ash_shortcut_customization_app_resources.grd": {
    "META": {"sizes": {"includes": [200],}},
    "includes": [7480],
  },
  "ash/shortcut_viewer/shortcut_viewer_strings.grd": {
    "messages": [7500],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ash/webui/shimless_rma/resources/ash_shimless_rma_resources.grd": {
    "META": {"sizes": {"includes": [80],}},
    "includes": [7520],
  },
  "ash/keyboard/ui/keyboard_resources.grd": {
    "includes": [7540],
  },
  "ash/login/resources/login_resources.grd": {
    "structures": [7560],
  },
  "ash/public/cpp/resources/ash_public_unscaled_resources.grd": {
    "includes": [7580],
    "structures": [7600],
  },
  "base/tracing/protos/resources.grd": {
    "includes": [7620],
  },
  "chromecast/app/resources/chromecast_settings.grd": {
    "messages": [7640],
  },
  "chromecast/app/resources/shell_resources.grd": {
    "includes": [7660],
  },
  "chromecast/renderer/resources/extensions_renderer_resources.grd": {
    "includes": [7680],
  },

  "device/bluetooth/bluetooth_strings.grd": {
    "messages": [7700],
  },

  "device/fido/fido_strings.grd": {
    "messages": [7720],
  },

  "extensions/browser/resources/extensions_browser_resources.grd": {
    "structures": [7740],
  },
  "extensions/extensions_resources.grd": {
    "includes": [7760],
  },
  "extensions/renderer/resources/extensions_renderer_resources.grd": {
    "includes": [7780],
    "structures": [7800],
  },
  "extensions/shell/app_shell_resources.grd": {
    "includes": [7820],
  },
  "extensions/strings/extensions_strings.grd": {
    "messages": [7840],
  },

  "mojo/public/js/mojo_bindings_resources.grd": {
    "includes": [7860],
  },

  "net/base/net_resources.grd": {
    "includes": [7880],
  },

  "remoting/resources/remoting_strings.grd": {
    "messages": [7900],
  },

  "services/services_strings.grd": {
    "messages": [7920],
  },
  "third_party/blink/public/blink_image_resources.grd": {
    "structures": [7940],
  },
  "third_party/blink/public/blink_resources.grd": {
    "includes": [7960],
  },
  "third_party/blink/renderer/modules/media_controls/resources/media_controls_resources.grd": {
    "includes": [7980],
    "structures": [8000],
  },
  "third_party/blink/public/strings/blink_accessibility_strings.grd": {
    "messages": [8020],
  },
  "third_party/blink/public/strings/blink_strings.grd": {
    "messages": [8040],
  },
  "third_party/libaddressinput/chromium/address_input_strings.grd": {
    "messages": [8060],
  },

  "ui/base/test/ui_base_test_resources.grd": {
    "messages": [8080],
  },
  "ui/chromeos/resources/ui_chromeos_resources.grd": {
    "structures": [8100],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/styles/cros_typography_resources.grd": {
    "META": {"sizes": {"includes": [5],}},
    "includes": [8120],
  },
  "ui/chromeos/ui_chromeos_strings.grd": {
    "messages": [8140],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ui/file_manager/file_manager_gen_resources.grd": {
    "META": {"sizes": {"includes": [2000]}},
    "includes": [8160],
  },
  "ui/file_manager/file_manager_resources.grd": {
    "includes": [8180],
  },
  "ui/resources/ui_resources.grd": {
    "structures": [8200],
  },
  "ui/resources/ui_unscaled_resources.grd": {
    "includes": [8220],
  },
  "ui/strings/app_locale_settings.grd": {
    "messages": [8240],
  },
  "ui/strings/ax_strings.grd": {
    "messages": [8260],
  },
  "ui/strings/ui_strings.grd": {
    "messages": [8280],
  },
  "ui/views/examples/views_examples_resources.grd": {
    "messages": [8300],
  },
  "ui/views/resources/views_resources.grd": {
    "structures": [8320],
  },
  "ui/webui/examples/resources/webui_examples_resources.grd": {
    "messages": [8340],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ui/webui/examples/resources/browser/resources.grd": {
    "META": {"sizes": {"includes": [10]}},
    "includes": [8360],
  },
  "<(SHARED_INTERMEDIATE_DIR)/ui/webui/resources/webui_resources.grd": {
    "META": {"sizes": {"includes": [1100]}},
    "includes": [8380],
  },
  "weblayer/weblayer_resources.grd": {
    "includes": [8400],
  },

  # This file is generated during the build.
  # .grd extension is required because it's checked before var interpolation.
  "<(DEVTOOLS_GRD_PATH).grd": {
    # In debug build, devtools frontend sources are not bundled and therefore
    # includes a lot of individual resources
    "META": {"sizes": {"includes": [2500],}},
    "includes": [8420],
  },

  # This file is generated during the build.
  "<(SHARED_INTERMEDIATE_DIR)/resources/inspector_overlay/inspector_overlay_resources.grd": {
    "META": {"sizes": {"includes": [50],}},
    "includes": [8440],
  },

  # END "everything else" section.
  # Everything but chrome/, components/, content/, and ios/

  # Thinking about appending to the end?
  # Please read the header and find the right section above instead.
}
