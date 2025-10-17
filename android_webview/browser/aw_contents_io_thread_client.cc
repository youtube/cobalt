// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_contents_io_thread_client.h"

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "android_webview/browser/aw_settings.h"
#include "android_webview/browser/network_service/aw_web_resource_intercept_response.h"
#include "android_webview/browser/network_service/aw_web_resource_request.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/devtools_instrumentation.h"
#include "base/android/jni_array.h"
#include "base/android/jni_callback.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/base_tracing.h"
#include "base/unguessable_token.h"
#include "components/embedder_support/android/util/features.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "components/embedder_support/android/util/web_resource_response.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/data_url.h"
#include "services/network/public/cpp/resource_request.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwContentsIoThreadClient_jni.h"
#include "android_webview/browser_jni_headers/ShouldInterceptRequestMediator_jni.h"

using base::LazyInstance;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;
using content::BrowserThread;
using content::RenderFrameHost;
using content::WebContents;
using std::map;
using std::pair;
using std::string;

namespace android_webview {

namespace {

using RenderFrameHostToWeakGlobalRefType =
    map<content::GlobalRenderFrameHostToken, JavaObjectWeakGlobalRef>;

using HostsAndWeakGlobalRefPair =
    pair<base::flat_set<raw_ptr<RenderFrameHost, CtnExperimental>>,
         JavaObjectWeakGlobalRef>;

// When browser side navigation is enabled, RenderFrameIDs do not have
// valid render process host and render frame ids for frame navigations.
// We need to identify these by using FrameTreeNodeIds. Furthermore, we need
// to keep track of which RenderFrameHosts are associated with each
// FrameTreeNodeId, so we know when the last RenderFrameHost is deleted (and
// therefore the FrameTreeNodeId should be removed).
using FrameTreeNodeToWeakGlobalRefType =
    map<content::FrameTreeNodeId, HostsAndWeakGlobalRefPair>;

// RfhToIoThreadClientMap -----------------------------------------------------
class RfhToIoThreadClientMap {
 public:
  static RfhToIoThreadClientMap* GetInstance();
  void Set(const content::GlobalRenderFrameHostToken& rfh_token,
           const JavaObjectWeakGlobalRef& client);
  std::optional<JavaObjectWeakGlobalRef> Get(
      const content::GlobalRenderFrameHostToken& rfh_token);

  std::optional<JavaObjectWeakGlobalRef> Get(
      content::FrameTreeNodeId frame_tree_node_id);

  // Prefer to call these when RenderFrameHost* is available, because they
  // update both maps at the same time.
  void Set(RenderFrameHost* rfh, const JavaObjectWeakGlobalRef& client);
  void Erase(RenderFrameHost* rfh);

  void RenderFrameHostChanged(RenderFrameHost* old_rfh,
                              RenderFrameHost* new_rfh);

 private:
  base::Lock map_lock_;
  // We maintain two maps simultaneously so that we can always get the correct
  // JavaObjectWeakGlobalRef, even when only GlobalRenderFrameHostToken or
  // FrameTreeNodeId is available.
  RenderFrameHostToWeakGlobalRefType rfh_to_weak_global_ref_;
  FrameTreeNodeToWeakGlobalRefType frame_tree_node_to_weak_global_ref_;
};

// static
LazyInstance<RfhToIoThreadClientMap>::DestructorAtExit g_instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
RfhToIoThreadClientMap* RfhToIoThreadClientMap::GetInstance() {
  return g_instance_.Pointer();
}

void RfhToIoThreadClientMap::Set(
    const content::GlobalRenderFrameHostToken& rfh_token,
    const JavaObjectWeakGlobalRef& client) {
  base::AutoLock lock(map_lock_);
  rfh_to_weak_global_ref_[rfh_token] = client;
}

std::optional<JavaObjectWeakGlobalRef> RfhToIoThreadClientMap::Get(
    const content::GlobalRenderFrameHostToken& rfh_token) {
  base::AutoLock lock(map_lock_);
  RenderFrameHostToWeakGlobalRefType::iterator iterator =
      rfh_to_weak_global_ref_.find(rfh_token);
  if (iterator == rfh_to_weak_global_ref_.end()) {
    return std::nullopt;
  } else {
    return iterator->second;
  }
}

std::optional<JavaObjectWeakGlobalRef> RfhToIoThreadClientMap::Get(
    content::FrameTreeNodeId frame_tree_node_id) {
  base::AutoLock lock(map_lock_);
  FrameTreeNodeToWeakGlobalRefType::iterator iterator =
      frame_tree_node_to_weak_global_ref_.find(frame_tree_node_id);
  if (iterator == frame_tree_node_to_weak_global_ref_.end()) {
    return std::nullopt;
  } else {
    return iterator->second.second;
  }
}

void RfhToIoThreadClientMap::Set(RenderFrameHost* rfh,
                                 const JavaObjectWeakGlobalRef& client) {
  content::FrameTreeNodeId frame_tree_node_id = rfh->GetFrameTreeNodeId();
  auto rfh_token = rfh->GetGlobalFrameToken();
  base::AutoLock lock(map_lock_);

  // If this FrameTreeNodeId already has an associated JavaObjectWeakGlobalRef,
  // add this RenderFrameHost to the hosts set (it's harmless to overwrite the
  // JavaObjectWeakGlobalRef). Otherwise, operator[] creates a new map entry and
  // we add this RenderFrameHost to the hosts set and insert |client| in the
  // pair.
  HostsAndWeakGlobalRefPair& current_entry =
      frame_tree_node_to_weak_global_ref_[frame_tree_node_id];
  current_entry.second = client;
  current_entry.first.insert(rfh);

  // Always add the entry to the HostIdPair map, since entries are 1:1 with
  // RenderFrameHosts.
  rfh_to_weak_global_ref_[rfh_token] = client;
}

void RfhToIoThreadClientMap::Erase(RenderFrameHost* rfh) {
  content::FrameTreeNodeId frame_tree_node_id = rfh->GetFrameTreeNodeId();
  auto rfh_token = rfh->GetGlobalFrameToken();
  base::AutoLock lock(map_lock_);
  HostsAndWeakGlobalRefPair& current_entry =
      frame_tree_node_to_weak_global_ref_[frame_tree_node_id];
  size_t num_erased = current_entry.first.erase(rfh);
  DCHECK_EQ(num_erased, 1u);
  // Only remove this entry from the FrameTreeNodeId map if there are no more
  // live RenderFrameHosts.
  if (current_entry.first.empty()) {
    frame_tree_node_to_weak_global_ref_.erase(frame_tree_node_id);
  }

  // Always safe to remove the entry from the HostIdPair map, since entries are
  // 1:1 with RenderFrameHosts.
  rfh_to_weak_global_ref_.erase(rfh_token);
}

void RfhToIoThreadClientMap::RenderFrameHostChanged(RenderFrameHost* old_rfh,
                                                    RenderFrameHost* new_rfh) {
  // Handles FrameTree swap, which occurs only in prerender activation.

  if (old_rfh == nullptr) {
    return;
  }
  // Ensured by `WebContentsObserver::RenderFrameHostChanged()`.
  CHECK(new_rfh);

  // If the swap is for subframes, it's not a prerender activation and therefore
  // not a FrameTree swap.
  if (old_rfh->GetParentOrOuterDocument() ||
      new_rfh->GetParentOrOuterDocument()) {
    return;
  }

  // If this is a prerender activation, `new_rfh` should initially be associated
  // with a prerendering FrameTree's root FrameTreeNode before getting
  // transferred to the primary FrameTree's root FrameTreeNode during the
  // navigation commit. (See also `PrerenderHost::Activate()`.) This means the
  // entry for `new_rfh` used in `frame_tree_node_to_weak_global_ref_` will be
  // keyed by the prerendering FrameTreeNode id. We want to move that entry to
  // the primary root FTN's entry, so that future lookups will be correct.
  //
  // If `pre_swap_ftn_id` and `post_swap_ftn_id` are the same, it's not a
  // FrameTree swap (and therefore not a prerender activation). So, there's no
  // need to move entries.
  content::FrameTreeNodeId pre_swap_ftn_id;
  content::FrameTreeNodeId post_swap_ftn_id = new_rfh->GetFrameTreeNodeId();
  CHECK_EQ(post_swap_ftn_id, old_rfh->GetFrameTreeNodeId());

  base::AutoLock lock(map_lock_);

  for (auto& [frame_tree_node_id, entry] :
       frame_tree_node_to_weak_global_ref_) {
    if (entry.first.contains(new_rfh)) {
      pre_swap_ftn_id = frame_tree_node_id;
      break;
    }
  }

  CHECK(pre_swap_ftn_id);

  if (pre_swap_ftn_id == post_swap_ftn_id) {
    return;
  }

  // Otherwise, it is a prerender activation and `new_rfh` is now attached to
  // frame tree node with `post_swap_ftn_id`. So, we move entry with key
  // `(pre_swap_ftn_id, new_rfh)` to `(post_swap_ftn_id, new_rfh)`.

  HostsAndWeakGlobalRefPair& pre_swap_entry =
      frame_tree_node_to_weak_global_ref_[pre_swap_ftn_id];
  HostsAndWeakGlobalRefPair& post_swap_entry =
      frame_tree_node_to_weak_global_ref_[post_swap_ftn_id];

  // Note that `post_swap_entry.second == pre_swap_entry.second` must hold
  // because `old_rfh` and `new_rfh` belongs to the same WebContents. It's nice
  // to `CHECK_EQ`, but we can't because `JavaObjectWeakGlobalRef` doesn't
  // provide comparison.
  //
  // Note that we don't modify `rfh_to_weak_global_ref_` in this function, as we
  // are only handling the moving of the entries of
  // `frame_tree_node_to_weak_global_ref_` that is affected by the FrameTreeNode
  // change here. Addition/deletion of RenderFrameHosts is handled by
  // `WebContentsObserver::RenderFrameCreated()` and
  // `WebContentsObserver::RenderFrameDeleted()` calling `Set()` and `Erase()`
  // respectively.

  size_t num_erased = pre_swap_entry.first.erase(new_rfh);
  CHECK_EQ(num_erased, 1u);
  post_swap_entry.first.insert(new_rfh);

  CHECK(pre_swap_entry.first.empty());
  frame_tree_node_to_weak_global_ref_.erase(pre_swap_ftn_id);
}

// WebContentsToIoThreadClientMap ---------------------------------------------

class WebContentsToIoThreadClientMap {
 public:
  static WebContentsToIoThreadClientMap* GetInstance() {
    static base::NoDestructor<WebContentsToIoThreadClientMap> instance;
    return instance.get();
  }

  void Set(WebContentsKey key, const JavaObjectWeakGlobalRef& client) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::AutoLock lock(map_lock_);
    web_contents_to_weak_global_ref_[key] = client;
  }

  std::optional<JavaObjectWeakGlobalRef> Get(WebContentsKey key) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    base::AutoLock lock(map_lock_);
    auto iterator = web_contents_to_weak_global_ref_.find(key);
    if (iterator == web_contents_to_weak_global_ref_.end()) {
      return std::nullopt;
    } else {
      return iterator->second;
    }
  }

  void Erase(WebContentsKey key) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::AutoLock lock(map_lock_);
    web_contents_to_weak_global_ref_.erase(key);
  }

 private:
  base::Lock map_lock_;
  map<WebContentsKey, JavaObjectWeakGlobalRef> web_contents_to_weak_global_ref_;
};

// ClientMapEntryUpdater ------------------------------------------------------

class ClientMapEntryUpdater : public content::WebContentsObserver {
 public:
  ClientMapEntryUpdater(JNIEnv* env,
                        WebContents* web_contents,
                        const jni_zero::JavaRef<jobject>& jdelegate);

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(RenderFrameHost* old_rfh,
                              RenderFrameHost* new_rfh) override;
  void WebContentsDestroyed() override;

 private:
  JavaObjectWeakGlobalRef jdelegate_;
};

ClientMapEntryUpdater::ClientMapEntryUpdater(
    JNIEnv* env,
    WebContents* web_contents,
    const jni_zero::JavaRef<jobject>& jdelegate)
    : content::WebContentsObserver(web_contents), jdelegate_(env, jdelegate) {
  DCHECK(web_contents);
  DCHECK(jdelegate);

  if (web_contents->GetPrimaryMainFrame())
    RenderFrameCreated(web_contents->GetPrimaryMainFrame());

  WebContentsToIoThreadClientMap::GetInstance()->Set(
      GetWebContentsKey(*web_contents), jdelegate_);
}

void ClientMapEntryUpdater::RenderFrameCreated(RenderFrameHost* rfh) {
  RfhToIoThreadClientMap::GetInstance()->Set(rfh, jdelegate_);
}

void ClientMapEntryUpdater::RenderFrameDeleted(RenderFrameHost* rfh) {
  RfhToIoThreadClientMap::GetInstance()->Erase(rfh);
}

void ClientMapEntryUpdater::RenderFrameHostChanged(RenderFrameHost* old_rfh,
                                                   RenderFrameHost* new_rfh) {
  RfhToIoThreadClientMap::GetInstance()->RenderFrameHostChanged(old_rfh,
                                                                new_rfh);
}

void ClientMapEntryUpdater::WebContentsDestroyed() {
  WebContentsToIoThreadClientMap::GetInstance()->Erase(
      GetWebContentsKey(*web_contents()));
  delete this;
}

class WebContentsKeyHolder
    : public content::WebContentsUserData<WebContentsKeyHolder> {
 public:
  // [M138] We cherry-pick the helper function introduced in [*] only for this
  // class's use.
  // [*] crrev.com/6626551
  static WebContentsKeyHolder* GetOrCreateForWebContents(
      WebContents* contents) {
    if (!FromWebContents(contents)) {
      CreateForWebContents(contents);
    }
    return FromWebContents(contents);
  }

  ~WebContentsKeyHolder() override = default;

  const base::UnguessableToken& GetToken() { return token_; }

 private:
  explicit WebContentsKeyHolder(WebContents* contents)
      : content::WebContentsUserData<WebContentsKeyHolder>(*contents),
        token_(base::UnguessableToken::Create()) {}
  friend class WebContentsUserData<WebContentsKeyHolder>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
  const base::UnguessableToken token_;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsKeyHolder);

}  // namespace

WebContentsKey GetWebContentsKey(content::WebContents& web_contents) {
  return WebContentsKeyHolder::GetOrCreateForWebContents(&web_contents)
      ->GetToken();
}

// AwContentsIoThreadClient -----------------------------------------------

// static
// Wrap an optional |JavaObjectWeakGlobalRef| to a Java
// |AwContentsIoThreadClient| in a native |AwContentsIoThreadClient| by getting
// a scoped local reference. This will return |nullptr| if either the optional
// is empty or the weak reference has already expired.
std::unique_ptr<AwContentsIoThreadClient> WrapOptionalWeakRef(
    std::optional<JavaObjectWeakGlobalRef> opt_delegate_weak_ref) {
  if (opt_delegate_weak_ref) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> java_delegate = opt_delegate_weak_ref->get(env);
    if (java_delegate) {
      return std::make_unique<AwContentsIoThreadClient>(java_delegate);
    }
  }
  return nullptr;
}

// static
std::unique_ptr<AwContentsIoThreadClient> AwContentsIoThreadClient::FromToken(
    const content::GlobalRenderFrameHostToken& global_frame_token) {
  return WrapOptionalWeakRef(
      RfhToIoThreadClientMap::GetInstance()->Get(global_frame_token));
}

std::unique_ptr<AwContentsIoThreadClient> AwContentsIoThreadClient::FromID(
    content::FrameTreeNodeId frame_tree_node_id) {
  return WrapOptionalWeakRef(
      RfhToIoThreadClientMap::GetInstance()->Get(frame_tree_node_id));
}

// static
std::unique_ptr<AwContentsIoThreadClient> AwContentsIoThreadClient::FromKey(
    WebContentsKey key) {
  return WrapOptionalWeakRef(
      WebContentsToIoThreadClientMap::GetInstance()->Get(key));
}

// static
void AwContentsIoThreadClient::SubFrameCreated(
    int child_id,
    const blink::LocalFrameToken& parent_frame_token,
    const blink::LocalFrameToken& child_frame_token) {
  RfhToIoThreadClientMap* map = RfhToIoThreadClientMap::GetInstance();
  std::optional<JavaObjectWeakGlobalRef> opt_delegate_weak_ref = map->Get(
      content::GlobalRenderFrameHostToken(child_id, parent_frame_token));
  if (opt_delegate_weak_ref) {
    map->Set(content::GlobalRenderFrameHostToken(child_id, child_frame_token),
             opt_delegate_weak_ref.value());
  } else {
    // It is possible to not find a mapping for the parent rfh_id if the WebView
    // is in the process of being destroyed, and the mapping has already been
    // erased.
    LOG(WARNING) << "No IoThreadClient associated with parent RenderFrameHost.";
  }
}

// static
void AwContentsIoThreadClient::Associate(WebContents* web_contents,
                                         const JavaRef<jobject>& jclient) {
  JNIEnv* env = AttachCurrentThread();
  // The ClientMapEntryUpdater lifespan is tied to the WebContents.
  new ClientMapEntryUpdater(env, web_contents, jclient);
}

AwContentsIoThreadClient::AwContentsIoThreadClient(const JavaRef<jobject>& obj)
    : java_object_(obj) {
  DCHECK(java_object_);
}

AwContentsIoThreadClient::~AwContentsIoThreadClient() = default;

AwContentsIoThreadClient::CacheMode AwContentsIoThreadClient::GetCacheMode()
    const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  JNIEnv* env = AttachCurrentThread();
  return static_cast<AwContentsIoThreadClient::CacheMode>(
      Java_AwContentsIoThreadClient_getCacheMode(env, java_object_));
}


namespace {

AwContentsIoThreadClient::InterceptResponseData NoInterceptRequest() {
  return AwContentsIoThreadClient::InterceptResponseData();
}

void OnShouldInterceptCallback(
    const base::TimeTicks request_started,
    AwContentsIoThreadClient::ShouldInterceptRequestResponseCallback callback,
    AwWebResourceInterceptResponse java_response) {
  JNIEnv* env = AttachCurrentThread();
  bool has_response = java_response.HasResponse(env);
  UMA_HISTOGRAM_BOOLEAN(
      "Android.WebView.ShouldInterceptRequest.IsRequestIntercepted",
      has_response);
  AwContentsIoThreadClient::InterceptResponseData response_data;
  // Grab the input stream now as an optimization to avoid thread hopping later.
  if (base::FeatureList::IsEnabled(
          embedder_support::features::kInputStreamOptimizations) &&
      has_response) {
    auto response = java_response.GetResponse(env);
    if (response->HasInputStream(env)) {
      // Only transfer the input stream if it exists since
      // GetInputStream() can only be called once, even for null input
      // streams.
      response_data.input_stream = response->GetInputStream(env);
    }
  }
  response_data.response =
      std::make_unique<AwWebResourceInterceptResponse>(java_response);

  base::UmaHistogramTimes(
      "Android.WebView.ShouldInterceptRequest.InterceptDuration",
      base::TimeTicks::Now() - request_started);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(response_data)));
}

void StartShouldInterceptRequest(
    AwWebResourceRequest request,
    const base::TimeTicks request_started,
    AwContentsIoThreadClient::ShouldInterceptRequestResponseCallback callback,
    JavaObjectWeakGlobalRef ref) {
  // Historically this method was called `RunShouldInterceptRequest` and so we
  // keep this here to preserve trace comparisons across different milestones
  // and versions.
  TRACE_EVENT0("android_webview", "RunShouldInterceptRequest");
  // The app may perform blocking calls as part of synchronous
  // shouldInterceptRequest, so mark the rest of this scope as possibly
  // blocking. This will ensure that the thread pool is expanded to avoid it
  // being exhausted if all the threads end up waiting at the same time.
  // See https://crbug.com/404563944 for an example of this happening.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = ref.get(env);
  if (!obj) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), NoInterceptRequest()));
    return;
  }

  devtools_instrumentation::ScopedEmbedderCallbackTask embedder_callback(
      "shouldInterceptRequest");
  Java_ShouldInterceptRequestMediator_shouldInterceptRequestFromNative(
      env, obj, request,
      base::android::ToJniCallback(
          env, base::BindOnce(&OnShouldInterceptCallback, request_started,
                              std::move(callback))));
}

// Utility class to increment the delta with the time taken by the rest of the
// current scope.
class CallTimer {
 public:
  explicit CallTimer(base::TimeDelta& counter)
      : counter_(counter), start_(base::TimeTicks::Now()) {}

  // Destructor increments the timedelta with the elapsed time.
  ~CallTimer() { *counter_ += base::TimeTicks::Now() - start_; }

 private:
  raw_ref<base::TimeDelta> counter_;
  base::TimeTicks start_;
};

}  // namespace

AwContentsIoThreadClient::InterceptResponseData::InterceptResponseData() =
    default;
AwContentsIoThreadClient::InterceptResponseData::~InterceptResponseData() =
    default;
AwContentsIoThreadClient::InterceptResponseData::InterceptResponseData(
    InterceptResponseData&& other) = default;
AwContentsIoThreadClient::InterceptResponseData&
AwContentsIoThreadClient::InterceptResponseData::operator=(
    InterceptResponseData&& other) = default;

void AwContentsIoThreadClient::ShouldInterceptRequestAsync(
    AwWebResourceRequest request,
    ShouldInterceptRequestResponseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> mediator =
      Java_AwContentsIoThreadClient_getShouldInterceptRequestMediator(
          env, java_object_, request.url);

  UMA_HISTOGRAM_BOOLEAN(
      "Android.WebView.ShouldInterceptRequest.IsRequestSkipped", !mediator);

  if (mediator) {
    const base::TimeTicks request_started = base::TimeTicks::Now();
    // The mediator is kept alive on the Java side.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&StartShouldInterceptRequest, std::move(request),
                       request_started, std::move(callback),
                       JavaObjectWeakGlobalRef(env, mediator)));
  } else {
    Java_AwContentsIoThreadClient_onLoadResource(env, java_object_,
                                                 request.url);
    // We are already on the IOThread. Just call the callback directly here.
    std::move(callback).Run(NoInterceptRequest());
  }
}

bool AwContentsIoThreadClient::ShouldBlockContentUrls(
    base::TimeDelta& counter) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CallTimer timer(counter);
  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockContentUrls(env,
                                                              java_object_);
}

bool AwContentsIoThreadClient::ShouldBlockFileUrls(
    base::TimeDelta& counter) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CallTimer timer(counter);
  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockFileUrls(env, java_object_);
}

bool AwContentsIoThreadClient::ShouldBlockSpecialFileUrls(
    base::TimeDelta& counter) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CallTimer timer(counter);
  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockSpecialFileUrls(env,
                                                                  java_object_);
}

bool AwContentsIoThreadClient::ShouldAcceptCookies(
    base::TimeDelta& counter) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CallTimer timer(counter);
  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldAcceptCookies(env, java_object_);
}

bool AwContentsIoThreadClient::ShouldAcceptThirdPartyCookies(
    base::TimeDelta& counter) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CallTimer timer(counter);
  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldAcceptThirdPartyCookies(
      env, java_object_);
}

bool AwContentsIoThreadClient::GetSafeBrowsingEnabled() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_getSafeBrowsingEnabled(env,
                                                              java_object_);
}

bool AwContentsIoThreadClient::ShouldBlockNetworkLoads(
    base::TimeDelta& counter) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CallTimer timer(counter);
  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldBlockNetworkLoads(env,
                                                               java_object_);
}

bool AwContentsIoThreadClient::ShouldIncludeCookiesOnIntercept(
    base::TimeDelta& counter) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CallTimer timer(counter);
  JNIEnv* env = AttachCurrentThread();
  return Java_AwContentsIoThreadClient_shouldIncludeCookiesInIntercept(
      env, java_object_);
}

}  // namespace android_webview
