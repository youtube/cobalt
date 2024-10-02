// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_TRANSLATE_AGENT_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_TRANSLATE_AGENT_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace blink {
class WebLocalFrame;
}

namespace translate {

// This class deals with page translation.
// There is one TranslateAgent per RenderView.
//
// Note: this class only supports translation of the main frame. See
// PerFrameTranslateAgent for sub frame translation support.
class TranslateAgent : public content::RenderFrameObserver,
                       public mojom::TranslateAgent {
 public:
  TranslateAgent(content::RenderFrame* render_frame, int world_id);

  TranslateAgent(const TranslateAgent&) = delete;
  TranslateAgent& operator=(const TranslateAgent&) = delete;

  ~TranslateAgent() override;

  // content::RenderFrameObserver implementation.
  void WasShown() override;

  // Informs us that the page's text has been extracted.
  void PageCaptured(const std::u16string& contents);

  // Lets the translation system know that we are preparing to navigate to
  // the specified URL. If there is anything that can or should be done before
  // this URL loads, this is the time to prepare for it.
  void PrepareForUrl(const GURL& url);

  // Under kRetryLanguageDetection, this is true if a previous call to
  // PageCaptured has been made with captured page content and language
  // detection was run.
  bool WasPageContentCapturedForUrl() { return page_contents_length_; }

  // mojom::TranslateAgent implementation.
  void GetWebLanguageDetectionDetails(
      GetWebLanguageDetectionDetailsCallback callback) override;
  void TranslateFrame(const std::string& translate_script,
                      const std::string& source_lang,
                      const std::string& target_lang,
                      TranslateFrameCallback callback) override;
  void RevertTranslation() override;

  // Set the language detection model for used by |this|. For testing only.
  void SeedLanguageDetectionModelForTesting(base::File model_file);

 protected:
  // Returns true if the translate library is available, meaning the JavaScript
  // has already been injected in that page.
  virtual bool IsTranslateLibAvailable();

  // Returns true if the translate library has been initialized successfully.
  virtual bool IsTranslateLibReady();

  // Returns true if the translation script has finished translating the page.
  virtual bool HasTranslationFinished();

  // Returns true if the translation script has reported an error performing the
  // translation.
  virtual bool HasTranslationFailed();

  // Returns the error code generated in translate library.
  virtual int64_t GetErrorCode();

  // Starts the translation by calling the translate library.  This method
  // should only be called when the translate script has been injected in the
  // page.  Returns false if the call failed immediately.
  virtual bool StartTranslation();

  // Asks the Translate element in the page what the language of the page is.
  // Can only be called if a translation has happened and was successful.
  // Returns the language code on success, an empty string on failure.
  virtual std::string GetPageSourceLanguage();

  // Adjusts a delay time for a posted task. This is overridden in tests to do
  // tasks immediately by returning 0.
  virtual base::TimeDelta AdjustDelay(int delay_in_milliseconds);

  // Executes the JavaScript code in |script| in the main frame of RenderView.
  virtual void ExecuteScript(const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of RenderView,
  // and returns the boolean returned by the script evaluation if the script was
  // run successfully. Otherwise, returns |fallback| value.
  virtual bool ExecuteScriptAndGetBoolResult(const std::string& script,
                                             bool fallback);

  // Executes the JavaScript code in |script| in the main frame of RenderView,
  // and returns the string returned by the script evaluation if the script was
  // run successfully. Otherwise, returns empty string.
  virtual std::string ExecuteScriptAndGetStringResult(
      const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of RenderView.
  // and returns the number returned by the script evaluation if the script was
  // run successfully. Otherwise, returns 0.0.
  virtual double ExecuteScriptAndGetDoubleResult(const std::string& script);

  // Executes the JavaScript code in |script| in the main frame of RenderView.
  // and returns the integer value returned by the script evaluation if the
  // script was run successfully. Otherwise, returns 0.
  virtual int64_t ExecuteScriptAndGetIntegerResult(const std::string& script);

 private:
  FRIEND_TEST_ALL_PREFIXES(TranslateAgentTest, TestBuildTranslationScript);

  // Converts language code to the one used in server supporting list.
  static void ConvertLanguageCodeSynonym(std::string* code);

  // Builds the translation JS used to translate from source_lang to
  // target_lang.
  static std::string BuildTranslationScript(const std::string& source_lang,
                                            const std::string& target_lang);

  const mojo::Remote<mojom::ContentTranslateDriver>& GetTranslateHandler();

  // Cleanups all states and pending callbacks associated with the current
  // running page translation.
  void ResetPage();

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Cancels any translation that is currently being performed.  This does not
  // revert existing translations.
  void CancelPendingTranslation();

  // Checks if the current running page translation is finished or errored and
  // notifies the browser accordingly.  If the translation has not terminated,
  // posts a task to check again later.
  void CheckTranslateStatus();

  // Called by TranslatePage to do the actual translation.  |count| is used to
  // limit the number of retries.
  void TranslatePageImpl(int count);

  // Sends a message to the browser to notify it that the translation failed
  // with |error|.
  void NotifyBrowserTranslationFailed(TranslateErrors error);

  // Convenience method to access the main frame.  Can return nullptr, typically
  // if the page is being closed.
  blink::WebLocalFrame* GetMainFrame();

  // Called by the translate host when a new language detection model file
  // has been loaded and is available.
  void UpdateLanguageDetectionModel(base::File model_file);

  // The states associated with the current translation.
  TranslateFrameCallback translate_callback_pending_;
  std::string source_lang_;
  std::string target_lang_;

  // Time when a page language is determined. This is used to know a duration
  // time from showing infobar to requesting translation.
  base::TimeTicks language_determined_time_;

  // The world ID to use for script execution.
  int world_id_;

  // The page content length at language detection time. Recorded to UMA when a
  // user translates the page.
  size_t page_contents_length_ = 0;

  // The task runner responsible for the translation task, freezing it
  // when the frame is backgrounded.
  scoped_refptr<base::SingleThreadTaskRunner> translate_task_runner_;

  // Whether the render frame observed by |this| was initially hidden and
  // the request for a model is delayed until the frame is in the foreground.
  bool waiting_for_first_foreground_ = false;

  // The Mojo pipe for communication with the browser process. Due to a
  // refactor, the other end of the pipe is now attached to a
  // LanguageDetectionTabHelper (which implements the ContentTranslateDriver
  // Mojo interface).
  mojo::Remote<mojom::ContentTranslateDriver> translate_handler_;

  mojo::Receiver<mojom::TranslateAgent> receiver_{this};

  // Method factory used to make calls to TranslatePageImpl.
  base::WeakPtrFactory<TranslateAgent> weak_method_factory_{this};

  // Weak pointer factory used to provide references to the translate host.
  base::WeakPtrFactory<TranslateAgent> weak_pointer_factory_{this};
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_TRANSLATE_AGENT_H_
