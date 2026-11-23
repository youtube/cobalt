// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"

#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/profiles/profile.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

ContextHubPageHandler::ContextHubPageHandler(
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> receiver,
    Profile* profile,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      web_contents_(web_contents) {}

ContextHubPageHandler::~ContextHubPageHandler() = default;

void ContextHubPageHandler::GenerateAutoTodos(
    GenerateAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  service->GenerateAutoTodos(
      base::BindOnce(&ContextHubPageHandler::OnAutoTodosGenerated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextHubPageHandler::OnAutoTodosGenerated(
    GenerateAutoTodosCallback callback,
    std::optional<personal_context::proto::AutoTodosResponse> result) {
  std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>
      mojo_todos;
  if (result.has_value() && result.value().todos_size() > 0) {
    mojo_todos.emplace();
    for (const personal_context::proto::AutoTodoItem& todo :
         result.value().todos()) {
      browser::context_hub::mojom::AutoTodoItemPtr mojo_todo =
          browser::context_hub::mojom::AutoTodoItem::New();
      mojo_todo->title = todo.title();
      mojo_todo->description = todo.description();
      for (const personal_context::proto::SourceReference& ref :
           todo.source_references()) {
        if (ref.has_gmail()) {
          browser::context_hub::mojom::GmailReferencePtr gmail =
              browser::context_hub::mojom::GmailReference::New();
          gmail->message_url = GURL(ref.gmail().message_url());
          mojo_todo->source_references.push_back(
              browser::context_hub::mojom::SourceReference::NewGmail(
                  std::move(gmail)));
        } else if (ref.has_photos()) {
          browser::context_hub::mojom::PhotosReferencePtr photos =
              browser::context_hub::mojom::PhotosReference::New();
          photos->photos_url = GURL(ref.photos().photos_url());
          mojo_todo->source_references.push_back(
              browser::context_hub::mojom::SourceReference::NewPhotos(
                  std::move(photos)));
        }
      }
      mojo_todos->push_back(std::move(mojo_todo));
    }
  }
  std::move(callback).Run(std::move(mojo_todos));
}

void ContextHubPageHandler::GetAllEntries(GetAllEntriesCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->GetAllEntries(base::BindOnce(
      [](GetAllEntriesCallback callback,
         std::vector<context_hub::MemoryBankEntry> entries) {
        std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>
            mojo_entries;
        for (const auto& entry : entries) {
          auto mojo_entry = browser::context_hub::mojom::MemoryBankEntry::New();
          mojo_entry->id = entry.id;
          switch (entry.type) {
            case context_hub::MemoryBankType::kTab:
              mojo_entry->type = browser::context_hub::mojom::EntryType::kTab;
              break;
            case context_hub::MemoryBankType::kTextSelection:
              mojo_entry->type =
                  browser::context_hub::mojom::EntryType::kTextSelection;
              break;
          }
          mojo_entry->timestamp = entry.timestamp;
          mojo_entry->url = entry.url;
          mojo_entry->tab_title = entry.tab_title;
          mojo_entry->selected_text = entry.selected_text;
          mojo_entry->tags = entry.tags;
          mojo_entries.push_back(std::move(mojo_entry));
        }
        std::move(callback).Run(std::move(mojo_entries));
      },
      std::move(callback)));
}
