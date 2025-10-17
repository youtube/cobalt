// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace lens::features {

BASE_FEATURE(kLensStandalone,
             "LensStandalone",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlay,
             "LensOverlay",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kLensOverlayTranslateButton,
             "LensOverlayTranslateButton",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayTranslateLanguages,
             "LensOverlayTranslateLanguages",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayImageContextMenuActions,
             "LensOverlayImageContextMenuActions",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayContextualSearchbox,
             "LensOverlayContextualSearchbox",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayContextualSearchboxForOmniboxSuggestions,
             "LensOverlayContextualSearchboxForOmniboxSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlaySuggestionsMigration,
             "LensOverlaySuggestionsMigration",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayLatencyOptimizations,
             "LensOverlayLatencyOptimizations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayRoutingInfo,
             "LensOverlayRoutingInfo",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlaySurvey,
             "LensOverlaySurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlaySidePanelOpenInNewTab,
             "LensOverlaySidePanelOpenInNewTab",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlaySimplifiedSelection,
             "LensOverlaySimplifiedSelection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayVisualSelectionUpdates,
             "LensOverlayVisualSelectionUpdates",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayVisualSelectionUpdatesForOmniboxSuggestions,
             "LensOverlayVisualSelectionUpdatesForOmniboxSuggestions",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayUpdatedClientContext,
             "LensOverlayUpdatedClientContext",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayMGTInSidePanel,
             "LensOverlayMGTInSidePanel",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensSearchSidePanelNewFeedback,
             "LensSearchSidePanelNewFeedback",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensSearchSidePanelScrollToAPI,
             "LensSearchSidePanelScrollToAPI",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the Lens Overlay omnibox entry point. This is a separate feature from
// kLensOverlay so that the omnibox entry point can be disabled without a
// dependency on the rest of the Lens Overlay features. This means if can be
// experimented with independently.
BASE_FEATURE(kLensOverlayOmniboxEntryPoint,
             "LensOverlayOmniboxEntryPoint",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayUploadChunking,
             "LensOverlayUploadChunking",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayRecontextualizeOnQuery,
             "LensOverlayRecontextualizeOnQuery",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayCornerSliders,
             "LensOverlayCornerSliders",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensSearchProtectedPage,
             "LensSearchProtectedPage",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kLensOverlayEduActionChip,
             "LensOverlayEduActionChip",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kLensOverlayMinRamMb{&kLensOverlay, "min_ram_mb",
                                                   /*default=value=*/-1};
const base::FeatureParam<std::string> kActivityUrl{
    &kLensOverlay, "activity-url",
    "https://myactivity.google.com/myactivity?pli=1"};
const base::FeatureParam<std::string> kHelpCenterUrl{
    &kLensOverlay, "help-center-url",
    "https://support.google.com/chrome?p=search_from_page"};
const base::FeatureParam<std::string> kResultsSearchUrl{
    &kLensOverlay, "results-search-url", "https://www.google.com/search"};
const base::FeatureParam<int> kLensOverlayScreenshotRenderQuality{
    &kLensOverlay, "overlay-screenshot-render-quality", 90};
const base::FeatureParam<int> kLensOverlayImageCompressionQuality{
    &kLensOverlay, "image-compression-quality", 40};
const base::FeatureParam<bool> kLensOverlayUseTieredDownscaling{
    &kLensOverlay, "enable-tiered-downscaling", false};
const base::FeatureParam<bool> kLensOverlaySendLatencyGen204{
    &kLensOverlay, "enable-gen204-latency", true};
const base::FeatureParam<bool> kLensOverlaySendTaskCompletionGen204{
    &kLensOverlay, "enable-gen204-task-completion", true};
const base::FeatureParam<bool> kLensOverlaySendSemanticEventGen204{
    &kLensOverlay, "enable-gen204-semantic-event", true};
const base::FeatureParam<int> kLensOverlayImageMaxArea{
    &kLensOverlay, "image-dimensions-max-area", 1500000};
const base::FeatureParam<int> kLensOverlayImageMaxHeight{
    &kLensOverlay, "image-dimensions-max-height", 1600};
const base::FeatureParam<int> kLensOverlayImageMaxWidth{
    &kLensOverlay, "image-dimensions-max-width", 1600};
const base::FeatureParam<int> kLensOverlayImageMaxAreaTier1{
    &kLensOverlay, "image-dimensions-max-area-tier-1", 1000000};
const base::FeatureParam<int> kLensOverlayImageMaxHeightTier1{
    &kLensOverlay, "image-dimensions-max-height-tier-1", 1600};
const base::FeatureParam<int> kLensOverlayImageMaxWidthTier1{
    &kLensOverlay, "image-dimensions-max-width-tier-1", 1600};
const base::FeatureParam<int> kLensOverlayImageMaxAreaTier2{
    &kLensOverlay, "image-dimensions-max-area-tier-2", 2000000};
const base::FeatureParam<int> kLensOverlayImageMaxHeightTier2{
    &kLensOverlay, "image-dimensions-max-height-tier-2", 1890};
const base::FeatureParam<int> kLensOverlayImageMaxWidthTier2{
    &kLensOverlay, "image-dimensions-max-width-tier-2", 1890};
const base::FeatureParam<int> kLensOverlayImageMaxAreaTier3{
    &kLensOverlay, "image-dimensions-max-area-tier-3", 3000000};
const base::FeatureParam<int> kLensOverlayImageMaxHeightTier3{
    &kLensOverlay, "image-dimensions-max-height-tier-3", 2300};
const base::FeatureParam<int> kLensOverlayImageMaxWidthTier3{
    &kLensOverlay, "image-dimensions-max-width-tier-3", 2300};
const base::FeatureParam<int> kLensOverlayImageDownscaleUiScalingFactor{
    &kLensOverlay, "image-downscale-ui-scaling-factor", 2};
const base::FeatureParam<bool> kLensOverlayDebuggingMode{
    &kLensOverlay, "debugging-mode", false};
const base::FeatureParam<int> kLensOverlayVerticalTextMargin{
    &kLensOverlay, "text-vertical-margin", 12};
const base::FeatureParam<int> kLensOverlayHorizontalTextMargin{
    &kLensOverlay, "text-horizontal-margin", 4};
const base::FeatureParam<bool> kLensOverlayEnableShimmer{
    &kLensOverlay, "enable-shimmer", true};
const base::FeatureParam<bool> kLensOverlayEnableShimmerSparkles{
    &kLensOverlay, "enable-shimmer-sparkles", true};
const base::FeatureParam<std::string> kResultsSearchLoadingUrl{
    &kLensOverlay, "results-search-loading-url",
    "https://www.gstatic.com/lens/chrome/"
    "lens_overlay_sidepanel_results_ghostloader_light-"
    "71af0ff0f00a1a03d3fe8abad71a2665.svg"};
const base::FeatureParam<std::string> kResultsSearchLoadingDarkModeUrl{
    &kLensOverlay, "results-search-loading-dark-mode-url",
    "https://www.gstatic.com/lens/chrome/"
    "lens_overlay_sidepanel_results_ghostloader_dark-"
    "b7b5c4f8c8891c881b7a20344f5298b0.svg"};

const base::FeatureParam<bool> kLensOverlayGoogleDseRequired{
    &kLensOverlay, "google-dse-required", true};

const base::FeatureParam<bool> kUseLensOverlayForImageSearch{
    &kLensOverlay, "use-for-image-search", true};

const base::FeatureParam<bool> kUseLensOverlayForVideoFrameSearch{
    &kLensOverlay, "use-for-video-frame-search", true};

const base::FeatureParam<bool> kIsOmniboxEntryPointEnabled{
    &kLensOverlay, "omnibox-entry-point", true};

constexpr base::FeatureParam<bool> kIsOmniboxEntrypointAlwaysVisible{
    &kLensOverlay, "omnibox-entry-point-always-visible", false};

const base::FeatureParam<bool> kUseBrowserDarkModeSettingForLensOverlay{
    &kLensOverlay, "use-browser-dark-mode-setting", true};

const base::FeatureParam<bool> kDynamicThemeForLensOverlay{
    &kLensOverlay, "use-dynamic-theme", true};

const base::FeatureParam<double> kDynamicThemeMinPopulationPct{
    &kLensOverlay, "use-dynamic-theme-min-population-pct", 0.002f};

const base::FeatureParam<double> kDynamicThemeMinChroma{
    &kLensOverlay, "use-dynamic-theme-min-chroma", 3.0f};

const base::FeatureParam<bool>
    kSendVisualSearchInteractionParamForLensTextQueries{
        &kLensOverlay, "send-vsint-for-text-selections", true};

constexpr base::FeatureParam<std::string> kLensOverlayEndpointUrl{
    &kLensOverlay, "endpoint-url",
    "https://lensfrontend-pa.googleapis.com/v1/crupload"};

constexpr base::FeatureParam<bool> kUseOauthForLensOverlayRequests{
    &kLensOverlay, "use-oauth-for-requests", true};

constexpr base::FeatureParam<int> kLensOverlayClusterInfoLifetimeSeconds{
    &kLensOverlay, "cluster-info-lifetime-seconds", 1800};

constexpr base::FeatureParam<int> kLensOverlayTapRegionHeight{
    &kLensOverlay, "tap-region-height", 300};
constexpr base::FeatureParam<int> kLensOverlayTapRegionWidth{
    &kLensOverlay, "tap-region-width", 300};

constexpr base::FeatureParam<double>
    kLensOverlaySelectTextOverRegionTriggerThreshold{
        &kLensOverlay, "select-text-over-region-trigger-threshold", 0.1};

constexpr base::FeatureParam<int> kLensOverlaySignificantRegionMinArea{
    &kLensOverlay, "significant-regions-min-area", 500};

constexpr base::FeatureParam<int> kLensOverlayMaxSignificantRegions{
    &kLensOverlay, "max-significant-regions", 100};

constexpr base::FeatureParam<bool> kLensOverlayUseBlur{&kLensOverlay,
                                                       "use-blur", true};

constexpr base::FeatureParam<int> kLensOverlayCustomBlurBlurRadiusPixels{
    &kLensOverlay, "custom-blur-blur-radius-pixels", 60};

constexpr base::FeatureParam<double> kLensOverlayCustomBlurQuality{
    &kLensOverlay, "custom-blur-quality", 0.1};

constexpr base::FeatureParam<double> kLensOverlayCustomBlurRefreshRateHertz{
    &kLensOverlay, "custom-blur-refresh-rate-hertz", 30};

constexpr base::FeatureParam<double>
    kLensOverlayPostSelectionComparisonThreshold{
        &kLensOverlay, "post-selection-comparison-threshold", 0.005};

constexpr base::FeatureParam<int> kLensOverlayServerRequestTimeout{
    &kLensOverlay, "server-request-timeout", 10000};

constexpr base::FeatureParam<bool> kLensOverlayEnableErrorPage{
    &kLensOverlay, "enable-error-page-webui", true};

constexpr base::FeatureParam<std::string> kLensOverlayGscQueryParamValue{
    &kLensOverlay, "gsc-query-param-value", "2"};

const base::FeatureParam<bool> kLensOverlayEnableInFullscreen{
    &kLensOverlay, "enable-in-fullscreen", true};

constexpr base::FeatureParam<int> kLensOverlaySegmentationMaskCornerRadius{
    &kLensOverlay, "segmentation-mask-corner-radius", 12};

constexpr base::FeatureParam<bool>
    kLensOverlayImageContextMenuActionsEnableCopyAsImage{
        &kLensOverlayImageContextMenuActions, "enable-copy-as-image", true};

constexpr base::FeatureParam<bool>
    kLensOverlayImageContextMenuActionsEnableSaveAsImage{
        &kLensOverlayImageContextMenuActions, "enable-save-as-image", false};

constexpr base::FeatureParam<int>
    kLensOverlayImageContextMenuActionsTextReceivedTimeout{
        &kLensOverlayImageContextMenuActions, "text-received-timeout", 2000};

constexpr base::FeatureParam<bool> kEnableClusterInfoOptimization{
    &kLensOverlayLatencyOptimizations, "enable-cluster-info-optimization",
    true};

constexpr base::FeatureParam<bool> kEnableEarlyInteractionOptimization{
    &kLensOverlayLatencyOptimizations, "enable-early-interaction-optimization",
    true};

constexpr base::FeatureParam<bool> kEnableEarlyStartQueryFlowOptimization{
    &kLensOverlayLatencyOptimizations,
    "enable-early-start-query-flow-optimization", true};

constexpr base::FeatureParam<bool>
    kSendClientContextToClusterInfoRequestForContextualSuggest{
        &kLensOverlayContextualSearchbox,
        "send-client-context-to-cluster-info-request-for-contextual-suggest",
        true};

constexpr base::FeatureParam<bool> kUseUpdatedContentFields{
    &kLensOverlayContextualSearchbox, "use-updated-content-fields", true};

constexpr base::FeatureParam<bool> kUsePdfsAsContext{
    &kLensOverlayContextualSearchbox, "use-pdfs-as-context", true};

constexpr base::FeatureParam<bool> kUseInnerTextAsContext{
    &kLensOverlayContextualSearchbox, "use-inner-text-as-context", true};

constexpr base::FeatureParam<bool> kUseInnerHtmlAsContext{
    &kLensOverlayContextualSearchbox, "use-inner-html-as-context", false};

constexpr base::FeatureParam<bool> kUseApcAsContext{
    &kLensOverlayContextualSearchbox, "use-apc-as-context", true};

constexpr base::FeatureParam<bool> kSendPageUrlForContextualization{
    &kLensOverlayContextualSearchbox, "send-page-url-for-contextualization",
    true};

constexpr base::FeatureParam<bool> kSendPageTitleForContextualization{
    &kLensOverlayContextualSearchbox, "send-page-title-for-contextualization",
    true};

constexpr base::FeatureParam<int> kLensOverlayPageContentRequestTimeoutMs{
    &kLensOverlayContextualSearchbox, "page-content-request-timeout-ms", 60000};

constexpr base::FeatureParam<bool>
    kUseVideoContextForTextOnlyLensOverlayRequests{
        &kLensOverlayContextualSearchbox,
        "use-video-context-for-text-only-requests", false};

constexpr base::FeatureParam<bool>
    kUseVideoContextForMultimodalLensOverlayRequests{
        &kLensOverlayContextualSearchbox,
        "use-video-context-for-multimodal-requests", false};

constexpr base::FeatureParam<std::string> kLensOverlayClusterInfoEndpointUrl{
    &kLensOverlayContextualSearchbox, "cluster-info-endpoint-url",
    "https://lensfrontend-pa.googleapis.com/v1/gsessionid"};

constexpr base::FeatureParam<bool>
    kLensOverlaySendLensInputsForContextualSuggest{
        &kLensOverlayContextualSearchbox,
        "send-lens-inputs-for-contextual-suggest", true};

constexpr base::FeatureParam<bool> kLensOverlaySendLensInputsForLensSuggest{
    &kLensOverlaySuggestionsMigration, "send-lens-inputs-for-lens-suggest",
    false};

constexpr base::FeatureParam<bool> kEnableContextualSearchboxGhostLoader{
    &kLensOverlayContextualSearchbox,
    "enable-contextual-searchbox-ghost-loader", true};

constexpr base::FeatureParam<bool>
    kShowContextualSearchboxGhostLoaderLoadingState{
        &kLensOverlayContextualSearchbox,
        "show-contextual-searchbox-ghost-loader-loading-state", true};

constexpr base::FeatureParam<base::TimeDelta> kLensSearchboxAutocompleteTimeout{
    &kLensOverlayContextualSearchbox, "lens-searchbox-autocomplete-timeout",
    base::Milliseconds(10000)};

constexpr base::FeatureParam<bool> kShowContextualSearchboxSearchSuggest{
    &kLensOverlayContextualSearchbox,
    "show-contextual-searchbox-search-suggest", false};

constexpr base::FeatureParam<bool> kShowContextualSearchboxZeroPrefixSuggest{
    &kLensOverlayContextualSearchbox, "enable-zps-suggestions", true};

constexpr base::FeatureParam<bool>
    kLensOverlaySendLensVisualInteractionDataForLensSuggest{
        &kLensOverlaySuggestionsMigration,
        "send-lens-visual-interaction-data-for-lens-suggest", false};

constexpr base::FeatureParam<bool> kLensOverlaySendImageSignalsForLensSuggest{
    &kLensOverlaySuggestionsMigration, "send-image-signals-for-lens-suggest",
    true};

constexpr base::FeatureParam<size_t> kLensOverlayFileUploadLimitBytes{
    &kLensOverlayContextualSearchbox, "file-upload-limit-bytes", 200000000};

constexpr base::FeatureParam<size_t> kLensOverlayPdfTextCharacterLimit{
    &kLensOverlayContextualSearchbox, "pdf-text-character-limit", 5000};

const base::FeatureParam<base::TimeDelta> kLensOverlaySurveyResultsTime{
    &kLensOverlaySurvey, "results-time", base::Seconds(1)};

constexpr base::FeatureParam<bool> kUsePdfVitParam{
    &kLensOverlayContextualSearchbox, "use-pdf-vit-param", true};

constexpr base::FeatureParam<bool> kUseWebpageVitParam{
    &kLensOverlayContextualSearchbox, "use-webpage-vit-param", true};

constexpr base::FeatureParam<bool> kUsePdfInteractionType{
    &kLensOverlayContextualSearchbox, "use-pdf-interaction-type", true};

constexpr base::FeatureParam<bool> kUseWebpageInteractionType{
    &kLensOverlayContextualSearchbox, "use-webpage-interaction-type", true};

constexpr base::FeatureParam<int> kScannedPdfCharacterPerPageHeuristic{
    &kLensOverlayContextualSearchbox, "characters-per-page-heuristic", 200};

constexpr base::FeatureParam<bool> kHandleSidePanelTextDirectives{
    &kLensOverlayContextualSearchbox, "handle-side-panel-text-directives",
    true};

constexpr base::FeatureParam<bool> kHoldContextualQueriesUntilAck{
    &kLensOverlayContextualSearchbox, "hold-csb-queries-until-ack", true};

constexpr base::FeatureParam<bool> kZstdCompressPdfBytes{
    &kLensOverlayContextualSearchbox, "zstd-compress-pdf-bytes", true};

constexpr base::FeatureParam<int> kZstdCompressionLevel{
    &kLensOverlayContextualSearchbox, "zstd-compression-level", 3};

constexpr base::FeatureParam<bool> kPageContentUploadRequestIdFix{
    &kLensOverlayContextualSearchbox, "page-content-request-id-fix", true};

constexpr base::FeatureParam<bool> kShowUploadProgressBar{
    &kLensOverlayContextualSearchbox, "show-upload-progress-bar", true};

constexpr base::FeatureParam<double> kUploadProgressBarShowHeuristic{
    &kLensOverlayContextualSearchbox, "upload-progress-bar-show-heuristic",
    0.1};

constexpr base::FeatureParam<bool> kAutoFocusSearchbox{
    &kLensOverlayContextualSearchbox, "auto-focus-searchbox", false};

constexpr base::FeatureParam<bool> kAutoFocusSearchboxForOmniboxSuggestions{
    &kLensOverlayContextualSearchboxForOmniboxSuggestions,
    "auto-focus-searchbox", true};

constexpr base::FeatureParam<bool> kUpdateViewportEachQuery{
    &kLensOverlayContextualSearchbox, "update-viewport-each-query", true};

constexpr base::FeatureParam<bool> kSendPdfCurrentPage{
    &kLensOverlayContextualSearchbox, "send-pdf-current-page", true};

constexpr base::FeatureParam<bool> kUseAltLoadingHintWeb{
    &kLensOverlayContextualSearchbox, "use-alt-loading-hint-web", false};
constexpr base::FeatureParam<bool> kUseAltLoadingHintPdf{
    &kLensOverlayContextualSearchbox, "use-alt-loading-hint-pdf", false};

constexpr base::FeatureParam<bool>
    kLensOverlayEnableSummarizeHintForContextualSuggest{
        &kLensOverlayContextualSearchbox,
        "enable-summarize-hint-for-contextual-suggest", false};

constexpr base::FeatureParam<std::string> kTranslateEndpointUrl{
    &kLensOverlayTranslateLanguages, "translate-endpoint-url",
    "https://translate-pa.googleapis.com/v1/supportedLanguages"};
constexpr base::FeatureParam<std::string> kSupportedSourceTranslateLanguages{
    &kLensOverlayTranslateLanguages, "supported-source-languages",
    "aa,ab,ace,ach,af,ak,alz,am,ar,as,av,awa,ay,az,ba,ban,bbc,bci,be,bem,ber-"
    "Latn,bew,bg,bho,bik,bm,bn,bo,br,bs,bts,btx,bua,ca,ce,ceb,cgg,ch,chk,chm,"
    "ckb,cnh,co,crh,crs,cs,cv,cy,da,de,doi,dov,dyu,dz,ee,el,en,eo,es,et,eu,fa,"
    "fa-AF,ff,fi,fj,fo,fon,fr,fur,fy,ga,gaa,gd,gl,gn,gom,gu,gv,ha,haw,hi,hil,"
    "hmn,hr,hrx,ht,hu,hy,iba,id,ig,ilo,is,it,iw,ja,jam,jw,ka,kac,kek,kg,kha,kk,"
    "kl,km,kn,ko,kr,kri,ktu,ku,kv,ky,la,lb,lg,li,lij,lmo,ln,lo,lt,ltg,luo,lus,"
    "lv,mad,mai,mak,mam,mfe,mg,mh,mi,min,mk,ml,mn,mr,ms,ms-Arab,mt,mwr,my,ndc-"
    "ZW,ne,new,nhe,nl,no,nr,nso,nus,ny,oc,om,or,os,pa,pa-Arab,pag,pam,pap,pl,"
    "ps,pt,pt-PT,qu,rn,ro,rom,ru,rw,sa,sah,sat-Latn,scn,sd,se,sg,si,sk,sl,sm,"
    "sn,so,sq,sr,ss,st,su,sus,sv,sw,szl,ta,tcy,te,tet,tg,th,ti,tiv,tk,tl,tn,to,"
    "tpi,tr,trp,ts,tt,tum,ty,tyv,udm,ug,uk,ur,uz,ve,vec,vi,war,wo,xh,yi,yo,yua,"
    "yue,zap,zh-CN,zh-TW,zu"};
// To get the supported translate languages, we combine the source with these
// additional languages.
constexpr base::FeatureParam<std::string> kSupportedTargetTranslateLanguages{
    &kLensOverlayTranslateLanguages, "supported-target-languages",
    "bal,ber,bm-Nkoo,din,dv,mni-Mtei,shn"};
constexpr base::FeatureParam<base::TimeDelta> kSupportedLanguagesCacheTimeoutMs{
    &kLensOverlayTranslateLanguages, "supported-languages-cache-timeout-ms",
    base::Days(30)};
constexpr base::FeatureParam<int> kRecentLanguagesAmount{
    &kLensOverlayTranslateLanguages, "recent-languages-amount", 5};

constexpr base::FeatureParam<int>
    kLensOverlaySimplifiedSelectionTextReceivedTimeout{
        &kLensOverlaySimplifiedSelection, "simplified-text-received-timeout",
        2000};
constexpr base::FeatureParam<int>
    kLensOverlaySimplifiedSelectionCopyTextReceivedTimeout{
        &kLensOverlaySimplifiedSelection, "copy-text-received-timeout", 500};
constexpr base::FeatureParam<int>
    kLensOverlaySimplifiedSelectionTranslateTextReceivedTimeout{
        &kLensOverlaySimplifiedSelection, "translate-text-received-timeout",
        500};
constexpr base::FeatureParam<bool>
    kLensOverlaySimplifiedSelectionShouldCopyAsImage{
        &kLensOverlaySimplifiedSelection, "copy-command-copies-as-image",
        false};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesEnableDynamicTheme{
        &kLensOverlayVisualSelectionUpdates, "enable-dynamic-theme", false};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesEnableBorderGlow{
        &kLensOverlayVisualSelectionUpdates, "enable-border-glow", true};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesEnableGradientRegionStroke{
        &kLensOverlayVisualSelectionUpdates, "enable-gradient-region-stroke",
        false};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesEnableWhiteRegionStroke{
        &kLensOverlayVisualSelectionUpdates, "enable-white-region-stroke",
        true};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesEnableRegionSelectedGlow{
        &kLensOverlayVisualSelectionUpdates, "enable-region-selected-glow",
        true};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesEnableGradientSuperG{
        &kLensOverlayVisualSelectionUpdates, "enable-gradient-super-g", true};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesCsbThumbnail{
        &kLensOverlayVisualSelectionUpdates, "enable-csb-thumbnail", true};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesEnableCsbMotionTweaks{
        &kLensOverlayVisualSelectionUpdates, "enable-csb-motion-tweaks", true};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesThumbnailSizingTweaks{
        &kLensOverlayVisualSelectionUpdates, "enable-thumbnail-sizing-tweaks",
        true};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesHideCsbEllipsis{
        &kLensOverlayVisualSelectionUpdates, "hide-csb-ellipsis", true};

constexpr base::FeatureParam<bool>
    kLensOverlayVisualSelectionUpdatesCloseButtonTweaks{
        &kLensOverlayVisualSelectionUpdates, "enable-close-button-tweaks",
        true};

constexpr base::FeatureParam<std::string> kHomepageURLForLens{
    &kLensStandalone, "lens-homepage-url", "https://lens.google.com/v3/"};

constexpr base::FeatureParam<bool> kEnableLensHtmlRedirectFix{
    &kLensStandalone, "lens-html-redirect-fix", false};

constexpr base::FeatureParam<bool> kShouldIssuePreconnectForLens{
    &kLensStandalone, "lens-issue-preconnect", true};

constexpr base::FeatureParam<std::string> kPreconnectKeyForLens{
    &kLensStandalone, "lens-preconnect-key", "https://google.com"};

constexpr base::FeatureParam<bool> kShouldIssueProcessPrewarmingForLens{
    &kLensStandalone, "lens-issue-process-prewarming", true};

constexpr base::FeatureParam<size_t> kLensOverlayChunkSizeBytes{
    &kLensOverlayUploadChunking, "chunk-size-bytes", 2 * 1024 * 1024};  // 2 MiB

constexpr base::FeatureParam<std::string> kLensOverlayUploadChunkEndpointUrl{
    &kLensOverlayUploadChunking, "upload-chunk-endpoint-url",
    "https://lensfrontend-pa.googleapis.com/v1/uploadChunk"};

constexpr base::FeatureParam<bool> kLensOverlayUploadChunkingUseDebugOptions{
    &kLensOverlayUploadChunking, "use-debug-options", false};

constexpr base::FeatureParam<int> kLensOverlayUploadChunkRequestTimeoutMs{
    &kLensOverlayUploadChunking, "upload-chunk-request-timeout-ms", 60000};

constexpr base::FeatureParam<int> kLensOverlaySliderChangedTimeout{
    &kLensOverlayCornerSliders, "slider-changed-timeout", 1000};

const base::FeatureParam<std::string> kLensOverlayEduUrlAllowFilters{
    &kLensOverlayEduActionChip, "url-allow-filters", "[]"};

const base::FeatureParam<std::string> kLensOverlayEduUrlBlockFilters{
    &kLensOverlayEduActionChip, "url-block-filters", "[]"};

const base::FeatureParam<std::string> kLensOverlayEduUrlPathMatchAllowFilters{
    &kLensOverlayEduActionChip, "url-path-match-allow-filters", "[]"};

const base::FeatureParam<std::string> kLensOverlayEduUrlPathMatchBlockFilters{
    &kLensOverlayEduActionChip, "url-path-match-block-filters", "[]"};

const base::FeatureParam<std::string> kLensOverlayEduHashedDomainBlockFilters{
    &kLensOverlayEduActionChip, "hashed-domain-block-filters", ""};

const base::FeatureParam<std::string>
    kLensOverlayEduUrlForceAllowedMatchPatterns{
        &kLensOverlayEduActionChip, "url-path-forced-allowed-match-patterns",
        "[]"};

const base::FeatureParam<bool> kLensOverlayEduActionChipDisabledByGlic{
    &kLensOverlayEduActionChip, "disabled-by-glic", true};

std::string GetHomepageURLForLens() {
  return kHomepageURLForLens.Get();
}

bool GetEnableLensHtmlRedirectFix() {
  return kEnableLensHtmlRedirectFix.Get();
}

bool GetShouldIssuePreconnectForLens() {
  return kShouldIssuePreconnectForLens.Get();
}

std::string GetPreconnectKeyForLens() {
  return kPreconnectKeyForLens.Get();
}

bool GetShouldIssueProcessPrewarmingForLens() {
  return kShouldIssueProcessPrewarmingForLens.Get();
}

bool IsLensOverlayEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlay);
}

std::string GetLensOverlayActivityURL() {
  return kActivityUrl.Get();
}

std::string GetLensOverlayHelpCenterURL() {
  return kHelpCenterUrl.Get();
}

int GetLensOverlayMinRamMb() {
  return kLensOverlayMinRamMb.Get();
}

std::string GetLensOverlayResultsSearchURL() {
  return kResultsSearchUrl.Get();
}

int GetLensOverlayImageCompressionQuality() {
  return kLensOverlayImageCompressionQuality.Get();
}

int GetLensOverlayScreenshotRenderQuality() {
  return kLensOverlayScreenshotRenderQuality.Get();
}

int GetLensOverlayImageMaxAreaTier1() {
  return kLensOverlayImageMaxAreaTier1.Get();
}

int GetLensOverlayImageMaxHeightTier1() {
  return kLensOverlayImageMaxHeightTier1.Get();
}

bool LensOverlayUseTieredDownscaling() {
  return kLensOverlayUseTieredDownscaling.Get();
}

bool GetLensOverlaySendLatencyGen204() {
  return kLensOverlaySendLatencyGen204.Get();
}

bool GetLensOverlaySendTaskCompletionGen204() {
  return kLensOverlaySendTaskCompletionGen204.Get();
}

bool GetLensOverlaySendSemanticEventGen204() {
  return kLensOverlaySendSemanticEventGen204.Get();
}

int GetLensOverlayImageMaxArea() {
  return kLensOverlayImageMaxArea.Get();
}

int GetLensOverlayImageMaxHeight() {
  return kLensOverlayImageMaxHeight.Get();
}

int GetLensOverlayImageMaxWidth() {
  return kLensOverlayImageMaxWidth.Get();
}

int GetLensOverlayImageMaxWidthTier1() {
  return kLensOverlayImageMaxWidthTier1.Get();
}

int GetLensOverlayImageMaxAreaTier2() {
  return kLensOverlayImageMaxAreaTier2.Get();
}

int GetLensOverlayImageMaxHeightTier2() {
  return kLensOverlayImageMaxHeightTier2.Get();
}

int GetLensOverlayImageMaxWidthTier2() {
  return kLensOverlayImageMaxWidthTier2.Get();
}

int GetLensOverlayImageMaxAreaTier3() {
  return kLensOverlayImageMaxAreaTier3.Get();
}

int GetLensOverlayImageMaxHeightTier3() {
  return kLensOverlayImageMaxHeightTier3.Get();
}

int GetLensOverlayImageMaxWidthTier3() {
  return kLensOverlayImageMaxWidthTier3.Get();
}

int GetLensOverlayImageDownscaleUiScalingFactorThreshold() {
  return kLensOverlayImageDownscaleUiScalingFactor.Get();
}

std::string GetLensOverlayEndpointURL() {
  return kLensOverlayEndpointUrl.Get();
}

bool IsLensOverlayDebuggingEnabled() {
  return kLensOverlayDebuggingMode.Get();
}

bool UseOauthForLensOverlayRequests() {
  return kUseOauthForLensOverlayRequests.Get();
}

int GetLensOverlayClusterInfoLifetimeSeconds() {
  return kLensOverlayClusterInfoLifetimeSeconds.Get();
}

bool UseVideoContextForTextOnlyLensOverlayRequests() {
  return kUseVideoContextForTextOnlyLensOverlayRequests.Get();
}

bool UseVideoContextForMultimodalLensOverlayRequests() {
  return kUseVideoContextForMultimodalLensOverlayRequests.Get();
}

std::string GetLensOverlayClusterInfoEndpointUrl() {
  return kLensOverlayClusterInfoEndpointUrl.Get();
}

bool GetLensOverlaySendLensInputsForContextualSuggest() {
  return kLensOverlaySendLensInputsForContextualSuggest.Get();
}

bool GetLensOverlaySendLensInputsForLensSuggest() {
  return kLensOverlaySendLensInputsForLensSuggest.Get();
}

bool GetLensOverlaySendLensVisualInteractionDataForLensSuggest() {
  return kLensOverlaySendLensVisualInteractionDataForLensSuggest.Get();
}

bool GetLensOverlaySendImageSignalsForLensSuggest() {
  return kLensOverlaySendImageSignalsForLensSuggest.Get();
}

uint32_t GetLensOverlayFileUploadLimitBytes() {
  size_t limit = kLensOverlayFileUploadLimitBytes.Get();
  return base::IsValueInRangeForNumericType<uint32_t>(limit)
             ? static_cast<uint32_t>(limit)
             : 0;
}

uint32_t GetLensOverlayPdfSuggestCharacterTarget() {
  size_t limit = kLensOverlayPdfTextCharacterLimit.Get();
  return base::IsValueInRangeForNumericType<uint32_t>(limit)
             ? static_cast<uint32_t>(limit)
             : 0;
}

bool UsePdfVitParam() {
  return kUsePdfVitParam.Get();
}

bool UseWebpageVitParam() {
  return kUseWebpageVitParam.Get();
}

bool UsePdfInteractionType() {
  return kUsePdfInteractionType.Get();
}

bool UseWebpageInteractionType() {
  return kUseWebpageInteractionType.Get();
}

int GetScannedPdfCharacterPerPageHeuristic() {
  return kScannedPdfCharacterPerPageHeuristic.Get();
}

bool UseUpdatedContextFields() {
  return kUseUpdatedContentFields.Get();
}

bool UsePdfsAsContext() {
  return kUsePdfsAsContext.Get();
}

bool UseInnerTextAsContext() {
  return kUseInnerTextAsContext.Get();
}

int GetLensOverlayPageContentRequestTimeoutMs() {
  return kLensOverlayPageContentRequestTimeoutMs.Get();
}

bool UseInnerHtmlAsContext() {
  return kUseInnerHtmlAsContext.Get();
}

bool SendClientContextToClusterInfoRequestForContextualSuggest() {
  return kSendClientContextToClusterInfoRequestForContextualSuggest.Get();
}

bool UseApcAsContext() {
  return kUseApcAsContext.Get();
}

bool SendPageUrlForContextualization() {
  return kSendPageUrlForContextualization.Get();
}

bool SendPageTitleForContextualization() {
  return kSendPageTitleForContextualization.Get();
}

int GetLensOverlayVerticalTextMargin() {
  return kLensOverlayVerticalTextMargin.Get();
}

int GetLensOverlayHorizontalTextMargin() {
  return kLensOverlayHorizontalTextMargin.Get();
}

bool IsLensOverlayShimmerEnabled() {
  return kLensOverlayEnableShimmer.Get();
}

bool IsLensOverlayShimmerSparklesEnabled() {
  return kLensOverlayEnableShimmerSparkles.Get();
}

bool IsLensOverlayGoogleDseRequired() {
  return kLensOverlayGoogleDseRequired.Get();
}

std::string GetLensOverlayResultsSearchLoadingURL(bool dark_mode) {
  return dark_mode ? kResultsSearchLoadingDarkModeUrl.Get()
                   : kResultsSearchLoadingUrl.Get();
}

int GetLensOverlayTapRegionHeight() {
  return kLensOverlayTapRegionHeight.Get();
}

int GetLensOverlayTapRegionWidth() {
  return kLensOverlayTapRegionWidth.Get();
}

bool UseLensOverlayForImageSearch() {
  return kUseLensOverlayForImageSearch.Get();
}

bool UseLensOverlayForVideoFrameSearch() {
  return kUseLensOverlayForVideoFrameSearch.Get();
}

bool IsOmniboxEntryPointEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayOmniboxEntryPoint) &&
         kIsOmniboxEntryPointEnabled.Get();
}

bool IsOmniboxEntrypointAlwaysVisible() {
  return kIsOmniboxEntrypointAlwaysVisible.Get();
}

bool UseBrowserDarkModeSettingForLensOverlay() {
  return kUseBrowserDarkModeSettingForLensOverlay.Get();
}

double DynamicThemeMinPopulationPct() {
  return kDynamicThemeMinPopulationPct.Get();
}

double DynamicThemeMinChroma() {
  return kDynamicThemeMinChroma.Get();
}

bool SendVisualSearchInteractionParamForLensTextQueries() {
  return kSendVisualSearchInteractionParamForLensTextQueries.Get();
}

double GetLensOverlaySelectTextOverRegionTriggerThreshold() {
  return kLensOverlaySelectTextOverRegionTriggerThreshold.Get();
}

int GetLensOverlaySignificantRegionMinArea() {
  return kLensOverlaySignificantRegionMinArea.Get();
}

int GetLensOverlayMaxSignificantRegions() {
  return kLensOverlayMaxSignificantRegions.Get();
}

double GetLensOverlayPostSelectionComparisonThreshold() {
  return kLensOverlayPostSelectionComparisonThreshold.Get();
}

bool GetLensOverlayUseBlur() {
  return kLensOverlayUseBlur.Get();
}

int GetLensOverlayCustomBlurBlurRadiusPixels() {
  return kLensOverlayCustomBlurBlurRadiusPixels.Get();
}

double GetLensOverlayCustomBlurQuality() {
  return kLensOverlayCustomBlurQuality.Get();
}

double GetLensOverlayCustomBlurRefreshRateHertz() {
  return kLensOverlayCustomBlurRefreshRateHertz.Get();
}

int GetLensOverlayServerRequestTimeout() {
  return kLensOverlayServerRequestTimeout.Get();
}

bool GetLensOverlayEnableErrorPage() {
  return kLensOverlayEnableErrorPage.Get();
}

std::string GetLensOverlayGscQueryParamValue() {
  return kLensOverlayGscQueryParamValue.Get();
}

bool GetLensOverlayEnableInFullscreen() {
  return kLensOverlayEnableInFullscreen.Get();
}

int GetLensOverlaySegmentationMaskCornerRadius() {
  return kLensOverlaySegmentationMaskCornerRadius.Get();
}

bool IsLensOverlayTranslateButtonEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayTranslateButton);
}

bool IsLensOverlayCopyAsImageEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayImageContextMenuActions) &&
         kLensOverlayImageContextMenuActionsEnableCopyAsImage.Get();
}

bool IsLensOverlaySaveAsImageEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayImageContextMenuActions) &&
         kLensOverlayImageContextMenuActionsEnableSaveAsImage.Get();
}

int GetLensOverlayImageContextMenuActionsTextReceivedTimeout() {
  return kLensOverlayImageContextMenuActionsTextReceivedTimeout.Get();
}

bool IsLensOverlayContextualSearchboxEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayContextualSearchbox) ||
         base::FeatureList::IsEnabled(
             kLensOverlayContextualSearchboxForOmniboxSuggestions);
}

bool IsLensOverlaySidePanelOpenInNewTabEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlaySidePanelOpenInNewTab);
}

bool IsLensOverlayClusterInfoOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayLatencyOptimizations) &&
         kEnableClusterInfoOptimization.Get();
}

bool IsLensOverlayEarlyInteractionOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayLatencyOptimizations) &&
         kEnableEarlyInteractionOptimization.Get();
}

bool IsLensOverlayEarlyStartQueryFlowOptimizationEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayLatencyOptimizations) &&
         kEnableEarlyStartQueryFlowOptimization.Get();
}

base::TimeDelta GetLensOverlaySurveyResultsTime() {
  return kLensOverlaySurveyResultsTime.Get();
}

bool IsLensOverlayTranslateLanguagesFetchEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayTranslateLanguages);
}

std::string GetLensOverlayTranslateEndpointURL() {
  return kTranslateEndpointUrl.Get();
}

bool EnableContextualSearchboxGhostLoader() {
  return kEnableContextualSearchboxGhostLoader.Get();
}

bool ShowContextualSearchboxGhostLoaderLoadingState() {
  return kShowContextualSearchboxGhostLoaderLoadingState.Get();
}

base::TimeDelta GetLensSearchboxAutocompleteTimeout() {
  return kLensSearchboxAutocompleteTimeout.Get();
}

std::string GetLensOverlayTranslateSourceLanguages() {
  return kSupportedSourceTranslateLanguages.Get();
}

std::string GetLensOverlayTranslateTargetLanguages() {
  return kSupportedTargetTranslateLanguages.Get();
}

base::TimeDelta GetLensOverlaySupportedLanguagesCacheTimeoutMs() {
  return kSupportedLanguagesCacheTimeoutMs.Get();
}

bool ShowContextualSearchboxSearchSuggest() {
  return kShowContextualSearchboxSearchSuggest.Get();
}

int GetLensOverlayTranslateRecentLanguagesAmount() {
  return kRecentLanguagesAmount.Get();
}

bool IsLensOverlayRoutingInfoEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayRoutingInfo);
}

bool HandleSidePanelTextDirectivesEnabled() {
  return kHandleSidePanelTextDirectives.Get();
}

bool ShouldHoldContextualQueriesUntilAck() {
  return kHoldContextualQueriesUntilAck.Get();
}

bool ShouldZstdCompressPdfBytes() {
  return kZstdCompressPdfBytes.Get();
}

int GetZstdCompressionLevel() {
  return kZstdCompressionLevel.Get();
}

bool ShouldShowUploadProgressBar() {
  return kShowUploadProgressBar.Get();
}

double GetUploadProgressBarShowHeuristic() {
  return kUploadProgressBarShowHeuristic.Get();
}

bool ShouldAutoFocusSearchbox() {
  if (base::FeatureList::IsEnabled(
          kLensOverlayContextualSearchboxForOmniboxSuggestions)) {
    return kAutoFocusSearchboxForOmniboxSuggestions.Get();
  }
  return kAutoFocusSearchbox.Get();
}

bool IsSimplifiedSelectionEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlaySimplifiedSelection);
}

int GetSimplifiedSelectionTextReceivedTimeout() {
  return kLensOverlaySimplifiedSelectionTextReceivedTimeout.Get();
}

int GetCopyTextReceivedTimeout() {
  return kLensOverlaySimplifiedSelectionCopyTextReceivedTimeout.Get();
}

int GetTranslateTextReceivedTimeout() {
  return kLensOverlaySimplifiedSelectionTranslateTextReceivedTimeout.Get();
}

bool GetShouldCopyAsImage() {
  return kLensOverlaySimplifiedSelectionShouldCopyAsImage.Get();
}

bool IsLensOverlayVisualSelectionUpdatesEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayVisualSelectionUpdates) ||
         base::FeatureList::IsEnabled(
             kLensOverlayVisualSelectionUpdatesForOmniboxSuggestions);
}

bool IsDynamicThemeDetectionEnabled() {
  if (IsLensOverlayVisualSelectionUpdatesEnabled()) {
    return kLensOverlayVisualSelectionUpdatesEnableDynamicTheme.Get();
  }
  return kDynamicThemeForLensOverlay.Get();
}

bool GetVisualSelectionUpdatesEnableBorderGlow() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesEnableBorderGlow.Get();
}

bool GetVisualSelectionUpdatesEnableGradientRegionStroke() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesEnableGradientRegionStroke.Get();
}

bool GetVisualSelectionUpdatesEnableWhiteRegionStroke() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesEnableWhiteRegionStroke.Get();
}

bool GetVisualSelectionUpdatesEnableRegionSelectedGlow() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesEnableRegionSelectedGlow.Get();
}

bool GetVisualSelectionUpdatesEnableGradientSuperG() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesEnableGradientSuperG.Get();
}

bool GetVisualSelectionUpdatesEnableCsbThumbnail() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesCsbThumbnail.Get();
}

bool GetVisualSelectionUpdatesEnableCsbMotionTweaks() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesEnableCsbMotionTweaks.Get();
}

bool GetVisualSelectionUpdatesEnableThumbnailSizingTweaks() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesThumbnailSizingTweaks.Get();
}

bool GetVisualSelectionUpdatesHideCsbEllipsis() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesHideCsbEllipsis.Get();
}

bool GetVisualSelectionUpdatesEnableCloseButtonTweaks() {
  return IsLensOverlayVisualSelectionUpdatesEnabled() &&
         kLensOverlayVisualSelectionUpdatesCloseButtonTweaks.Get();
}

bool PageContentUploadRequestIdFixEnabled() {
  return kPageContentUploadRequestIdFix.Get();
}

bool UpdateViewportEachQueryEnabled() {
  return kUpdateViewportEachQuery.Get();
}

bool SendPdfCurrentPageEnabled() {
  return kSendPdfCurrentPage.Get();
}

bool ShowContextualSearchboxZeroPrefixSuggest() {
  return kShowContextualSearchboxZeroPrefixSuggest.Get();
}

bool IsUpdatedClientContextEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayUpdatedClientContext);
}

bool ShouldShowMGTInSidePanel() {
  return base::FeatureList::IsEnabled(kLensOverlayMGTInSidePanel);
}

bool ShouldUseAltLoadingHintWeb() {
  return kUseAltLoadingHintWeb.Get();
}

bool ShouldUseAltLoadingHintPdf() {
  return kUseAltLoadingHintPdf.Get();
}

bool ShouldEnableSummarizeHintForContextualSuggest() {
  return kLensOverlayEnableSummarizeHintForContextualSuggest.Get();
}

bool IsLensOverlayUploadChunkingEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayUploadChunking);
}

uint32_t GetLensOverlayChunkSizeBytes() {
  size_t limit = kLensOverlayChunkSizeBytes.Get();
  return base::IsValueInRangeForNumericType<uint32_t>(limit)
             ? static_cast<uint32_t>(limit)
             : static_cast<uint32_t>(kLensOverlayChunkSizeBytes.default_value);
}

std::string GetLensOverlayUploadChunkEndpointURL() {
  return kLensOverlayUploadChunkEndpointUrl.Get();
}

bool IsLensOverlayUploadChunkingUseDebugOptionsEnabled() {
  return kLensOverlayUploadChunkingUseDebugOptions.Get();
}

int GetLensOverlayUploadChunkRequestTimeoutMs() {
  return kLensOverlayUploadChunkRequestTimeoutMs.Get();
}

bool IsLensSearchSidePanelNewFeedbackEnabled() {
  return base::FeatureList::IsEnabled(kLensSearchSidePanelNewFeedback);
}

bool ShouldLensOverlayRecontextualizeOnQuery() {
  return base::FeatureList::IsEnabled(kLensOverlayRecontextualizeOnQuery);
}

bool AreLensOverlayCornerSlidersEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayCornerSliders);
}

int GetLensOverlaySliderChangedTimeout() {
  return kLensOverlaySliderChangedTimeout.Get();
}

bool IsLensSearchProtectedPageEnabled() {
  return base::FeatureList::IsEnabled(kLensSearchProtectedPage);
}

bool IsLensSearchSidePanelScrollToAPIEnabled() {
  return base::FeatureList::IsEnabled(kLensSearchSidePanelScrollToAPI);
}

bool IsLensOverlayEduActionChipEnabled() {
  return base::FeatureList::IsEnabled(kLensOverlayEduActionChip);
}

std::string GetLensOverlayEduUrlAllowFilters() {
  return kLensOverlayEduUrlAllowFilters.Get();
}

std::string GetLensOverlayEduUrlBlockFilters() {
  return kLensOverlayEduUrlBlockFilters.Get();
}

std::string GetLensOverlayEduUrlPathMatchAllowFilters() {
  return kLensOverlayEduUrlPathMatchAllowFilters.Get();
}

std::string GetLensOverlayEduUrlPathMatchBlockFilters() {
  return kLensOverlayEduUrlPathMatchBlockFilters.Get();
}

std::string GetLensOverlayEduUrlForceAllowedMatchPatterns() {
  return kLensOverlayEduUrlForceAllowedMatchPatterns.Get();
}

std::string GetLensOverlayEduHashedDomainBlockFilters() {
  return kLensOverlayEduHashedDomainBlockFilters.Get();
}

bool IsLensOverlayEduActionChipDisabledByGlic() {
  return kLensOverlayEduActionChipDisabledByGlic.Get();
}

}  // namespace lens::features
