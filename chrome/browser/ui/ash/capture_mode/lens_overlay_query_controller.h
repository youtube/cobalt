// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_QUERY_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_QUERY_CONTROLLER_H_

#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/capture_mode/lens_overlay_request_id_generator.h"
// TODO(b/362363034): Determine whether to use `LensOverlayClientLogs` for
// client metrics.
#include "chrome/browser/ui/lens/ref_counted_lens_overlay_client_logs.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/lens_server_proto/lens_overlay_client_context.pb.h"
#include "third_party/lens_server_proto/lens_overlay_cluster_info.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_crop.pb.h"
#include "third_party/lens_server_proto/lens_overlay_image_data.pb.h"
#include "third_party/lens_server_proto/lens_overlay_interaction_request_metadata.pb.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

class Profile;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace variations {
class VariationsClient;
}  // namespace variations

// Callback type alias for the lens overlay full image response.
using LensOverlayFullImageResponseCallback = base::RepeatingCallback<
    void(std::vector<lens::OverlayObject>, lens::Text, bool)>;
// Callback type alias for the lens overlay url response.
using LensOverlayUrlResponseCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>;
// Callback type alias for the lens overlay suggest inputs response.
using LensOverlaySuggestInputsCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlaySuggestInputs)>;
// Callback type alias for the thumbnail image creation.
using LensOverlayThumbnailCreatedCallback =
    base::RepeatingCallback<void(const std::string&)>;
// Callback type alias for the OAuth headers created.
using OAuthHeadersCreatedCallback =
    base::OnceCallback<void(std::vector<std::string>)>;

// Manages queries on behalf of a Capture Mode delegate.
class LensOverlayQueryController {
 public:
  LensOverlayQueryController(
      LensOverlayFullImageResponseCallback full_image_callback,
      LensOverlayUrlResponseCallback url_callback,
      LensOverlaySuggestInputsCallback suggest_inputs_callback,
      LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode);
  LensOverlayQueryController(const LensOverlayQueryController&) = delete;
  LensOverlayQueryController& operator=(const LensOverlayQueryController&) =
      delete;
  ~LensOverlayQueryController();

  // Starts a query flow by sending a request to Lens using the screenshot,
  // returning the response to the full image callback. Should be called
  // exactly once. Override these methods to stub out network requests for
  // testing.
  void StartQueryFlow(
      const SkBitmap& screenshot,
      GURL page_url,
      std::optional<std::string> page_title,
      std::vector<lens::CenterRotatedBox> significant_region_boxes,
      base::span<const uint8_t> underlying_content_bytes,
      lens::MimeType underlying_content_type,
      float ui_scale_factor,
      base::TimeTicks invocation_time);

  // Clears the state and resets stored values.
  void EndQuery();

  // Sends a region search interaction. Expected to be called multiple times. If
  // region_bytes are included, those will be sent to Lens instead of cropping
  // the region out of the screenshot. This should be used to provide a higher
  // definition image than image cropping would provide.
  void SendRegionSearch(
      lens::CenterRotatedBox region,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Sends a multimodal interaction. Expected to be called multiple times.
  void SendMultimodalRequest(
      lens::CenterRotatedBox region,
      const std::string& query_text,
      lens::LensOverlaySelectionType lens_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

 protected:
  // Returns the EndpointFetcher to use with the given params. Protected to
  // allow overriding in tests to mock server responses.
  std::unique_ptr<EndpointFetcher> CreateEndpointFetcher(
      lens::LensOverlayServerRequest* request,
      const GURL& fetch_url,
      const HttpMethod& http_method,
      const base::TimeDelta& timeout,
      const std::vector<std::string>& request_headers,
      const std::vector<std::string>& cors_exempt_headers);

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  LensOverlayFullImageResponseCallback full_image_callback_;

  // Suggest inputs callback, used for sending Lens suggest data to the
  // search box.
  LensOverlaySuggestInputsCallback suggest_inputs_callback_;

  // Callback for when a thumbnail image is created from a region selection.
  LensOverlayThumbnailCreatedCallback thumbnail_created_callback_;

 private:
  enum class QueryControllerState {
    // StartQueryFlow has not been called and the query controller is
    // inactive.
    kOff = 0,
    // StartQueryFlow has been called, but the cluster info has not been
    // received so we cannot proceed to sending the full image request.
    kAwaitingClusterInfoResponse = 1,
    // The cluster info response has been received so we can proceed to sending
    // the full image request.
    kReceivedClusterInfoResponse = 2,
    // The full image response has not been received, or is no longer valid.
    kAwaitingFullImageResponse = 3,
    // The full image response has been received and the query controller can
    // send interaction requests.
    kReceivedFullImageResponse = 4,
    // The full image response has been received and resulted in an error
    // response.
    kReceivedFullImageErrorResponse = 5,
  };

  // Data class for constructing a fetch request to the Lens servers.
  // All fields that are required for the request should use std::unique_ptr to
  // validate all fields are non-null prior to making a request. If a field does
  // not need to be set, but should be included if it is set, use std::optional.
  struct LensServerFetchRequest {
   public:
    LensServerFetchRequest(
        std::unique_ptr<lens::LensOverlayRequestId> request_id,
        base::TimeTicks query_start_time);
    ~LensServerFetchRequest();

    // Returns the sequence ID of the request this data belongs to. Used
    // for cancelling any requests that have been superseded by another.
    int sequence_id() const { return request_id_->sequence_id(); }

    // The request ID for this request.
    const std::unique_ptr<lens::LensOverlayRequestId> request_id_;

    // The start time of the query.
    const base::TimeTicks query_start_time_;

    // The request to be sent to the server. Must be set prior to making the
    // request.
    std::unique_ptr<lens::LensOverlayServerRequest> request_;

    // The headers to attach to the request.
    std::unique_ptr<std::vector<std::string>> request_headers_;

    // A callback to run once the request has been sent. This is optional, but
    // can be used to run some logic once the request has been sent.
    std::optional<base::OnceClosure> request_sent_callback_;
  };

  // Makes a LensOverlayServerClusterInfoRequest to get the cluster info. Will
  // continue to the FullImageRequest once a response is received.
  void FetchClusterInfoRequest();

  // Creates the endpoint fetcher and sends the cluster info request.
  void PerformClusterInfoFetchRequest(base::TimeTicks query_start_time,
                                      std::vector<std::string> request_headers);

  // Handles the response from the cluster info request. If a successful request
  // was made, kicks off the full image request to use the retrieved server
  // session id. If the request failed, the full image request will still be
  // tried, just without the server session id.
  void ClusterInfoFetchResponseHandler(
      base::TimeTicks query_start_time,
      std::unique_ptr<EndpointResponse> response);

  // Processes the screenshot and fetches a full image request.
  void PrepareAndFetchFullImageRequest();

  // Does any preprocessing on the image data outside of encoding the
  // screenshot bytes that needs to be done before attaching the ImageData to
  // the full image request.
  void PrepareImageDataForFullImageRequest(
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      lens::ImageData image_data);

  // Creates the FullImageRequest that is sent to the server and tries to
  // perform the request. If all async flows have not finished, the attempt to
  // perform the request will be ignored, and the last async flow to finish
  // will perform the request.
  void CreateFullImageRequestAndTryPerformFullImageRequest(
      int sequence_id,
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      lens::ImageData image_data);

  // Creates the OAuth headers to be used in the full image request. If the
  // users OAuth is unavailable, will fallback to using the API key. If all
  // async flows have not finished, the attempt to perform the request will be
  // ignored, and the last async flow to finish will perform the request.
  void CreateOAuthHeadersAndTryPerformFullPageRequest(int sequence_id);

  // Called when an asynchronous piece of data needed to make the full image
  // request is ready. Once this has been invoked with every necessary piece of
  // data with the same sequence_id, it will call PerformFullImageRequest to
  // send the request to the server. Ignores any data received from an old
  // sequence_id.
  void FullImageRequestDataReady(int sequence_id,
                                 lens::LensOverlayServerRequest request);
  void FullImageRequestDataReady(int sequence_id,
                                 std::vector<std::string> headers);
  // Helper to the above, used to actually validate the data prior to calling
  // PerformFullImageRequest().
  void FullImageRequestDataHelper(int sequence_id);

  // Verifies the given sequence_id is still the most recent.
  bool IsCurrentFullImageSequence(int sequence_id);

  // Creates the endpoint fetcher and send the full image request to the server.
  void PerformFullImageRequest();

  // Handles the endpoint fetch response for the full image request.
  void FullImageFetchResponseHandler(
      int request_sequence_id,
      std::unique_ptr<EndpointResponse> response);

  // Runs the full image callback with empty response data, for errors.
  void RunFullImageCallbackForError();

  // Sends the interaction data, triggering async image cropping and fetching
  // the request.
  void SendInteraction(
      lens::CenterRotatedBox region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes);

  // Creates the interaction request that is sent to the server and tries to
  // perform the interaction request. If not all asynchronous flows have
  // finished, the attempt to perform the request will be ignored. Only the last
  // asynchronous flow to finish will perform the request.
  void CreateInteractionRequestAndTryPerformInteractionRequest(
      int sequence_id,
      lens::CenterRotatedBox region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      scoped_refptr<lens::RefCountedLensOverlayClientLogs> ref_counted_logs,
      std::optional<lens::ImageCrop> image_crop);

  // Creates the OAuth headers that get attached to the interaction request to
  // authenticate the user. After, tries to perform the interaction request. If
  // not all asynchronous flows have finished, the attempt to perform the
  // request will be ignored. Only the last asynchronous flow to finish will
  // perform the request.
  void CreateOAuthHeadersAndTryPerformInteractionRequest(int sequence_id);

  // Called when an asynchronous piece of data needed to make the interaction
  // request is ready. Once this has been invoked with every necessary piece of
  // data with the same sequence_id, it will call PerformInteractionRequest to
  // send the request to the server. Ignores any data received from an old
  // sequence_id.
  void InteractionRequestDataReady(int sequence_id,
                                   lens::LensOverlayServerRequest request);
  void InteractionRequestDataReady(int sequence_id,
                                   std::vector<std::string> headers);

  // If all data needed to PerformInteractionRequest is available, will call
  // PerformInteractionRequest to fetch the request. If any async flow has not
  // finished, it will ignore the request with the assumption
  // TryPerformInteractionRequest will be called again once the flow has
  // finished. Will also ensure the full image response has been received. If
  // the full image response has not been received, will kick off the full image
  // response flow with a callback to send this interaction request after.
  void TryPerformInteractionRequest(int sequence_id);

  // Verifies the given sequence_id is still the most recent.
  bool IsCurrentInteractionSequence(int sequence_id);

  // Creates the endpoint fetcher and send the full image request to the server.
  void PerformInteractionRequest();

  // Creates the URL to load in the side panel and sends it to the callback.
  void CreateSearchUrlAndSendToCallback(
      std::optional<std::string> query_text,
      std::map<std::string, std::string> additional_search_query_params,
      lens::LensOverlaySelectionType selection_type,
      std::unique_ptr<lens::LensOverlayRequestId> request_id);

  // Handles the endpoint fetch response for an interaction request.
  void InteractionFetchResponseHandler(
      int sequence_id,
      std::unique_ptr<EndpointResponse> response);

  // Runs the interaction callback with empty response data, for errors.
  void RunInteractionCallbackForError();

  // Creates an endpoint fetcher with the given request_headers to perform the
  // given request. Calls fetcher_created_callback when the EndpointFetcher is
  // created to keep it alive while the request is being made.
  // response_received_callback is invoked once the request returns a response.
  void PerformFetchRequest(
      lens::LensOverlayServerRequest* request,
      std::vector<std::string>* request_headers,
      const base::TimeDelta& timeout,
      base::OnceCallback<void(std::unique_ptr<EndpointFetcher>)>
          fetcher_created_callback,
      EndpointFetcherCallback response_received_callback);

  // Creates a client context proto to be attached to a server request.
  lens::LensOverlayClientContext CreateClientContext();

  // Fetches the OAuth headers and calls the callback with the headers. If the
  // OAuth cannot be retrieve (like if the user is not logged in), the callback
  // will be called with an empty vector. Returns the access token fetcher
  // making the request so it can be kept alive.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
  CreateOAuthHeadersAndContinue(OAuthHeadersCreatedCallback callback);

  // Gets the visual search interaction log data param as a base64url
  // encoded string.
  std::string GetEncodedVisualSearchInteractionLogData(
      lens::LensOverlaySelectionType selection_type);

  // Creates the metadata for an interaction request using the latest
  // interaction and image crop data.
  lens::LensOverlayServerRequest CreateInteractionRequest(
      lens::CenterRotatedBox region,
      std::optional<std::string> query_text,
      std::optional<std::string> object_id,
      std::optional<lens::ImageCrop> image_crop,
      lens::LensOverlayClientLogs client_logs);

  // Resets the request cluster info state.
  void ResetRequestClusterInfoState();

  // Updates the suggest inputs with the request id.
  void UpdateSuggestInputsWithRequestId(lens::LensOverlayRequestId* request_id);

  // Updates the suggest inputs with the feature params and latest cluster info
  // response, then runs the callback.
  void RunSuggestInputsCallback();

  // Callback for when the full image endpoint fetcher is created.
  void OnFullImageEndpointFetcherCreated(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher);

  // Callback for when the interaction endpoint fetcher is created.
  void OnInteractionEndpointFetcherCreated(
      std::unique_ptr<EndpointFetcher> endpoint_fetcher);

  // The request id generator.
  std::unique_ptr<LensOverlayRequestIdGenerator> request_id_generator_;

  // The original screenshot image.
  SkBitmap original_screenshot_;

  // The dimensions of the resized bitmap. Needed in case geometry needs to be
  // recaclulated. For example, in the case of translated words.
  gfx::Size resized_bitmap_size_;

  // The page url. Empty if it is not allowed to be shared.
  GURL page_url_;

  // The page title, if it is allowed to be shared.
  std::optional<std::string> page_title_;

  // Options needed to send a translate request with the proper parameters.
  struct TranslateOptions {
    std::string source_language;
    std::string target_language;

    TranslateOptions(const std::string& source, const std::string& target)
        : source_language(source), target_language(target) {}
  };
  std::optional<TranslateOptions> translate_options_;

  // Bounding boxes for significant regions identified in the original
  // screenshot image.
  std::vector<lens::CenterRotatedBox> significant_region_boxes_;

  // The UI Scaling Factor of the underlying page, if it has been passed in.
  // Else 0.
  float ui_scale_factor_ = 0;

  // The current state.
  QueryControllerState query_controller_state_ = QueryControllerState::kOff;

  // The callback for full image requests, including upon query flow start
  // and interaction retries.
  LensOverlayUrlResponseCallback url_callback_;

  // The last received cluster info.
  std::optional<lens::LensOverlayClusterInfo> cluster_info_ = std::nullopt;

  // The callback for issuing a pending interaction request. Will be used to
  // send the interaction request after the cluster info is available and the
  // full image request id sequence is ready, if the interaction occurred
  // before the full image response was received.
  base::OnceClosure pending_interaction_callback_;

  // TODO(b/370805019): All our flows are requesting the same headers, so
  // ideally we use one fetcher that returns the same headers wherever they are
  // needed.
  // The access token fetcher used for getting OAuth for cluster info requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      cluster_info_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for full image requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      full_image_access_token_fetcher_;

  // The access token fetcher used for getting OAuth for interaction requests.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      interaction_access_token_fetcher_;

  // The data for the full image request in progress. Is null if no full image
  // request has been made.
  std::unique_ptr<LensServerFetchRequest> latest_full_image_request_data_;

  // The data for the interaction request in progress. Is null if no interaction
  // request has been made.
  std::unique_ptr<LensServerFetchRequest> latest_interaction_request_data_;

  // The endpoint fetcher used for the cluster info request.
  std::unique_ptr<EndpointFetcher> cluster_info_endpoint_fetcher_;

  // The endpoint fetcher used for the full image request.
  std::unique_ptr<EndpointFetcher> full_image_endpoint_fetcher_;

  // The endpoint fetcher used for the interaction request. Only the last
  // endpoint fetcher is kept; additional fetch requests will discard
  // earlier unfinished requests.
  std::unique_ptr<EndpointFetcher> interaction_endpoint_fetcher_;

  // Task runner used to encode/downscale the JPEG images on a separate thread.
  scoped_refptr<base::TaskRunner> encoding_task_runner_;

  // Tracks the encoding/downscaling tasks currently running for follow up
  // interactions. Does not track the encoding for the full image request
  // because it is assumed this request will finish, never need to be
  // cancelled, and all other tasks will wait on it if needed.
  std::unique_ptr<base::CancelableTaskTracker> encoding_task_tracker_;

  // The current suggest inputs. The fields in this proto are updated
  // whenever new data is available (i.e. after an objects or interaction
  // response is received) and the overlay controller notified via the
  // suggest inputs callback.
  lens::proto::LensOverlaySuggestInputs suggest_inputs_;

  // Owned by Profile, and thus guaranteed to outlive this instance.
  const raw_ptr<variations::VariationsClient> variations_client_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  const raw_ptr<Profile> profile_;

  // The bytes of the content the user is viewing. Owned by
  // LensOverlayController. Will be empty if no bytes to the underlying page
  // could be provided.
  // TODO(367764863) Rewrite to base::raw_span.
  RAW_PTR_EXCLUSION base::span<const uint8_t> underlying_content_bytes_;

  // The mime type of underlying_content_bytes. Will be kNone if
  // underlying_content_bytes_ is empty.
  lens::MimeType underlying_content_type_;

  // Whether or not the parent interaction query has been sent. This should
  // always be the first interaction in a query flow.
  bool parent_query_sent_ = false;

  // The invocation source that triggered the query flow.
  lens::LensOverlayInvocationSource invocation_source_;

  // Whether or not to use dark mode in search urls. This is only calculated
  // once per session because the search box theme is also only set once
  // per session.
  bool use_dark_mode_;

  // The time it took from sending the cluster info request to receiving
  // the response.
  std::optional<base::TimeDelta> cluster_info_fetch_response_time_;

  base::WeakPtrFactory<LensOverlayQueryController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_CAPTURE_MODE_LENS_OVERLAY_QUERY_CONTROLLER_H_
