// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace base {
class TimeDelta;
}  // namespace base

// The set of parameters customizing the HUP scoring.
struct HUPScoringParams {
  // A set of parameters describing how to cap a given count score.  First,
  // we apply a half-life based decay of the given count and then find the
  // maximum relevance score based on the decay factor or counts specified
  // in the corresponding bucket list. See comment on |buckets_| for details.
  class ScoreBuckets {
   public:
    // Stores the max relevance at each count/decay factor threshold.
    typedef std::pair<double, int> CountMaxRelevance;

    ScoreBuckets();
    ScoreBuckets(const ScoreBuckets& other);
    ~ScoreBuckets();

    // Computes a half-life time decay given the |elapsed_time|.
    double HalfLifeTimeDecay(const base::TimeDelta& elapsed_time) const;

    int relevance_cap() const { return relevance_cap_; }
    void set_relevance_cap(int relevance_cap) {
      relevance_cap_ = relevance_cap;
    }

    int half_life_days() const { return half_life_days_; }
    void set_half_life_days(int half_life_days) {
      half_life_days_ = half_life_days;
    }

    bool use_decay_factor() const { return use_decay_factor_; }
    void set_use_decay_factor(bool use_decay_factor) {
      use_decay_factor_ = use_decay_factor;
    }

    std::vector<CountMaxRelevance>& buckets() { return buckets_; }
    const std::vector<CountMaxRelevance>& buckets() const { return buckets_; }

    // Estimates dynamic memory usage.
    // See base/trace_event/memory_usage_estimator.h for more info.
    size_t EstimateMemoryUsage() const;

   private:
    // History matches with relevance score greater or equal to |relevance_cap_|
    // are not affected by this experiment.
    // Set to -1, if there is no relevance cap in place and all matches are
    // subject to demotion.
    int relevance_cap_;

    // Half life time for a decayed count as measured since the last visit.
    // Set to -1 if not used.
    int half_life_days_;

    // The relevance score caps at successively decreasing threshold values.
    // The thresholds are either decayed counts or decay factors, depending on
    // the value of |use_decay_factor_|.
    //
    // Consider this example specifying the decayed counts:
    //   [(1, 1000), (0.5, 500), (0, 100)]
    // If decayed count is 2 (which is >= 1), the corresponding match's maximum
    // relevance will be capped at 1000.  In case of 0.5, the score is capped
    // at 500.  Anything below 0.5 is capped at 100.
    //
    // This list is sorted by the pair's first element in descending order.
    std::vector<CountMaxRelevance> buckets_;

    // True when the bucket thresholds are decay factors rather than counts.
    bool use_decay_factor_;
  };

  HUPScoringParams() {}

  // Estimates dynamic memory usage.
  // See base/trace_event/memory_usage_estimator.h for more info.
  size_t EstimateMemoryUsage() const;

  ScoreBuckets typed_count_buckets;

  // Used only when the typed count is 0.
  ScoreBuckets visited_count_buckets;
};

namespace OmniboxFieldTrial {

// A mapping that contains multipliers indicating that matches of the
// specified type should have their relevance score multiplied by the
// given number.  Omitted types are assumed to have multipliers of 1.0.
typedef std::map<AutocompleteMatchType::Type, float> DemotionMultipliers;

// A vector that maps from the number of matching pages to the document
// specificity score used in HistoryQuick provider / ScoredHistoryMatch
// scoring. The vector is sorted by the size_t (the number of matching pages).
// If an entry is omitted, the appropriate value is assumed to be the one in
// the later bucket.  For example, with a vector containing {{1, 2.0},
// {3, 1.5}}, the score for 2 is inferred to be 1.5.  Values beyond the
// end of the vector are assumed to have scores of 1.0.
typedef std::vector<std::pair<size_t, double>> NumMatchesScores;

// ---------------------------------------------------------
// For any experiment that's part of the bundled omnibox field trial.

// Returns a bitmap containing AutocompleteProvider::Type values
// that should be disabled in AutocompleteController.
int GetDisabledProviderTypes();

// Returns whether the user is in any dynamic field trial where the
// group has a the prefix |group_prefix|.
bool HasDynamicFieldTrialGroupPrefix(const char* group_prefix);

// ---------------------------------------------------------
// For the suggest field trial.

// Populates |field_trial_hash| with hashes of the active suggest field trial
// names, if any.
void GetActiveSuggestFieldTrialHashes(std::vector<uint32_t>* field_trial_hash);

// ---------------------------------------------------------
// For the SearchHistory experiment that's part of the bundled omnibox
// field trial.

// ---------------------------------------------------------
// For the DemoteByType experiment that's part of the bundled omnibox field
// trial.

// If the user is in an experiment group that, in the provided
// |current_page_classification| context, demotes the relevance scores
// of certain types of matches, populates the |demotions_by_type| map
// appropriately.  Otherwise, sets |demotions_by_type| to its default
// value based on the context.
void GetDemotionsByType(
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    DemotionMultipliers* demotions_by_type);

// ---------------------------------------------------------
// For experiments related to the number of suggestions shown.

// If the user is in an experiment group that specifies the max results for a
// particular provider, returns the limit. Otherwise returns the default limit.
size_t GetProviderMaxMatches(AutocompleteProvider::Type provider);

// Returns whether the feature to limit the number of shown URL matches
// is enabled.
bool IsMaxURLMatchesFeatureEnabled();

// Returns the maximum number of URL matches that should be allowed within
// the Omnibox if there are search-type matches available to replace them.
// If the capping feature is not enabled, or the parameter cannot be
// parsed, it returns 0.
size_t GetMaxURLMatches();

// ---------------------------------------------------------
// For the HistoryURL provider new scoring experiment that is part of the
// bundled omnibox field trial.

// Initializes the HUP |scoring_params| based on the active HUP scoring
// experiment.
void GetDefaultHUPScoringParams(HUPScoringParams* scoring_params);
void GetExperimentalHUPScoringParams(HUPScoringParams* scoring_params);

// ---------------------------------------------------------
// For the HQPBookmarkValue experiment that's part of the
// bundled omnibox field trial.

// Returns the value an untyped visit to a bookmark should receive.
// Compare this value with the default of 1 for non-bookmarked untyped
// visits to pages and the default of 20 for typed visits.  Returns
// 10 if the bookmark value experiment isn't active.
float HQPBookmarkValue();

// ---------------------------------------------------------
// For the HQPAllowMatchInTLD experiment that's part of the
// bundled omnibox field trial.

// Returns true if HQP should allow an input term to match in the
// top level domain (e.g., .com) of a URL.  Returns false if the
// allow match in TLD experiment isn't active.
bool HQPAllowMatchInTLDValue();

// ---------------------------------------------------------
// For the HQPAllowMatchInScheme experiment that's part of the
// bundled omnibox field trial.

// Returns true if HQP should allow an input term to match in the
// scheme (e.g., http://) of a URL.  Returns false if the allow
// match in scheme experiment isn't active.
bool HQPAllowMatchInSchemeValue();

// ---------------------------------------------------------
// For SearchProvider related experiments.

// Returns how the search provider should poll Suggest. Currently, we support
// measuring polling delay from the last keystroke or last suggest request.
void GetSuggestPollingStrategy(bool* from_last_keystroke,
                               int* polling_delay_ms);

// ---------------------------------------------------------
// For HQP scoring related experiments to control the topicality and scoring
// ranges of relevancy scores.

// Returns the scoring buckets for HQP experiments. Returns an empty string
// if scoring buckets are not specified in the field trial. Scoring buckets
// are stored in string form giving mapping from (topicality_score,
// frequency_score) to final relevance score. Please see GetRelevancyScore()
// under chrome/browser/history::ScoredHistoryMatch for details.
std::string HQPExperimentalScoringBuckets();

// Returns the topicality threshold for HQP experiments. Returns a default
// value of 0.5 if no threshold is specified in the field trial.
float HQPExperimentalTopicalityThreshold();

// ---------------------------------------------------------
// For experiment to limit HQP url indexing that's part of the bundled
// omnibox field trial.

// Returns the maximum number of history urls to index for HQP at the startup.
// Note: this limit is only applied at startup and more urls can be indexed
// during the session. Returns -1 if limit is not set by trials.
int MaxNumHQPUrlsIndexedAtStartup();

// ---------------------------------------------------------
// For the HQPFixFrequencyScoring experiment that's part of the
// bundled omnibox field trial.

// Returns the number of visits HQP should use when computing frequency
// scores.  Returns 10 if the experiment isn't active.
size_t HQPMaxVisitsToScore();

// Returns the score that should be given to typed transitions.  (The score
// of non-typed transitions is 1.)  Returns 1.5 if the experiment isn't
// active.
float HQPTypedValue();

// Returns NumMatchesScores; see comment by the declaration of it.
// If the experiment isn't active, returns an NumMatchesScores of
// {{1, 3}, {2, 2.5}, {3, 2}, {4, 1.5}}.
NumMatchesScores HQPNumMatchesScores();

// ---------------------------------------------------------
// For the HQPNumTitleWords experiment that's part of the
// bundled omnibox field trial.

// Returns the number of title words that are allowed to contribute
// to the topicality score.  Words later in the title are ignored.
// Returns 20 as a default if the experiment isn't active.
size_t HQPNumTitleWordsToAllow();

// ---------------------------------------------------------
// For the replace HUP experiment that's part of the bundled omnibox field
// trial.

// Returns whether HistoryQuick provider (HQP) should attempt to score
// suggestions also with a HistoryURL-provider-like (HUP-like) mode, and
// assign suggestions the max of this score and the normal score.
// Returns false if the experiment isn't active.
bool HQPAlsoDoHUPLikeScoring();

// Returns whether HistoryURL provider (HUP) should search its database for
// URLs and suggest them.  If false, HistoryURL provider merely creates the
// URL-what-you-typed match when appropriate.  Return true if the experiment
// isn't active.
bool HUPSearchDatabase();

// ---------------------------------------------------------
// For the aggressive keyword matching experiment that's part of the bundled
// omnibox field trial.

// One function is missing from here to avoid a cyclic dependency
// between search_engine and omnibox. In the search_engine component
// there is a OmniboxFieldTrialKeywordRequiresRegistry function
// that logically should be here.
//
// It returns whether KeywordProvider should consider the registry portion
// (e.g., co.uk) of keywords that look like hostnames as an important part of
// the keyword name for matching purposes.  Returns true if the experiment
// isn't active.

// Returns the relevance score that KeywordProvider should assign to keywords
// with sufficiently-complete matches, i.e., the user has typed all of the
// important part of the keyword.  Returns -1 if the experiment isn't active.
int KeywordScoreForSufficientlyCompleteMatch();

// ---------------------------------------------------------
// For UI experiments.

// Returns true if the fuzzy URL suggestions feature is enabled.
bool IsFuzzyUrlSuggestionsEnabled();
// Indicates whether fuzzy match behavior is counterfactual.
extern const base::FeatureParam<bool> kFuzzyUrlSuggestionsCounterfactual;
// Indicates whether to bypass fuzzy processing when `IsLowEndDevice` is true.
extern const base::FeatureParam<bool> kFuzzyUrlSuggestionsLowEndBypass;
// Indicates whether to support transpose edit operations in fuzzy search.
extern const base::FeatureParam<bool> kFuzzyUrlSuggestionsTranspose;
// The minimum length of input that will be checked for fuzzy URL
// suggestions. Note, this is a threshold independent of the fuzzy
// search tolerance schedule.
extern const base::FeatureParam<int> kFuzzyUrlSuggestionsMinInputLength;

// The *Penalty* parameters below provide control over how fuzzy matches
// are penalized according to input length. The relevance from the source
// match is reduced because the match is inexact (typo-corrected) but
// when the input is very short, such corrections are more likely and
// therefore confidence is lower. Hence the high penalty is applied
// to the shortest inputs and the low is applied to longer inputs, with
// a linear taper for input length to smooth the effect.

// Highest penalty percentage that will be applied to fuzzy URL suggestions.
extern const base::FeatureParam<int> kFuzzyUrlSuggestionsPenaltyHigh;
// Lowest penalty percentage that will be applied to fuzzy URL suggestions.
extern const base::FeatureParam<int> kFuzzyUrlSuggestionsPenaltyLow;
// The number of input characters between where the high penalty will be
// applied and where the low penalty will be applied.
extern const base::FeatureParam<int> kFuzzyUrlSuggestionsPenaltyTaperLength;

// Simply a convenient wrapper for testing a flag. Used downstream for an
// assortment of keyword mode experiments.
bool IsExperimentalKeywordModeEnabled();

// On Device Suggestions feature and its helper functions.
// TODO(crbug.com/1307005): clean up head suggest flags once crbug.com/1307005
// no longer happens.
bool IsOnDeviceHeadSuggestEnabledForIncognito();
bool IsOnDeviceHeadSuggestEnabledForNonIncognito();
bool IsOnDeviceHeadSuggestEnabledForAnyMode();
bool IsOnDeviceTailSuggestEnabled();
bool ShouldEncodeLeadingSpaceForOnDeviceTailSuggest();
// Functions can be used in both non-incognito and incognito.
std::string OnDeviceHeadModelLocaleConstraint(bool is_incognito);

// Returns true if CGI parameter names should not be considered when scoring
// suggestions.
bool ShouldDisableCGIParamMatching();

// If true, enables a "starter pack" of @history, @bookmarks, and @settings
// scopes for Site Search.
bool IsSiteSearchStarterPackEnabled();

// Omnibox UI Simplification - Square icon backgrounds.
// Blue rounded rect background icons for answers e.g. '1+1' and 'define x'.
// Does not apply to weather answers. Also updates the shade of blue and the
// stroke color.
extern const base::FeatureParam<bool> kSquareSuggestIconAnswers;
// Gray rounded rect background for search loop and nav fav icons.
extern const base::FeatureParam<bool> kSquareSuggestIconIcons;
// Gray rounded rect background for entities.
extern const base::FeatureParam<bool> kSquareSuggestIconEntities;
// The entity size relative to the background. 0.5 means the entity
// takes up half of the space. Should be (0, 1). No effect if
// `kSquareSuggestIconEntities` is false or this is 1.
extern const base::FeatureParam<double> kSquareSuggestIconEntitiesScale;

// Omnibox UI simplification - uniform row heights.
// Returns true if the feature to enable uniform row height is enabled.
bool IsUniformRowHeightEnabled();
// Specifies the row height in pixels for omnibox suggestions.
extern const base::FeatureParam<int> kSuggestionRowHeight;
// Specifies the vertical margin to use in one-line rich entity and answer
// suggestions.
extern const base::FeatureParam<int> kRichSuggestionVerticalMargin;
// Omnibox GM3 - icons.
// Returns true if the feature to enable GM3 icons is enabled.
bool IsChromeRefreshIconsEnabled();
// Omnibox GM3 - text style.
// Returns true if the feature to enable GM3 text styling is enabled.
bool IsGM3TextStyleEnabled();
// Specifies the omnibox font size (Touch UI).
extern const base::FeatureParam<int> kFontSizeTouchUI;
// Specifies the omnibox font size (non-Touch UI).
extern const base::FeatureParam<int> kFontSizeNonTouchUI;

// ---------------------------------------------------------
// Clipboard URL suggestions:

// The parameter "ClipboardURLMaximumAge" doesn't live in this file; instead
// it lives in
// components/open_from_clipboard/clipboard_recent_content.cc.
// Please see ClipboardRecentContent::MaximumAgeOfClipboard() for the usage
// of it.  The parameter cannot live here because that component cannot
// include this component, else there would be a circular dependency.

// ---------------------------------------------------------
// Exposed publicly for the sake of unittests.
extern const char kBundledExperimentFieldTrialName[];
// Rule names used by the bundled experiment.
extern const char kDisableProvidersRule[];
extern const char kSearchHistoryRule[];
extern const char kDemoteByTypeRule[];
extern const char kHQPBookmarkValueRule[];
extern const char kHQPTypedValueRule[];
extern const char kHQPAllowMatchInTLDRule[];
extern const char kHQPAllowMatchInSchemeRule[];
extern const char kMeasureSuggestPollingDelayFromLastKeystrokeRule[];
extern const char kSuggestPollingDelayMsRule[];
extern const char kHQPMaxVisitsToScoreRule[];
extern const char kHQPNumMatchesScoresRule[];
extern const char kHQPNumTitleWordsRule[];
extern const char kHQPAlsoDoHUPLikeScoringRule[];
extern const char kHUPSearchDatabaseRule[];
extern const char kPreventUWYTDefaultForNonURLInputsRule[];
// kKeywordRequiresRegistryRule seemingly has no production uses, but the string
// is actually used within TemplateURLService as a string to break a circular
// omnibox -> search_engines -> omnibox dependency.
extern const char kKeywordRequiresRegistryRule[];
extern const char kKeywordScoreForSufficientlyCompleteMatchRule[];
extern const char kHQPAllowDupMatchesForScoringRule[];
extern const char kEmphasizeTitlesRule[];

// Parameter names used by the HUP new scoring experiments.
extern const char kHUPNewScoringTypedCountRelevanceCapParam[];
extern const char kHUPNewScoringTypedCountHalfLifeTimeParam[];
extern const char kHUPNewScoringTypedCountScoreBucketsParam[];
extern const char kHUPNewScoringTypedCountUseDecayFactorParam[];
extern const char kHUPNewScoringVisitedCountRelevanceCapParam[];
extern const char kHUPNewScoringVisitedCountHalfLifeTimeParam[];
extern const char kHUPNewScoringVisitedCountScoreBucketsParam[];
extern const char kHUPNewScoringVisitedCountUseDecayFactorParam[];

// Parameter names used by the HQP experimental scoring experiments.
extern const char kHQPExperimentalScoringBucketsParam[];
extern const char kHQPExperimentalScoringTopicalityThresholdParam[];

// Parameter names used by the experiment that limits the number of history
// urls indexed for suggestions.
extern const char kMaxNumHQPUrlsIndexedAtStartupOnLowEndDevicesParam[];
extern const char kMaxNumHQPUrlsIndexedAtStartupOnNonLowEndDevicesParam[];

// Parameter names used by num suggestion experiments.
extern const char kMaxZeroSuggestMatchesParam[];
extern const char kOmniboxMaxURLMatchesParam[];
extern const char kUIMaxAutocompleteMatchesByProviderParam[];
extern const char kUIMaxAutocompleteMatchesParam[];
// The URL cutoff and increased limit for dynamic max autocompletion.
// - When dynamic max autocompletion is disabled, the omnibox allows
//   UIMaxAutocompleteMatches suggestions.
// - When dynamic max autocompletion is enabled, the omnibox allows
//   suggestions up to the increased limit if doing so has URL cutoff or less
//   URL suggestions.
// E.g. a UIMaxAutocompleteMatches of 8, URL cutoff of 2, and increased limit of
// 10 translates to "show 10 or 9 suggestions if doing so includes at most 2
// URLs; otherwise show 8 suggestions.
extern const char kDynamicMaxAutocompleteUrlCutoffParam[];
extern const char kDynamicMaxAutocompleteIncreasedLimitParam[];

// Parameter names used by on device head provider.
// These four parameters are shared by both non-incognito and incognito.
extern const char kOnDeviceHeadModelLocaleConstraint[];

// The amount of time to wait before sending a new suggest request after the
// previous one unless overridden by a field trial parameter.
// Non-const because some unittests modify this value.
extern int kDefaultMinimumTimeBetweenSuggestQueriesMs;

// Parameter names used by omnibox experiments that hide the path (and
// optionally subdomains) in the steady state.
extern const char kOmniboxUIUnelideURLOnHoverThresholdMsParam[];

// `FeatureParam`s

// Autocomplete stability and related features.
// If enabled and the input is in keyword mode, the default suggestion isn't
// preserved.
extern const base::FeatureParam<bool>
    kAutocompleteStabilityPreserveDefaultExcludeKeywordInputs;
// When providers update their matches, the aggregated matches for the current
// input are sorted, then merged with the matches from the previous input
// (`TransferOldMatches()`), then resorted. If enabled, both sorts preserve the
// default suggestion. Otherwise, only the first sort of the pre-merged matches
// preserves the default.
extern const base::FeatureParam<bool>
    kAutocompleteStabilityPreserveDefaultAfterTransfer;
// The minimum input length for which to preserve the default suggestion during
// sync updates. If 0, all sync updates preserve the default suggestion,
// regardless of input length. If <0, no sync updates preserve the default
// suggestion; i.e. control behavior.
extern const base::FeatureParam<int>
    kAutocompleteStabilityPreserveDefaultForSyncUpdatesMinInputLength;
// Whether to preserve the default suggestion during async updates. It doesn't
// make too much sense to enable preservation for sync but not async
// updates. True by default.
extern const base::FeatureParam<bool>
    kAutocompleteStabilityPreserveDefaultForAsyncUpdates;
// If enabled, transferred matches from the previous input are not allowed to
// be default.
extern const base::FeatureParam<bool>
    kAutocompleteStabilityPreventDefaultPreviousMatches;
// Limit how frequently `AutocompleteController::UpdateResult()` will be
// invoked. See the comments at `AutocompleteController::update_debouncer_`.
extern const base::FeatureParam<bool>
    kAutocompleteStabilityUpdateResultDebounceFromLastRun;
// See `kAutocompleteStabilityUpdateResultDebounceFromLastRun`. No debouncing
// if set to 0.
extern const base::FeatureParam<int>
    kAutocompleteStabilityUpdateResultDebounceDelay;

// Local history zero-prefix (aka zero-suggest) and prefix suggestions.

// Determines the maximum number of entries stored by the in-memory ZPS cache.
extern const base::FeatureParam<int> kZeroSuggestCacheMaxSize;

// Determines the relevance score for the local history zero-prefix suggestions.
extern const base::FeatureParam<int> kLocalHistoryZeroSuggestRelevanceScore;

// Returns true if any of the zero-suggest prefetching features are enabled.
bool IsZeroSuggestPrefetchingEnabled();

// Returns whether zero-suggest prefetching is enabled in the given context.
bool IsZeroSuggestPrefetchingEnabledInContext(
    metrics::OmniboxEventProto::PageClassification page_classification);

// Short bookmarks.
// Determine whether bookmarks should look for exact matches only or prefix
// matches as well when the input is short.
bool IsShortBookmarkSuggestionsEnabled();
bool IsShortBookmarkSuggestionsByTotalInputLengthEnabled();
// Returns the minimum input length to enable prefix matches.
size_t ShortBookmarkSuggestionsByTotalInputLengthThreshold();
// If true, when applicable, the feature will be logged as triggered but won't
// affect omnibox results.
extern const base::FeatureParam<bool>
    kShortBookmarkSuggestionsByTotalInputLengthCounterfactual;
extern const base::FeatureParam<int>
    kShortBookmarkSuggestionsByTotalInputLengthThreshold;

// Shortcut Expanding.
bool IsShortcutExpandingEnabled();

// Shortcut boost
// The scores to use for boosting search and URL suggestions respectively.
// Default to 1414 (`kScoreForBestInlineableResult` + 1). Setting to 0 will
// prevent boosting.
extern const base::FeatureParam<int> kShortcutBoostSearchScore;
extern const base::FeatureParam<int> kShortcutBoostUrlScore;
extern const base::FeatureParam<bool> kShortcutBoostCounterfactual;

// Rich autocompletion.
bool IsRichAutocompletionEnabled();
bool RichAutocompletionShowAdditionalText();
extern const base::FeatureParam<bool> kRichAutocompletionAutocompleteTitles;
extern const base::FeatureParam<bool>
    kRichAutocompletionAutocompleteTitlesShortcutProvider;
extern const base::FeatureParam<int>
    kRichAutocompletionAutocompleteTitlesMinChar;
extern const base::FeatureParam<bool>
    kRichAutocompletionAutocompleteNonPrefixAll;
extern const base::FeatureParam<bool>
    kRichAutocompletionAutocompleteNonPrefixShortcutProvider;
extern const base::FeatureParam<int>
    kRichAutocompletionAutocompleteNonPrefixMinChar;
extern const base::FeatureParam<bool> kRichAutocompletionShowAdditionalText;
extern const base::FeatureParam<bool>
    kRichAutocompletionAdditionalTextWithParenthesis;
extern const base::FeatureParam<bool>
    kRichAutocompletionAutocompleteShortcutText;
extern const base::FeatureParam<int>
    kRichAutocompletionAutocompleteShortcutTextMinChar;
extern const base::FeatureParam<bool> kRichAutocompletionCounterfactual;
extern const base::FeatureParam<bool>
    kRichAutocompletionAutocompletePreferUrlsOverPrefixes;

// Specifies the relevance scores for the Site Search Starter Pack ACMatches
// (e.g. @bookmarks, @history) provided by the Builtin Provider.
extern const base::FeatureParam<int> kSiteSearchStarterPackRelevanceScore;

// Document provider.
// The max number of low quality doc suggestions to show. Docs are considered
// low quality if they're neither owned nor fully contain the input in their
// titles and owners.
extern const base::FeatureParam<int> kDocumentProviderMaxLowQualitySuggestions;

// Domain suggestions.
// Whether enabled for counterfactual logging; i.e. shouldn't use domain
// suggestions/scores.
extern const base::FeatureParam<bool> kDomainSuggestionsCounterfactual;
// The minimum number of unique URLs a domain needs to be considered highly
// visited.
extern const base::FeatureParam<int> kDomainSuggestionsTypedUrlsThreshold;
// The minimum number of typed visits a URL needs to count for
// `kDomainSuggestionsTypedUrlsThreshold`
extern const base::FeatureParam<int> kDomainSuggestionsTypedUrlsOffset;
// The minimum number of typed visits a domain needs to be considered highly
// visited.
extern const base::FeatureParam<int> kDomainSuggestionsTypedVisitThreshold;
// The value to subtract from each URL's typed visits before contributing to
// `kDomainSuggestionsTypedVisitThreshold`.
extern const base::FeatureParam<int> kDomainSuggestionsTypedVisitOffset;
// The max each visit can contribute to `kDomainSuggestionsTypedVisitThreshold`.
// E.g. if 2, 'google.com/x' is typed-visited 5 times, and 'google.com/y' is
// typed visited 1 time, then 'google.com' will be scored min(5,2) + min(1,2) =
// 3, rather than 5+1 = 6.
extern const base::FeatureParam<int> kDomainSuggestionsTypedVisitCapPerVisit;
// The input inclusive minimum length to trigger domain suggestions.
extern const base::FeatureParam<int> kDomainSuggestionsMinInputLength;
// The maximum number of matches per domain to suggest.
extern const base::FeatureParam<int> kDomainSuggestionsMaxMatchesPerDomain;
// The scoring factor used to boost HQP suggestions from highly visited domains.
// A value of 1 is the control behavior. A value of 2 will boost scores, but not
// necessarily double them due to how HQP maps the factors to actual scores.
extern const base::FeatureParam<double> kDomainSuggestionsScoreFactor;
// Whether to use an alternative scoring algorithm based on last visit time to
// boost scores (e.g., 1000 - 80 / day). If disabled, domain suggestions use
// traditional HQP scoring (optionally scaled by
// `kDomainSuggestionsScoreFactor`). If enabled, they use the max of the
// traditional and the alternate scoring algorithms.
extern const base::FeatureParam<bool> kDomainSuggestionsAlternativeScoring;

// ---------------------------------------------------------
// ML Relevance Scoring ->

// The ML Relevance Scoring features and params configuration.
// Use `GetMLConfig()` to get the current configuration.
//
// `MLConfig` has the same thread-safety as base::FeatureList. The first call to
// `GetMLConfig()` (which performs initialization) must be done single threaded
// on the main thread. After that, it can be called from any thread.
struct MLConfig {
  MLConfig();

  // If true, logs Omnibox URL scoring signals to OmniboxEventProto.
  // Equivalent to omnibox::kLogUrlScoringSignals.
  bool log_url_scoring_signals{false};

  // If true, enables scoring signal annotators for logging Omnibox URL scoring
  // signals to OmniboxEventProto. Equivalent to
  // OmniboxFieldTrial::kLogUrlScoringSignalsEnableScoringSignalsAnnotators.
  bool enable_scoring_signals_annotators{false};

  // If true, runs the ML scoring model to assign new relevance scores to the
  // URL suggestions and reranks them.
  // Equivalent to omnibox::kMlUrlScoring.
  bool ml_url_scoring{false};

  // If true, runs the ML scoring model but does not assign new relevance scores
  // to the URL suggestions and does not rerank them.
  // Equivalent to OmniboxFieldTrial::kMlUrlScoringCounterfactual.
  bool ml_url_scoring_counterfactual{false};

  // If true, increases the number of candidates the URL autocomplete providers
  // pass to the controller beyond `provider_max_matches`.
  // Equivalent to OmniboxFieldTrial::kMlUrlScoringIncreaseNumCandidates.
  bool ml_url_scoring_increase_num_candidates{false};

  // If true, the ML model only re-scores and re-ranks the final set of matches
  // that would be shown in the legacy scoring system. The full legacy system
  // including the final call to `SortAndCull()` is completed before the ML
  // model is invoked.
  bool ml_url_scoring_rerank_final_matches_only{false};

  // If true, the would-be default match from the legacy system is determined
  // before ML scoring is invoked, and preserved even after re-scoring and
  // re-ranking with the new scores.
  bool ml_url_scoring_preserve_default{false};

  // If true, creates Omnibox autocompete URL scoring model.
  // Equivalent to omnibox::kUrlScoringModel.
  bool url_scoring_model{false};
};

// A testing utility class for overriding the current configuration returned
// by the global or member `GetMLConfig()` and restoring it once the instance
// goes out of scope.
class ScopedMLConfigForTesting {
 public:
  ScopedMLConfigForTesting();
  ScopedMLConfigForTesting(const ScopedMLConfigForTesting&) = delete;
  ScopedMLConfigForTesting& operator=(const ScopedMLConfigForTesting&) = delete;
  ~ScopedMLConfigForTesting();

  // Returns the current configuration.
  MLConfig& GetMLConfig();

 private:
  std::unique_ptr<MLConfig> original_config_{nullptr};
};

// Returns the current configuration.
const MLConfig& GetMLConfig();

// If enabled, logs Omnibox scoring signals to OmniboxEventProto for training
// the ML scoring models.
bool IsLogUrlScoringSignalsEnabled();

// Whether the scoring signal annotators are enabled for logging Omnibox scoring
// signals to OmniboxEventProto.
bool AreScoringSignalsAnnotatorsEnabled();

// If enabled, runs the ML scoring model to assign new relevance scores to the
// URL suggestions and reranks them.
bool IsMlUrlScoringEnabled();

// If true, runs the ML scoring model but does not assign new relevance scores
// to URL suggestions.
bool IsMlUrlScoringCounterfactual();

// If true, increases the number of candidates the url autocomplete providers
// pass to the controller.
bool IsMlUrlScoringIncreaseNumCandidatesEnabled();

// Whether the URL scoring model is enabled.
bool IsUrlScoringModelEnabled();

// <- ML Relevance Scoring
// ---------------------------------------------------------
// Two-column realbox ->

// Specifies the number of zero-prefix suggestions in the 2nd column of realbox
// when `omnibox::kRealboxSecondaryZeroSuggest` is enabled.
extern const base::FeatureParam<int>
    kRealboxMaxPreviousSearchRelatedSuggestions;
// Does not show zero-prefix suggestions in the 2nd column of realbox, even if
// they are available.
extern const base::FeatureParam<bool>
    kRealboxSecondaryZeroSuggestCounterfactual;

// <- Two-column realbox
// ---------------------------------------------------------
// Inspire Me ->

// Specify number of additional Related and Trending queries appended to the
// suggestion list, when the Inspire Me feature is enabled.
extern const base::FeatureParam<int> kInspireMeAdditionalRelatedQueries;
extern const base::FeatureParam<int> kInspireMeAdditionalTrendingQueries;

// <- Inspire Me
// ---------------------------------------------------------
// Actions In Suggest ->
//
// When set to true, permits Entity suggestion with associated Actions to be
// promoted over the Escape Hatch.
extern const base::FeatureParam<bool> kActionsInSuggestPromoteEntitySuggestion;
// <- Actions In Suggest
// ---------------------------------------------------------

// New params should be inserted above this comment. They should be ordered
// consistently with `omnibox_features.h`. They should be formatted as:
// - Short comment categorizing the relevant features & params.
// - Optional: `bool Is[FeatureName]Enabled();` helpers that check if the
//   related features in `omnibox_features.h` are enabled.
// - Optional: Helper getter functions to determine the param values when
//   they're not trivial. E.g. a helper may be needed to return the min of 2
//   params. Trivial helpers that simply return the param values should be
//   omitted.
// - `extern const base::FeatureParam<[T]> k[ParamName];` declarations for
//   params. Param names should not begin with a `omnibox` prefix or end with a
//   `Param` suffix. Names for the same or related feature should share a common
//   prefix.
// An example:
/*
  // Drive suggestions.
  // True if the feature to enable the document provider is enabled.
  bool IsDocumentSuggestionsEnabled();
  // True if the feature to debounce the document provider is enabled.
  bool IsDocumentDebouncingEnabled();
  // The minimum input length for which to show document suggestions.
  extern const base::FeatureParam<int> kDocumentMinChar;
  // If true, document suggestions will be hidden but logged for analysis.
  extern const base::FeatureParam<bool> kDocumentCounterfactual;
*/

namespace internal {
// The bundled omnibox experiment comes with a set of parameters
// (key-value pairs).  Each key indicates a certain rule that applies in
// a certain context.  The value indicates what the consequences of
// applying the rule are.  For example, the value of a SearchHistory rule
// in the context of a search results page might indicate that we should
// prevent search history matches from inlining.
//
// This function returns the value associated with the |rule| that applies
// in the current context (which currently consists of |page_classification|
// and whether Instant Extended is enabled).  If no such rule exists in the
// current context, fall back to the rule in various wildcard contexts and
// return its value if found.  If the rule remains unfound in the global
// context, returns the empty string.  For more details, including how we
// prioritize different wildcard contexts, see the implementation.  How to
// interpret the value is left to the caller; this is rule-dependent.
//
// Deprecated. Use GetValueForRuleInContextByFeature instead.
std::string GetValueForRuleInContext(
    const std::string& rule,
    metrics::OmniboxEventProto::PageClassification page_classification);

// Same as GetValueForRuleInContext, but by |feature| instead of the bundled
// omnibox experiment.  Prefer to use this method over GetValueForRuleInContext
// when possible, as it can be useful to configure parameters outside of the
// omnibox bundled experiment.
std::string GetValueForRuleInContextByFeature(
    const base::Feature& feature,
    const std::string& rule,
    metrics::OmniboxEventProto::PageClassification page_classification);

}  // namespace internal

}  // namespace OmniboxFieldTrial

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_FIELD_TRIAL_H_
