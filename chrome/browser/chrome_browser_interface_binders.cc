// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_host.h"
#include "chrome/browser/predictors/network_hints_handler_impl.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_processor_impl_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/speech/on_device_speech_recognition_impl.h"
#include "chrome/browser/translate/translate_frame_binder.h"
#include "chrome/browser/ui/search_engines/search_engine_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/language_detection/content/common/language_detection.mojom.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl.h"
#include "components/performance_manager/embedder/binders.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/translate/content/common/translate.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/facilitated_payments/payment_link_handler.mojom.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/payments/secure_payment_confirmation_service.mojom.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "chrome/browser/android/contextualsearch/unhandled_tap_notifier_impl.h"
#include "chrome/browser/android/contextualsearch/unhandled_tap_web_contents_observer.h"
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom.h"
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"
#include "chrome/browser/facilitated_payments/payment_link_handler_binder.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#else
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/payments/payment_request_factory.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/web_applications/web_install_service_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/digital_goods/digital_goods_factory_impl.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#include "chrome/browser/webshare/share_service_impl.h"
#include "chrome/common/chrome_features.h"
#endif
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
#include "chrome/browser/accessibility/live_caption/live_caption_speech_recognition_host.h"
#include "chrome/browser/accessibility/live_caption/live_caption_unavailability_notifier.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/media/media_foundation_service_monitor.h"
#include "media/mojo/mojom/media_foundation_preferences.mojom.h"
#include "media/mojo/services/media_foundation_preferences.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#endif  // BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/web_api/web_printing_service_binder.h"
#include "third_party/blink/public/mojom/printing/web_printing.mojom.h"
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#endif

namespace chrome::internal {

namespace {

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
void BindUnhandledTapWebContentsObserver(
    content::RenderFrameHost* const host,
    mojo::PendingReceiver<blink::mojom::UnhandledTapNotifier> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(host);
  if (!web_contents) {
    return;
  }

  auto* unhandled_tap_notifier_observer =
      contextual_search::UnhandledTapWebContentsObserver::FromWebContents(
          web_contents);
  if (!unhandled_tap_notifier_observer) {
    return;
  }

  contextual_search::CreateUnhandledTapNotifierImpl(
      unhandled_tap_notifier_observer->unhandled_tap_callback(),
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

// Forward image Annotator requests to the profile's AccessibilityLabelsService.
void BindImageAnnotator(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver) {
  AccessibilityLabelsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(
          frame_host->GetProcess()->GetBrowserContext()))
      ->BindImageAnnotator(std::move(receiver));
}

void BindDistillabilityService(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<dom_distiller::mojom::DistillabilityService>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    return;
  }

  dom_distiller::DistillabilityDriver* driver =
      dom_distiller::DistillabilityDriver::FromWebContents(web_contents);
  if (!driver) {
    return;
  }
  driver->SetIsSecureCallback(
      base::BindRepeating([](content::WebContents* contents) {
        // SecurityStateTabHelper uses chrome-specific
        // GetVisibleSecurityState to determine if a page is SECURE.
        return SecurityStateTabHelper::FromWebContents(contents)
                   ->GetSecurityLevel() ==
               security_state::SecurityLevel::SECURE;
      }));
  driver->CreateDistillabilityService(std::move(receiver));
}

void BindDistillerJavaScriptService(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<dom_distiller::mojom::DistillerJavaScriptService>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    return;
  }

  dom_distiller::DomDistillerService* dom_distiller_service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
#if BUILDFLAG(IS_ANDROID)
  static_cast<dom_distiller::android::DistillerUIHandleAndroid*>(
      dom_distiller_service->GetDistillerUIHandle())
      ->set_render_frame_host(frame_host);
#endif
  CreateDistillerJavaScriptService(dom_distiller_service->GetWeakPtr(),
                                   std::move(receiver));
}

void BindNoStatePrefetchCanceler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<prerender::mojom::NoStatePrefetchCanceler> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    return;
  }

  auto* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents);
  if (!no_state_prefetch_contents) {
    return;
  }
  no_state_prefetch_contents->AddNoStatePrefetchCancelerReceiver(
      std::move(receiver));
}

void BindNoStatePrefetchProcessor(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::NoStatePrefetchProcessor> receiver) {
  prerender::NoStatePrefetchProcessorImpl::Create(
      frame_host, std::move(receiver),
      std::make_unique<
          prerender::ChromeNoStatePrefetchProcessorImplDelegate>());
}

#if BUILDFLAG(IS_ANDROID)
template <typename Interface>
void ForwardToJavaWebContents(content::RenderFrameHost* frame_host,
                              mojo::PendingReceiver<Interface> receiver) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  if (contents) {
    contents->GetJavaInterfaces()->GetInterface(std::move(receiver));
  }
}

template <typename Interface>
void ForwardToJavaFrame(content::RenderFrameHost* render_frame_host,
                        mojo::PendingReceiver<Interface> receiver) {
  render_frame_host->GetJavaInterfaces()->GetInterface(std::move(receiver));
}
#endif

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  predictors::NetworkHintsHandlerImpl::Create(frame_host, std::move(receiver));
}

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
void BindSpeechRecognitionContextHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  if (!captions::IsLiveCaptionFeatureSupported()) {
    return;
  }

  // Bind via the appropriate factory.
  Profile* profile = Profile::FromBrowserContext(
      frame_host->GetProcess()->GetBrowserContext());
#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
  auto* factory = SpeechRecognitionServiceFactory::GetForProfile(profile);
#elif BUILDFLAG(IS_CHROMEOS)
  auto* factory = CrosSpeechRecognitionServiceFactory::GetForProfile(profile);
#else
#error "No speech recognition service factory on this platform."
#endif
  factory->BindSpeechRecognitionContext(std::move(receiver));
}

void BindSpeechRecognitionClientBrowserInterfaceHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionClientBrowserInterface>
        receiver) {
  if (captions::IsLiveCaptionFeatureSupported()) {
    // Bind in this process.
    Profile* profile = Profile::FromBrowserContext(
        frame_host->GetProcess()->GetBrowserContext());
    SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile)
        ->BindReceiver(std::move(receiver));
  }
}

void BindSpeechRecognitionRecognizerClientHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        client_receiver) {
  Profile* profile = Profile::FromBrowserContext(
      frame_host->GetProcess()->GetBrowserContext());
  PrefService* profile_prefs = profile->GetPrefs();
  if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled) &&
      captions::IsLiveCaptionFeatureSupported()) {
    captions::LiveCaptionSpeechRecognitionHost::Create(
        frame_host, std::move(client_receiver));
  }
}

#if BUILDFLAG(IS_WIN)
void BindMediaFoundationRendererNotifierHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
        receiver) {
  if (captions::IsLiveCaptionFeatureSupported()) {
    captions::LiveCaptionUnavailabilityNotifier::Create(frame_host,
                                                        std::move(receiver));
  }
}
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)

void BindOnDeviceSpeechRecognitionHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::OnDeviceSpeechRecognition> receiver) {
  speech::OnDeviceSpeechRecognitionImpl::GetOrCreateForCurrentDocument(
      frame_host)
      ->Bind(std::move(receiver));
}

#if BUILDFLAG(IS_WIN)
void BindMediaFoundationPreferences(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::MediaFoundationPreferences> receiver) {
  MediaFoundationPreferencesImpl::Create(
      frame_host->GetSiteInstance()->GetSiteURL(),
      base::BindRepeating(&MediaFoundationServiceMonitor::
                              IsHardwareSecureDecryptionAllowedForSite),
      std::move(receiver));
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
void BindScreenAIAnnotator(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<screen_ai::mojom::ScreenAIAnnotator> receiver) {
  content::BrowserContext* browser_context =
      frame_host->GetProcess()->GetBrowserContext();

  screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(browser_context)
      ->BindScreenAIAnnotator(std::move(receiver));
}

void BindScreen2xMainContentExtractor(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<screen_ai::mojom::Screen2xMainContentExtractor>
        receiver) {
  screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
      frame_host->GetProcess()->GetBrowserContext())
      ->BindMainContentExtractor(std::move(receiver));
}
#endif

}  // namespace

void PopulateChromeFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  map->Add<image_annotation::mojom::Annotator>(
      base::BindRepeating(&BindImageAnnotator));

  map->Add<blink::mojom::AnchorElementMetricsHost>(
      base::BindRepeating(&NavigationPredictor::Create));

  map->Add<blink::mojom::LCPCriticalPathPredictorHost>(
      base::BindRepeating(&predictors::LCPCriticalPathPredictorHost::Create));

  map->Add<dom_distiller::mojom::DistillabilityService>(
      base::BindRepeating(&BindDistillabilityService));

  map->Add<dom_distiller::mojom::DistillerJavaScriptService>(
      base::BindRepeating(&BindDistillerJavaScriptService));

  map->Add<prerender::mojom::NoStatePrefetchCanceler>(
      base::BindRepeating(&BindNoStatePrefetchCanceler));

  map->Add<blink::mojom::NoStatePrefetchProcessor>(
      base::BindRepeating(&BindNoStatePrefetchProcessor));

  auto* pm_registry =
      performance_manager::PerformanceManagerRegistry::GetInstance();
  if (pm_registry) {
    pm_registry->GetBinders().ExposeInterfacesToRenderFrame(map);
  }

  map->Add<translate::mojom::ContentTranslateDriver>(
      base::BindRepeating(&translate::BindContentTranslateDriver));

  if (!base::FeatureList::IsEnabled(blink::features::kLanguageDetectionAPI)) {
    // When the feature is enabled, the driver is bound by
    // browser_interface_binders.cc to make it available to JS execution
    // contexts. When the feature is disabled, we bind it here for Chrome's
    // page-translation feature.
    //
    // TODO(https://crbug.com/354069716): Remove this when the flag is removed.
    map->Add<language_detection::mojom::ContentLanguageDetectionDriver>(
        base::BindRepeating(&translate::BindContentLanguageDetectionDriver));
  }

  map->Add<blink::mojom::CredentialManager>(
      base::BindRepeating(&ChromePasswordManagerClient::BindCredentialManager));

  map->Add<chrome::mojom::OpenSearchDescriptionDocumentHandler>(
      base::BindRepeating(
          &SearchEngineTabHelper::BindOpenSearchDescriptionDocumentHandler));

#if BUILDFLAG(IS_ANDROID)
  map->Add<blink::mojom::InstalledAppProvider>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::InstalledAppProvider>));
  map->Add<payments::mojom::DigitalGoodsFactory>(base::BindRepeating(
      &ForwardToJavaFrame<payments::mojom::DigitalGoodsFactory>));
#if defined(BROWSER_MEDIA_CONTROLS_MENU)
  map->Add<blink::mojom::MediaControlsMenuHost>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::MediaControlsMenuHost>));
#endif
  map->Add<chrome::mojom::OfflinePageAutoFetcher>(
      base::BindRepeating(&offline_pages::OfflinePageAutoFetcher::Create));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(base::BindRepeating(
        &ForwardToJavaFrame<payments::mojom::PaymentRequest>));
  }
  map->Add<blink::mojom::ShareService>(base::BindRepeating(
      &ForwardToJavaWebContents<blink::mojom::ShareService>));

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
  map->Add<blink::mojom::UnhandledTapNotifier>(
      base::BindRepeating(&BindUnhandledTapWebContentsObserver));
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#else
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&badging::BadgeManager::BindFrameReceiverIfAllowed));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(
        base::BindRepeating(&payments::CreatePaymentRequest));
  }
  if (base::FeatureList::IsEnabled(blink::features::kWebAppInstallation) &&
      !render_frame_host->GetParentOrOuterDocument()) {
    map->Add<blink::mojom::WebInstallService>(
        base::BindRepeating(&web_app::WebInstallServiceImpl::CreateIfAllowed));
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  map->Add<payments::mojom::DigitalGoodsFactory>(base::BindRepeating(
      &apps::DigitalGoodsFactoryImpl::BindDigitalGoodsFactory));
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kWebShare)) {
    map->Add<blink::mojom::ShareService>(
        base::BindRepeating(&ShareServiceImpl::Create));
  }
#endif

  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));
  map->Add<media::mojom::OnDeviceSpeechRecognition>(
      base::BindRepeating(&BindOnDeviceSpeechRecognitionHandler));

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
  map->Add<media::mojom::SpeechRecognitionContext>(
      base::BindRepeating(&BindSpeechRecognitionContextHandler));
  map->Add<media::mojom::SpeechRecognitionClientBrowserInterface>(
      base::BindRepeating(&BindSpeechRecognitionClientBrowserInterfaceHandler));
  map->Add<media::mojom::SpeechRecognitionRecognizerClient>(
      base::BindRepeating(&BindSpeechRecognitionRecognizerClientHandler));
#if BUILDFLAG(IS_WIN)
  map->Add<media::mojom::MediaFoundationRendererNotifier>(
      base::BindRepeating(&BindMediaFoundationRendererNotifierHandler));
#endif
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(blink::features::kDesktopPWAsSubApps) &&
      !render_frame_host->GetParentOrOuterDocument()) {
    // The service binder will reject non-primary main frames, but we still need
    // to register it for them because a non-primary main frame could become a
    // primary main frame at a later time (eg. a prerendered page).
    map->Add<blink::mojom::SubAppsService>(
        base::BindRepeating(&web_app::SubAppsServiceImpl::CreateIfAllowed));
  }

  map->Add<screen_ai::mojom::ScreenAIAnnotator>(
      base::BindRepeating(&BindScreenAIAnnotator));

  map->Add<screen_ai::mojom::Screen2xMainContentExtractor>(
      base::BindRepeating(&BindScreen2xMainContentExtractor));
#endif

#if BUILDFLAG(IS_WIN)
  map->Add<media::mojom::MediaFoundationPreferences>(
      base::BindRepeating(&BindMediaFoundationPreferences));
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  map->Add<blink::mojom::WebPrintingService>(
      base::BindRepeating(&printing::CreateWebPrintingServiceForFrame));
#endif

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(blink::features::kPaymentLinkDetection)) {
    map->Add<payments::facilitated::mojom::PaymentLinkHandler>(
        base::BindRepeating(&BindPaymentLinkHandler));
  }
#endif

#if BUILDFLAG(ENABLE_SPELLCHECK)
  map->Add<spellcheck::mojom::SpellCheckHost>(base::BindRepeating(
      [](content::RenderFrameHost* frame_host,
         mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
        SpellCheckHostChromeImpl::Create(
            frame_host->GetProcess()->GetDeprecatedID(), std::move(receiver));
      }));
#endif  // BUILDFLAG(ENABLE_SPELLCHECK)
}

}  // namespace chrome::internal
