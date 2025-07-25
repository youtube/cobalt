// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/emoji/emoji_page_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/storage_partition.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/ime/ash/ime_bridge.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

constexpr char kEmojiPickerToastId[] = "emoji_picker_toast";

// Keep in sync with entry in enums.xml.
enum class EmojiVariantType {
  // smaller entries only used by Chrome OS VK
  kEmojiPickerBase = 4,
  kEmojiPickerVariant = 5,
  kMaxValue = kEmojiPickerVariant,
};

void LogInsertEmoji(bool is_variant, int16_t search_length) {
  EmojiVariantType insert_value = is_variant
                                      ? EmojiVariantType::kEmojiPickerVariant
                                      : EmojiVariantType::kEmojiPickerBase;
  base::UmaHistogramEnumeration("InputMethod.SystemEmojiPicker.TriggerType",
                                insert_value);
  base::UmaHistogramCounts100("InputMethod.SystemEmojiPicker.SearchLength",
                              search_length);
}

void LogInsertEmojiDelay(base::TimeDelta delay) {
  base::UmaHistogramMediumTimes("InputMethod.SystemEmojiPicker.Delay", delay);
}

void LogLoadTime(base::TimeDelta delay) {
  base::UmaHistogramMediumTimes("InputMethod.SystemEmojiPicker.LoadTime",
                                delay);
}

void CopyEmojiToClipboard(const std::string& emoji_to_copy) {
  if (base::FeatureList::IsEnabled(features::kImeSystemEmojiPickerClipboard)) {
    auto clipboard = std::make_unique<ui::ScopedClipboardWriter>(
        ui::ClipboardBuffer::kCopyPaste, nullptr);
    clipboard->WriteText(base::UTF8ToUTF16(emoji_to_copy));
  }
}

std::string BuildGifHTML(const GURL& gif) {
  // Referrer-Policy is used to prevent Tenor from getting information about
  // where the GIFs are being used.
  return base::StrCat(
      {"<img src=\"", gif.spec(), "\" referrerpolicy=\"no-referrer\">"});
}

void CopyGifToClipboard(const GURL& gif_to_copy) {
  if (!gif_to_copy.is_valid()) {
    return;
  }

  // Overwrite the clipboard data with the GIF url.
  auto clipboard = std::make_unique<ui::ScopedClipboardWriter>(
      ui::ClipboardBuffer::kCopyPaste);

  clipboard->WriteHTML(base::UTF8ToUTF16(BuildGifHTML(gif_to_copy)), "",
                       ui::ClipboardContentType::kSanitized);

  // Show a toast that says "GIF not supported. Copied to clipboard.".
  ToastManager::Get()->Show(ToastData(
      kEmojiPickerToastId, ToastCatalogName::kCopyGifToClipboardAction,
      l10n_util::GetStringUTF16(IDS_ASH_EMOJI_PICKER_COPY_GIF_TO_CLIPBOARD)));
}

// Used to insert a gif / emoji after WebUI handler is destroyed, before
// self-constructing.
class InsertObserver : public ui::InputMethodObserver {
 public:
  explicit InsertObserver(ui::InputMethod* ime) : ime_(ime) {
    delete_timer_.Start(
        FROM_HERE, base::Seconds(1),
        base::BindOnce(&InsertObserver::DestroySelf, base::Unretained(this)));
  }

  ~InsertObserver() override { ime_->RemoveObserver(this); }

  virtual void PerformInsert(ui::TextInputClient* input_client) = 0;

  virtual void PerformCopy() = 0;

  void OnTextInputStateChanged(const ui::TextInputClient* client) override {
    focus_change_count_++;
    // At least 2 focus changes - 1 for loss of focus in emoji picker, second
    // for focusing in the new text field.
    // And in lacros, we may expect third change to correct text input type (
    // from initial value to actual correct value).
    // You would expect this to fail if the emoji picker window does not have
    // focus in the text field, but waiting for at least 2 focus changes is
    // still correct behavior.

    if (focus_change_count_ >= 2) {
      // Need to get the client via the IME as InsertText is non-const.
      // Can't use this->ime_ either as it may not be active, want to ensure
      // that we get the active IME.
      ui::InputMethod* input_method =
          IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();

      if (!input_method) {
        return;
      }
      ui::TextInputClient* input_client = input_method->GetTextInputClient();

      if (!input_client) {
        return;
      }
      if (input_client->GetTextInputType() ==
          ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
        // In some clients (e.g. Sheets), there is an extra focus before the
        // "real" text input field.
        focus_change_count_--;
        return;
      }

      PerformInsert(input_client);
      if (this->inserted_) {
        DestroySelf();
      }
      return;
    }
  }
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override {}
  void OnInputMethodDestroyed(const ui::InputMethod* client) override {}

 protected:
  void MarkInserted() { this->inserted_ = true; }

 private:
  void DestroySelf() {
    if (!inserted_) {
      PerformCopy();
    }
    delete this;
  }
  int focus_change_count_ = 0;
  base::OneShotTimer delete_timer_;
  raw_ptr<ui::InputMethod, LeakedDanglingUntriaged | ExperimentalAsh> ime_;
  bool inserted_ = false;
};

// Used to insert an emoji after WebUI handler is destroyed, before
// self-destructing.
class EmojiObserver : public InsertObserver {
 public:
  explicit EmojiObserver(const std::string& emoji_to_insert,
                         ui::InputMethod* ime)
      : InsertObserver(ime), emoji_to_insert_(emoji_to_insert) {}

  void PerformInsert(ui::TextInputClient* input_client) override {
    if (input_client->GetTextInputType() ==
        ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
      // In some clients (e.g. Sheets), there is an extra focus before the
      // "real" text input field. so we skip this insertion.
      return;
    }
    input_client->InsertText(
        base::UTF8ToUTF16(emoji_to_insert_),
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
    MarkInserted();
  }

  void PerformCopy() override { CopyEmojiToClipboard(emoji_to_insert_); }

 private:
  std::string emoji_to_insert_;
};

// Used to insert a gif after WebUI handler is destroyed, before
// self-destructing.
class GifObserver : public InsertObserver {
 public:
  explicit GifObserver(const GURL& gif_to_insert, ui::InputMethod* ime)
      : InsertObserver(ime), gif_to_insert_(gif_to_insert) {}

  void PerformInsert(ui::TextInputClient* input_client) override {
    if (input_client->CanInsertImage()) {
      input_client->InsertImage(gif_to_insert_);
      MarkInserted();
    }
  }

  void PerformCopy() override { CopyGifToClipboard(gif_to_insert_); }

 private:
  GURL gif_to_insert_;
};

EmojiPageHandler::EmojiPageHandler(
    mojo::PendingReceiver<emoji_picker::mojom::PageHandler> receiver,
    content::WebUI* web_ui,
    EmojiUI* webui_controller,
    bool incognito_mode,
    bool no_text_field)
    : receiver_(this, std::move(receiver)),
      webui_controller_(webui_controller),
      incognito_mode_(incognito_mode),
      no_text_field_(no_text_field) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // There are two conditions to control the GIF support:
  //   1. Feature flag is turned on.
  //   2. For managed users, the policy is turned on.
  gif_support_enabled_ =
      base::FeatureList::IsEnabled(features::kImeSystemEmojiPickerGIFSupport) &&
      (profile->GetPrefs()->IsManagedPreference(
           prefs::kEmojiPickerGifSupportEnabled)
           ? profile->GetPrefs()->GetBoolean(
                 prefs::kEmojiPickerGifSupportEnabled)
           : true);

  url_loader_factory_ = profile->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess();
}

EmojiPageHandler::~EmojiPageHandler() {}

void EmojiPageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  // Embedder may not exist in some cases (e.g. user browses to
  // chrome://emoji-picker directly rather than using right click on
  // text field -> emoji).
  if (embedder) {
    embedder->ShowUI();
  }
  shown_time_ = base::TimeTicks::Now();
}

void EmojiPageHandler::IsIncognitoTextField(
    IsIncognitoTextFieldCallback callback) {
  std::move(callback).Run(incognito_mode_);
}

void EmojiPageHandler::GetFeatureList(GetFeatureListCallback callback) {
  std::vector<emoji_picker::mojom::Feature> enabled_features;
  if (base::FeatureList::IsEnabled(
          features::kImeSystemEmojiPickerSearchExtension)) {
    enabled_features.push_back(
        emoji_picker::mojom::Feature::EMOJI_PICKER_SEARCH_EXTENSION);
  }
  if (gif_support_enabled_) {
    enabled_features.push_back(
        emoji_picker::mojom::Feature::EMOJI_PICKER_GIF_SUPPORT);
  }
  if (base::FeatureList::IsEnabled(
          features::kImeSystemEmojiPickerJellySupport)) {
    enabled_features.push_back(
        emoji_picker::mojom::Feature::EMOJI_PICKER_JELLY_SUPPORT);
  }

  std::move(callback).Run(enabled_features);
}

void EmojiPageHandler::GetCategories(GetCategoriesCallback callback) {
  gif_tenor_api_fetcher_.FetchCategories(std::move(callback),
                                         url_loader_factory_);
}

void EmojiPageHandler::GetFeaturedGifs(const absl::optional<std::string>& pos,
                                       GetFeaturedGifsCallback callback) {
  gif_tenor_api_fetcher_.FetchFeaturedGifs(std::move(callback),
                                           url_loader_factory_, pos);
}

void EmojiPageHandler::SearchGifs(const std::string& query,
                                  const absl::optional<std::string>& pos,
                                  SearchGifsCallback callback) {
  gif_tenor_api_fetcher_.FetchGifSearch(std::move(callback),
                                        url_loader_factory_, query, pos);
}

void EmojiPageHandler::GetGifsByIds(const std::vector<std::string>& ids,
                                    GetGifsByIdsCallback callback) {
  gif_tenor_api_fetcher_.FetchGifsByIds(std::move(callback),
                                        url_loader_factory_, ids);
}

void EmojiPageHandler::InsertEmoji(const std::string& emoji_to_insert,
                                   bool is_variant,
                                   int16_t search_length) {
  LogInsertEmoji(is_variant, search_length);
  LogInsertEmojiDelay(base::TimeTicks::Now() - shown_time_);
  // In theory, we are returning focus to the input field where the user
  // originally selected emoji. However, the input field may not exist anymore
  // e.g. JS has mutated the web page while emoji picker was open, so check
  // that a valid input client is available as part of inserting the emoji.
  ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();
  if (!input_method) {
    DLOG(WARNING) << "no input_method found";
    CopyEmojiToClipboard(emoji_to_insert);
    return;
  }
  if (no_text_field_) {
    CopyEmojiToClipboard(emoji_to_insert);
    return;
  }

  // It seems like this might leak EmojiObserver, but the EmojiObserver
  // destroys itself on a timer (complex behavior needed since the observer
  // needs to outlive the page handler)
  input_method->AddObserver(new EmojiObserver(emoji_to_insert, input_method));

  // By hiding the emoji picker, we restore focus to the original text field
  // so we can insert the text.
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }
}

void EmojiPageHandler::InsertGif(const GURL& gif) {
  if (!gif.is_valid()) {
    return;
  }

  ui::InputMethod* input_method =
      IMEBridge::Get()->GetInputContextHandler()->GetInputMethod();

  if (!input_method) {
    DLOG(WARNING) << "no input_method found";
    CopyGifToClipboard(gif);
    return;
  }

  if (no_text_field_) {
    CopyGifToClipboard(gif);
    return;
  }

  // The GifObserver here will self-destroy.
  input_method->AddObserver(new GifObserver(gif, input_method));

  // By hiding the emoji picker, we restore focus to the original text field.
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }
}

void EmojiPageHandler::OnUiFullyLoaded() {
  LogLoadTime(base::TimeTicks::Now() - shown_time_);
}

}  // namespace ash
